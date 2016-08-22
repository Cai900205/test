
// syslk_clientDlg.h : header file
//

#pragma once

#include "DlgProgress.h"
// Csyslk_clientDlg dialog
class Csyslk_clientDlg : public CDialogEx
{
// Construction
public:
	Csyslk_clientDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_SYSLK_CLIENT_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
protected:
	afx_msg LRESULT OnCmiUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCmiProgress(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCmiSnapshot(WPARAM wParam, LPARAM lParam);
public:
	afx_msg void OnBnClickedBtnInquiry();
	afx_msg void OnBnClickedBtnGetfl();
	CListCtrl m_lstFilelist;
	afx_msg void OnBnClickedBtnRec();
	afx_msg void OnBnClickedBtnFormat();
	afx_msg void OnBnClickedBtnDownload();
private:
	CDlgProgress m_dlgProgress;
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
//	CButton m_btnDownload;
	afx_msg void OnBnClickedBtnDelete();
	afx_msg void OnBnClickedBtnSynctime();
	afx_msg void OnBnClickedBtnPlayback();
	afx_msg void OnBnClickedBtnAutofresh();
	afx_msg void OnBnClickedBtnUpload();
	afx_msg void OnBnClickedBtnSysdown();
	afx_msg void OnBnClickedBtnUpgrade();
};
