
// pbbDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "pbb.h"
#include "pbbDlg.h"
#include "afxdialogex.h"
#include <boost/locale.hpp>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框
#define WM_ASYNC_MSG	WM_USER + 10
#define WM_ASYNC_PROC	WM_USER + 20
#define WM_ASYNC_ENABLE	WM_USER + 30

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CpbbDlg 对话框

CpbbDlg::CpbbDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_PBB_DIALOG, pParent),
	_io_service(),
	_work_thread(_io_service)
	, _info(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CpbbDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_INFO, _info);
}

BEGIN_MESSAGE_MAP(CpbbDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_RUN, &CpbbDlg::OnBnClickedBtnRun)
	ON_BN_CLICKED(IDC_BTN_OPEN, &CpbbDlg::OnBnClickedBtnOpen)
	ON_WM_DESTROY()
	ON_MESSAGE(WM_ASYNC_MSG,&CpbbDlg::onPassMsg)
	ON_MESSAGE(WM_ASYNC_PROC, &CpbbDlg::onProcess)
	ON_MESSAGE(WM_ASYNC_ENABLE, &CpbbDlg::onEnable)
END_MESSAGE_MAP()


// CpbbDlg 消息处理程序

BOOL CpbbDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	((CButton *)GetDlgItem(IDC_RADIO_GPU))->SetCheck(TRUE);

	((CProgressCtrl*)GetDlgItem(IDC_PROGRESS1))->SetRange(0, 100);
	((CProgressCtrl*)GetDlgItem(IDC_PROGRESS1))->SetPos(0);
	// 绑定
	_work_thread.sig_start.connect(
		boost::bind(&CpbbDlg::onWorkStart, this)
	);
	_work_thread.sig_end.connect(
		boost::bind(&CpbbDlg::onWorkEnd, this)
	);

	_work_thread.sig_message.connect(
		boost::bind(&CpbbDlg::onWorkMsg, this, _1)
	);
	_work_thread.sig_process.connect(
		boost::bind(&CpbbDlg::onWorkProcess, this, _1)
	);

	// 启动工作线程
	_work_th = std::thread([&] {
		boost::asio::io_service::work work(_io_service);
		_io_service.run(); 
	});

	_work_th.detach();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CpbbDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CpbbDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CpbbDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CpbbDlg::OnBnClickedBtnRun()
{
	// 获取key
	CString strTargetPath;
	GetDlgItemText(IDC_EDIT_OPEN, strTargetPath);

	if (strTargetPath.IsEmpty()) {
		return;
	}

	// 启动读写线程
	_work_thread.run(strTargetPath.GetBuffer(), ((CButton *)GetDlgItem(IDC_RADIO_GPU))->GetCheck());
}


void CpbbDlg::OnBnClickedBtnOpen()
{
	// 打开文件选择
	TCHAR szFilter[] = _T("鹏保宝文件(*.pbb)|*.pbb|所有文件(*.*)|*.*||");

	CFileDialog fileDlg(TRUE, _T("txt"), NULL, 0, szFilter, this);

	CString strFilePath;

	// 显示打开文件对话框   
	if (IDOK == fileDlg.DoModal())
	{
		// 如果点击了文件对话框上的“打开”按钮，则将选择的文件路径显示到编辑框里   
		strFilePath = fileDlg.GetPathName();
		SetDlgItemText(IDC_EDIT_OPEN, strFilePath);
		native_add_msg(_T(" 打开文件: [") + strFilePath + _T("]"));
	}
	else {
		native_add_msg(_T(" 取消打开文件...."));
	}
}

LRESULT CpbbDlg::onPassMsg(WPARAM, LPARAM)
{
	boost::mutex::scoped_lock lock(_msg_mutex);
	for (auto & msg : _msgs) {
#ifdef _UNICODE
		// auto s = boost::locale::conv::utf_to_utf<wchar_t>(msg);
		native_add_msg(msg.c_str());
#else
		native_add_msg(msg.c_str());
#endif
	}
	_msgs.clear();

	return 0;
}
 
LRESULT CpbbDlg::onProcess(WPARAM wParam, LPARAM lParam)
{
	process((int)wParam);
	return 0;
}

LRESULT CpbbDlg::onEnable(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case 1:
		GetDlgItem(IDC_BTN_RUN)->EnableWindow(FALSE);
		break;
	case 2:
		GetDlgItem(IDC_BTN_RUN)->EnableWindow(TRUE);
		break;
	}
	return 0;
}

void CpbbDlg::add_msg(std::wstring msg)
{
	boost::mutex::scoped_lock lock(_msg_mutex);
	_msgs.push_back(msg);

	PostMessage(WM_ASYNC_MSG, 0, 0);
}

void CpbbDlg::process(int pos)
{
	((CProgressCtrl*)GetDlgItem(IDC_PROGRESS1))->SetPos(pos);
}

void CpbbDlg::native_add_msg(CString msg)
{
	_info = msg + _T("\r\n") + _info;

	UpdateData(FALSE);
}

void CpbbDlg::onWorkStart()
{
	add_msg(_T(" 开始运行..."));
	// disable click
	PostMessage(WM_ASYNC_ENABLE, 1, 0);
}

void CpbbDlg::onWorkEnd()
{
	add_msg(_T(" 运行完成."));
	// enable click
	PostMessage(WM_ASYNC_ENABLE, 2, 0);
}

void CpbbDlg::onWorkMsg(std::wstring str)
{
	add_msg(_T(" ") + str);
}

void CpbbDlg::onWorkProcess(int proc_)
{
// 	std::wstringstream msg;
// 	msg << _T("进度:");
// 	msg << proc_;
// 	onWorkMsg(msg.str());
	PostMessage(WM_ASYNC_PROC, proc_, 0);
}

void CpbbDlg::OnDestroy()
{
	_io_service.stop();
	_io_service.reset();

	CDialogEx::OnDestroy();

	HANDLE handle = GetCurrentProcess();
	TerminateProcess(handle, 0);
}