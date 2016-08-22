// DlgSvrSelect.cpp : implementation file
//

#include "stdafx.h"
#include "syslk_client.h"
#include "DlgSvrSelect.h"
#include "afxdialogex.h"
#include "syslk_clientDlg.h"

// DlgSvrSelect dialog

IMPLEMENT_DYNAMIC(DlgSvrSelect, CDialogEx)

DlgSvrSelect::DlgSvrSelect(CWnd* pParent /*=NULL*/)
	: CDialogEx(DlgSvrSelect::IDD, pParent)
{
	m_server_ip = _T("");
}

DlgSvrSelect::~DlgSvrSelect()
{
}

void DlgSvrSelect::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_IP_SERVER, m_ipServer);
	DDX_Control(pDX, IDC_RAD_TCP, m_radProtocal);
}


BEGIN_MESSAGE_MAP(DlgSvrSelect, CDialogEx)
	ON_BN_CLICKED(IDOK, &DlgSvrSelect::OnBnClickedOk)
END_MESSAGE_MAP()


// DlgSvrSelect message handlers


void DlgSvrSelect::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	BYTE ip[4];
	m_ipServer.GetAddress(ip[0], ip[1], ip[2], ip[3]);
	g_svr_ip.Format(_T("%d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
	CString port;
	GetDlgItemText(IDC_EDT_SVR_PORT, port);
	g_svr_port = atoi(port);
	g_use_tcp = m_radProtocal.GetCheck()?1:0;

	CDialogEx::OnOK();
	Csyslk_clientDlg dlg;
	INT_PTR nResponse = dlg.DoModal();
}

BOOL DlgSvrSelect::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  Add extra initialization here
	DWORD ip = htonl(inet_addr(g_svr_ip));
	m_ipServer.SetAddress(ip);

	CString port;
	port.Format(_T("%d"), g_svr_port);
	SetDlgItemText(IDC_EDT_SVR_PORT, port);
	if (g_use_tcp)
		m_radProtocal.SetCheck(true);
	else 
		m_radProtocal.SetCheck(false);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
