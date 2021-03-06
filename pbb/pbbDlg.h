
// pbbDlg.h : 头文件
//

#pragma once
#include <thread>
#include "WorkThread.h"
#include <boost/thread/mutex.hpp>

// CpbbDlg 对话框
class CpbbDlg : public CDialogEx
{
// 构造
public:
	CpbbDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PBB_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnRun();
	afx_msg void OnBnClickedBtnOpen();

	afx_msg LRESULT onPassMsg(WPARAM, LPARAM);
	afx_msg LRESULT onProcess(WPARAM, LPARAM);
	afx_msg LRESULT onEnable(WPARAM, LPARAM);

private:
	boost::asio::io_service _io_service;
	CWorkThread _work_thread;
	std::thread _work_th;

public:
	void add_msg(std::wstring msg);
	void process(int);
private:
	std::vector<std::wstring> _msgs;
	boost::mutex _msg_mutex;

	void native_add_msg(CString msg);

public:
	// 回调
	void onWorkStart();
	void onWorkEnd();
	void onWorkMsg(std::wstring);
	void onWorkProcess(int);
private:
	CString _info;
public:
	afx_msg void OnDestroy();
};
