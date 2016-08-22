#pragma once


// CDlgProgress dialog

class CDlgProgress : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgProgress)
public:

	CString m_description;
	uint64_t m_total_len;
	uint64_t m_last_offset;
	DWORD	m_start_tick;
public:
	CDlgProgress(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgProgress();

	 void init_progress(uint64_t total_len);

// Dialog Data
	enum { IDD = IDD_DLG_PROGRESS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
public:
	CProgressCtrl m_prgCtrl;
	afx_msg void OnBnClickedCancel();
};
