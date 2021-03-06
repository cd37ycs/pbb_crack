#include "stdafx.h"
#include "WorkThread.h"
#include <boost/scope_exit.hpp>
#include <boost/process.hpp>
#include <boost/process/windows.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/locale.hpp>
#include "runGetInfo.h"
#include "doDec.h"

#undef USE_IPC

#ifdef USE_IPC
#include <boost/filesystem.hpp>
#include <boost/interprocess/file_mapping.hpp>  
#include <boost/interprocess/mapped_region.hpp>
#endif

CWorkThread::CWorkThread(boost::asio::io_service &io_service_):
	_io_service(io_service_)
{
}

CWorkThread::~CWorkThread()
{
}

void CWorkThread::run(std::wstring path, bool isGPU, std::string key)
{
	_io_service.post(
		boost::bind(&CWorkThread::async_run,
			this,
			path, isGPU, key)
	);
}

void CWorkThread::async_run(std::wstring path, bool isGPU, std::string key)
{
	sig_start();

	BOOST_SCOPE_EXIT(this_){
		this_->sig_end();
	}BOOST_SCOPE_EXIT_END

	// 开始
#ifdef USE_IPC
	try {
		using namespace boost::interprocess;
		boost::filesystem::path lpath(path);
		file_mapping file(lpath.string().c_str(), read_only);
		mapped_region region(file, read_only, 0);

		unsigned char *addr = static_cast<unsigned char *>(region.get_address());
		std::size_t size = region.get_size();

#else
	FILE *stream = 0;
	errno_t err = _wfopen_s(&stream, path.c_str(), L"rb+");

	if (err || stream == nullptr) {
		std::wstringstream msg;
		msg << _T("文件打开失败: ");
		msg << path;
		msg << _T(" ");
		msg << err;
		sig_message(msg.str());
		return;
	}
	unsigned char addr[0x3d4] = { 0 };
	std::size_t size = 0x3d4;
	fseek(stream, -0x3d4, SEEK_END);
	fread(addr, 1, 0x3d4, stream);
	fclose(stream);
#endif
	// 获取key
	pvKey key_ = getKey({ addr, size , path });
	sig_message(_T("文件key:") + key_->toWString());

	// 解密key
	bool rc = false;
	if (isGPU) {
		rc = gpuCalc(key_);
	}
	else if (!key.empty()) {
		rc = true;
	}
	else {
		sig_message(_T("cpu计算中,请耐心等待(大约二十四分钟跑完)..."));
		rc = cpuCalc(key_);
	}

	if (rc == false) {
		std::wstringstream msg;
		msg << _T("错误:计算失败,请选择 ");
		msg << (isGPU ? _T("CPU") : _T("GPU"));
		msg << _T("计算");
		sig_message(msg.str());
		return;
	}

	// 提取文件
	sig_message(_T("文件解密key:") + key_->toWString());

	dump({ addr, size , path }, key_);

	sig_message(_T("恭喜!文件成功提取.:-D"));

#ifdef USE_IPC
	}
	catch (std::exception &e) {
		std::wstring error = boost::locale::conv::utf_to_utf<wchar_t>(e.what());
		sig_message(_T("错误:") + error);
	}

#endif
}

bool CWorkThread::gpuCalc(pvKey key)
{
	namespace bp = boost::process;

	bp::ipstream p;
	bp::child child(
		"getKey",
		bp::windows::hide,
		bp::args = key->toString().c_str(),
		bp::std_out > p
	);

	child.wait();
	std::string s;

	unsigned int r = 0;
	while (std::getline(p, s)) {
		if (boost::algorithm::starts_with(s, "key:")) {
			boost::algorithm::trim_left_if(s, boost::is_any_of("key:"));
			boost::algorithm::trim_right_if(s, boost::is_any_of("\r\n"));

			r = boost::lexical_cast<unsigned int>(s);
			break;
		}
	}

	if (r == 0) return false;

	for (int i = 0; i < 16; i++) {
		r = r * 214013L + 2531011L;
		unsigned char k = (r >> 16) & 0xff;
		key->v[i] = k;
	}
	return true;
}

bool CWorkThread::cpuCalc(pvKey key)
{
	unsigned int r = 0;
	for (unsigned int i = 0; i < 0x6FFFFFFF; i++) {
		bool found = false;
		r = i;
		for (int j = 0; j < 16; j++) {
			r = r * 214013L + 2531011L;

			if (((r >> 16) & 0xff) != key->v[j]) break;

			if (j == 15) found = true;
		}
		if(found == false) continue;

		for (int j = 0; j < 32; j++) {
			r = (r - 2531011) * 3115528533;
		}

		// 通过r 生成 key
		for (int j = 0; j < 16; j++) {
			r = r * 214013L + 2531011L;
			unsigned char k = (r >> 16) & 0xff;
			key->v[j] = k;
		}
	}
	return false;
}

void CWorkThread::dump(fInfo file, pvKey sKey)
{
	for (int i = 0; i < 16; i++) {
		sKey->v[i] += 0x11;
	}

	pvTable vt = doBuild(sKey, false);

	// key
	char buf[0x20] = { 0 };
	memcpy(buf, file.addr + file.size - 0x10C - 0x2A8 - 0x20, 0x20);
	srcInfo info = getSrcInfo((const unsigned char*)buf, 0x20);

	// 写入文件
	boost::filesystem::path path(file.path);
	std::size_t calc_size = boost::filesystem::file_size(path) - 0x10c - 0x2a8 - 0x20 - 0x200000;
	std::size_t rand_size = calc_size % 0x10;

	calc_size -= rand_size;
	std::size_t free_size = calc_size;

	std::wstring rpath = path.stem().wstring();
	sig_message(_T("写入文件中(请耐心等待):") + rpath);
#ifndef USE_IPC
	FILE *stream = 0, *old_stream = 0;
	_wfopen_s(&stream, rpath.c_str() , L"wb+");
	_wfopen_s(&old_stream, path.wstring().c_str(), L"rb");

	fseek(old_stream, 0x200000 + rand_size, SEEK_SET);
	sig_process(1);
	int pos = 1;
	for (std::size_t i = 0; i < calc_size; i += 0x20) {
		fread(buf, 1, 0x20, old_stream);
		doDec((void *)buf, 0x20, vt);
		int n_pos = 100 - (int)((float)free_size / calc_size * 100);
		if (pos < n_pos) {
			pos = n_pos;
			sig_process(pos);
		}
		
		if (free_size < 0x20) {
			fwrite(buf, sizeof(unsigned char), free_size, stream);
		}
		else {
			fwrite(buf, sizeof(unsigned char), 0x20, stream);
		}

		free_size -= 0x20;
	}

	sig_process(100);

	fclose(old_stream);
	fclose(stream);
#else
	std::ofstream fs(rpath.c_str(), std::ios::binary);
	fs.write((const char *)pptr, calc_size);
	fs.close();

	using namespace boost::interprocess;
	file_mapping fm_file(path.stem().string().c_str(), read_write);
	mapped_region region(fm_file, read_write, 0);

	doDec(region.get_address(), region.get_size(), vt);
#endif
}