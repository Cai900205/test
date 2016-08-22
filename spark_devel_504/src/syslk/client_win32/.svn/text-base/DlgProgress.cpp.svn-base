// DlgProgress.cpp : implementation file
//

#include "stdafx.h"
#include "syslk_client.h"
#include "DlgProgress.h"
#include "afxdialogex.h"


// CDlgProgress dialog

IMPLEMENT_DYNAMIC(CDlgProgress, CDialogEx)

CDlgProgress::CDlgProgress(CWnd* pParent /*=NULL*/)
	: CDialogEx(CDlgProgress::IDD, pParent)
{

}

CDlgProgress::~CDlgProgress()
{
}

void CDlgProgress::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PRG_CTRL, m_prgCtrl);
}


BEGIN_MESSAGE_MAP(CDlgProgress, CDialogEx)
	ON_MESSAGE(WM_CMI_PROGRESS, &CDlgProgress::OnUpdateProgress)
	ON_BN_CLICKED(IDCANCEL, &CDlgProgress::OnBnClickedCancel)
END_MESSAGE_MAP()

// CDlgProgress message handlers
afx_msg LRESULT CDlgProgress::OnUpdateProgress(WPARAM wParam, LPARAM lParam)
{
	uint64_t offset = ((uint64_t)wParam | ((uint64_t)lParam<<32));
	static DWORD last_updated = 0;
	static uint64_t last_counter = 0;

	m_last_offset = offset;
	int pos = (int)(((double)(offset)/m_total_len)*100);
	last_counter = offset;
	m_prgCtrl.SetPos(pos);

	DWORD now = GetTickCount();
	if (now - last_updated >= 1) {
		CString str;
		str.Format(_T("[%d%%] %s / %s @ %.3fMbps"), pos, format_number(last_counter), format_number(m_total_len),
					(double)last_counter/1024/1024/((double)(now - m_start_tick)/1000)*8);
		GetDlgItem(IDC_STC_PRGDESC)->SetWindowText(str);
		last_updated = now;
	}

	return(0);
}

void CDlgProgress::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	if (IDYES != MessageBox(_T("是否中断数据传输？"), _T("CONFIRM"), MB_YESNO | MB_ICONQUESTION)) {
		return;
	}
	g_cancel_req = 1;
	CWaitCursor wc;
	while(g_cancel_req) {
		;
	}
	CDialogEx::OnCancel();
}

void CDlgProgress::init_progress(uint64_t total_len)
{
	m_total_len = total_len;
	m_start_tick = GetTickCount();
	m_last_offset = 0;
	PostMessageA(WM_CMI_PROGRESS, 0, 0);
	ShowWindow(SW_SHOW);
}
