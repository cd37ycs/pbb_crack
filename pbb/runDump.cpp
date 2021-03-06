#include "stdafx.h"
#include "runDump.h"
#include "doEnc.h"
#include "Utils.h"
#include <boost/filesystem.hpp>
#include <boost/interprocess/file_mapping.hpp>  
#include <boost/interprocess/mapped_region.hpp>
#include "runGetInfo.h"
#include "doDec.h"

void runDump(fInfo file, std::string key)
{
    // 转换为key形式
    if ( key.length() != 32 ) {
        printf("error: key格式不正确\n");
        return;
    }
    pvKey sKey = utils::str2key(key.c_str());

    for ( int i = 0; i < 16; i++ ) {
        sKey->v[i] += 0x11;
    }

    pvTable vt = doBuild(sKey, false);

    char buf[0x20];
    memcpy(buf, file.addr + file.size - 0x10C - 0x2A8 - 0x20, 0x20);

    srcInfo info = getSrcInfo((const unsigned char*)buf, 0x20);

    std::size_t calc_size = file.size - 0x10c - 0x2a8 - 0x20 - 0x200000;
    std::size_t rand_size = calc_size % 0x10;

    const unsigned char *pptr = file.addr + 0x200000 + rand_size;

    // 写入文件
    boost::filesystem::path path(file.path);

    calc_size -= rand_size;
    std::size_t free_size = calc_size;
#if 1
    FILE *stream = 0;
    fopen_s(&stream, path.stem().string().c_str(),"wb+");

    for ( std::size_t i = 0; i < calc_size; i += 0x20 ) {
        memcpy(buf, &pptr[i], 0x20);

        doDec((void *)buf, 0x20, vt);

        if ( free_size < 0x20 ) {
            fwrite(buf, sizeof(unsigned char), free_size, stream);
        }
        else {
            fwrite(buf, sizeof(unsigned char), 0x20, stream);
        }

        free_size -= 0x20;
    }

    fclose(stream);
#else

    std::string rpath = path.stem().string();
    std::ofstream fs(rpath.c_str(), std::ios::binary);
    fs.write((const char *)pptr, calc_size);

    using namespace boost::interprocess;
    file_mapping fm_file(rpath.c_str(), read_write);
    mapped_region region(fm_file, read_write);

    doDec(region.get_address(), region.get_size(), vt);
#endif
}
