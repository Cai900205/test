#pragma once


// DlgSvrSelect dialog

class DlgSvrSelect : public CDialogEx
{
	DECLARE_DYNAMIC(DlgSvrSelect)

public:
	DlgSvrSelect(CWnd* pParent = NULL);   // standard constructor
	virtual ~DlgSvrSelect();

// Dialog Data
	enum { IDD = IDD_DLG_SVRSELECT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CIPAddressCtrl m_ipServer;
	CButton m_radProtocal;
	CString m_server_ip;
	afx_msg void OnBnClickedOk();
	virtual BOOL OnInitDialog();
};
