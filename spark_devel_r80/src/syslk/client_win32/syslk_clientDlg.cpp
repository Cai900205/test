
// syslk_clientDlg.cpp : implementation file
//

#include "stdafx.h"
#include "syslk_client.h"
#include "syslk_clientDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// Csyslk_clientDlg dialog




Csyslk_clientDlg::Csyslk_clientDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(Csyslk_clientDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void Csyslk_clientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LST_FILELIST, m_lstFilelist);
	//  DDX_Control(pDX, IDC_BTN_DOWNLOAD, m_btnDownload);
}

BEGIN_MESSAGE_MAP(Csyslk_clientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &Csyslk_clientDlg::OnBnClickedOk)
	ON_MESSAGE(WM_CMI_UPDATE, &Csyslk_clientDlg::OnCmiUpdate)
	ON_MESSAGE(WM_CMI_PROGRESS, &Csyslk_clientDlg::OnCmiProgress)
	ON_MESSAGE(WM_CMI_SNAPSHOT, &Csyslk_clientDlg::OnCmiSnapshot)
	ON_BN_CLICKED(IDC_BTN_INQUIRY, &Csyslk_clientDlg::OnBnClickedBtnInquiry)
	ON_BN_CLICKED(IDC_BTN_GETFL, &Csyslk_clientDlg::OnBnClickedBtnGetfl)
	ON_BN_CLICKED(IDC_BTN_REC, &Csyslk_clientDlg::OnBnClickedBtnRec)
	ON_BN_CLICKED(IDC_BTN_FORMAT, &Csyslk_clientDlg::OnBnClickedBtnFormat)
	ON_BN_CLICKED(IDC_BTN_DOWNLOAD, &Csyslk_clientDlg::OnBnClickedBtnDownload)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BTN_DELETE, &Csyslk_clientDlg::OnBnClickedBtnDelete)
	ON_BN_CLICKED(IDC_BTN_SYNCTIME, &Csyslk_clientDlg::OnBnClickedBtnSynctime)
	ON_BN_CLICKED(IDC_BTN_PLAYBACK, &Csyslk_clientDlg::OnBnClickedBtnPlayback)
	ON_BN_CLICKED(IDC_BTN_AUTOFRESH, &Csyslk_clientDlg::OnBnClickedBtnAutofresh)
	ON_BN_CLICKED(IDC_BTN_UPLOAD, &Csyslk_clientDlg::OnBnClickedBtnUpload)
	ON_BN_CLICKED(IDC_BTN_SYSDOWN, &Csyslk_clientDlg::OnBnClickedBtnSysdown)
	ON_BN_CLICKED(IDC_BTN_UPGRADE, &Csyslk_clientDlg::OnBnClickedBtnUpgrade)
END_MESSAGE_MAP()


// Csyslk_clientDlg message handlers

BOOL Csyslk_clientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
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

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	int ret = cmi_init();
	if (ret != 0) {
		this->OnOK();
		return TRUE;
	}

	m_dlgProgress.Create((UINT)IDD_DLG_PROGRESS, this);
	hwndProgress = (HWND)m_dlgProgress;

	hwndMain = (HWND)this->m_hWnd;

    LONG lStyle;
    lStyle = GetWindowLong(m_lstFilelist.m_hWnd, GWL_STYLE);
    lStyle &= ~LVS_TYPEMASK;
    lStyle |= LVS_REPORT;
    SetWindowLong(m_lstFilelist.m_hWnd, GWL_STYLE, lStyle);
 
    DWORD dwStyle = m_lstFilelist.GetExtendedStyle();
    dwStyle |= LVS_EX_FULLROWSELECT;
    dwStyle |= LVS_EX_GRIDLINES;
	dwStyle |= LVS_EX_SUBITEMIMAGES;
	m_lstFilelist.SetExtendedStyle(dwStyle);

	m_lstFilelist.InsertColumn( 0, "Slot", LVCFMT_RIGHT, 40);
	m_lstFilelist.InsertColumn( 1, "File Size", LVCFMT_RIGHT, 110);
	m_lstFilelist.InsertColumn( 2, "Create Time", LVCFMT_RIGHT, 150);

	HANDLE hTest = CreateThread(NULL, 0, cmi_recv_thread, (LPVOID)this->m_hWnd, 0, NULL);

	cmi_cmd_disp_init();

	CString outstr;
	outstr.Format("%s v%s", CLIENT_APP_NAME, CLIENT_VER_STR);
	SetWindowText(outstr);

	//cmi_cmd_disp_inquiry();
	OnBnClickedBtnGetfl();

	((CButton*)GetDlgItem(IDC_BTN_AUTOFRESH))->SetCheck(1);
	OnBnClickedBtnAutofresh();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void Csyslk_clientDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void Csyslk_clientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR Csyslk_clientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void Csyslk_clientDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	if (g_status.sys_state != sys_state_idle) {
		MessageBox(_T("当前系统忙，请完成相应任务后退出！"), _T("WARNING"), MB_OK | MB_ICONSTOP);
		return;
	}
	CDialogEx::OnOK();
}

afx_msg LRESULT Csyslk_clientDlg::OnCmiProgress(WPARAM wParam, LPARAM lParam)
{
	m_dlgProgress.PostMessage(WM_CMI_PROGRESS, wParam, lParam);
	return(0);
}

afx_msg LRESULT Csyslk_clientDlg::OnCmiSnapshot(WPARAM wParam, LPARAM lParam)
{
	CString str;
	str.Format(_T("0x%08x%08x"), wParam, lParam);
	GetDlgItem(IDC_STC_SNAPSHOT)->SetWindowText(str);
	return(0);
}

afx_msg LRESULT Csyslk_clientDlg::OnCmiUpdate(WPARAM wParam, LPARAM lParam)
{
	CString outstr;
	CWaitCursor wait;

	if (g_sys_version == 0) {
		outstr.Format("<UNKNOWN>");
	}else {
		if (g_sys_version != (uint32_t)-1) {
			outstr.Format(_T("%d.%d.%d"), (g_sys_version >> 28) & 0x0f,
										  (g_sys_version >> 24) & 0x0f,
										  (g_sys_version >> 0) & 0xffffff);
		} else {
			outstr.Format("<ERROR>");
		}
	}
	GetDlgItem(IDC_EDT_SYSVER)->SetWindowText(outstr);

	if (g_agt_version == 0) {
		outstr.Format("<UNKNOWN>");
	} else {
		if (g_agt_version != (uint32_t)-1) {
			outstr.Format(_T("%d.%d.%d"), (g_agt_version >> 28) & 0x0f,
										  (g_agt_version >> 24) & 0x0f,
										  (g_agt_version >> 0) & 0xffffff);
		} else {
			outstr.Format("<ERROR>");
		}
	}
	GetDlgItem(IDC_EDT_AGTVER)->SetWindowText(outstr);

	// last updated
	if (wParam == CMI_UPDATE_STATUS) {
		CTime t = CTime::GetCurrentTime();
		outstr.Format(_T("%04d/%02d/%02d %02d:%02d:%02d"), 
				t.GetYear(),
				t.GetMonth(),
				t.GetDay(),
				t.GetHour(),
				t.GetMinute(),
				t.GetSecond());
		GetDlgItem(IDC_EDT_LASTUPDATE)->SetWindowText(outstr);

		// svr_time
		CTime s_t(sys_lktm_to_systm(g_status.svr_time));
		outstr.Format(_T("%04d/%02d/%02d %02d:%02d:%02d"), 
				s_t.GetYear(),
				s_t.GetMonth(),
				s_t.GetDay(),
				s_t.GetHour(),
				s_t.GetMinute(),
				s_t.GetSecond());
		GetDlgItem(IDC_EDT_SVR_TIME)->SetWindowText(outstr);

		// sys_state
		switch(g_status.sys_state) {
		case sys_state_idle:
			outstr = _T("空闲");		break;
		case sys_state_rec:
			outstr = _T("采集中");		break;
		case sys_state_play:
			outstr = _T("回放中");		break;
		case sys_state_ul:
			outstr = _T("本地上传中");	break;
		case sys_state_dl:
			outstr = _T("本地下载中");	break;
		case sys_state_delete:
			outstr = _T("文件删除中");	break;
		case sys_state_format:
			outstr = _T("格式化中");	break;
		default:
			outstr = _T("未知");		break;
		}
		GetDlgItem(IDC_EDT_SYS_STAT)->SetWindowText(outstr);

		outstr.Format(_T("%.3f MBPS"), format_number_mega((uint64_t)g_status.fb_speed[0]*1024));
		GetDlgItem(IDC_EDT_SENDSPD_FC0)->SetWindowText(outstr);
		outstr.Format(_T("%.3f MBPS"), format_number_mega((uint64_t)g_status.fb_speed[1]*1024));
		GetDlgItem(IDC_EDT_SENDSPD_FC1)->SetWindowText(outstr);
		outstr.Format(_T("%.3f MBPS"), format_number_mega((uint64_t)g_status.fb_speed[2]*1024));
		GetDlgItem(IDC_EDT_SENDSPD_FC2)->SetWindowText(outstr);
		outstr.Format(_T("%.3f MBPS"), format_number_mega((uint64_t)g_status.fb_speed[3]*1024));
		GetDlgItem(IDC_EDT_SENDSPD_FC3)->SetWindowText(outstr);

		((CButton*)GetDlgItem(IDC_CHK_LINK_FC0))->SetCheck(g_status.fb_link[0]);
		((CButton*)GetDlgItem(IDC_CHK_LINK_FC1))->SetCheck(g_status.fb_link[1]);
		((CButton*)GetDlgItem(IDC_CHK_LINK_FC2))->SetCheck(g_status.fb_link[2]);
		((CButton*)GetDlgItem(IDC_CHK_LINK_FC3))->SetCheck(g_status.fb_link[3]);

		outstr.Format(_T("%s"), format_number(g_status.fb_recv_cnt[0]*1024));
		GetDlgItem(IDC_EDT_BYTES_FC0)->SetWindowText(outstr);
		outstr.Format(_T("%s"), format_number(g_status.fb_recv_cnt[1]*1024));
		GetDlgItem(IDC_EDT_BYTES_FC1)->SetWindowText(outstr);
		outstr.Format(_T("%s"), format_number(g_status.fb_recv_cnt[2]*1024));
		GetDlgItem(IDC_EDT_BYTES_FC2)->SetWindowText(outstr);
		outstr.Format(_T("%s"), format_number(g_status.fb_recv_cnt[3]*1024));
		GetDlgItem(IDC_EDT_BYTES_FC3)->SetWindowText(outstr);

		outstr.Format(_T("%.3f MB"), format_number_mega(g_status.disk_size));
		GetDlgItem(IDC_EDT_DISK_CAP)->SetWindowText(outstr);
		
		outstr.Format(_T("%.3f MB"), format_number_mega(g_status.disk_free));
		GetDlgItem(IDC_EDT_DISK_FREE)->SetWindowText(outstr);
	} else if(wParam == CMI_UPDATE_FILELIST) {
		g_fl_lock.Lock();
		m_lstFilelist.DeleteAllItems();
		for (int i=0; i<CMI_MAX_FL_SLOTS; i++) {
			cmi_work_list_t* fl = &g_filelist.work_list[i];
			if (fl->begin_tm > 0) {
				CString outstr;

				outstr.Format(_T("%d"), fl->slot_id);
				int nRow = m_lstFilelist.InsertItem(m_lstFilelist.GetItemCount(), outstr);
				int64_t file_sz = (int64_t)(((uint64_t)fl->file_sz_h) << 32 | fl->file_sz_l);
				if (file_sz < 0) {
					outstr.Format(_T("(ERROR)"));
				} else if (file_sz == 0) {
					outstr.Format(_T("(EMPTY)"));
				} else {
					file_sz *= 1024;
					outstr.Format(_T("%s"), format_number(file_sz));
				}
				m_lstFilelist.SetItemText(nRow, 1, outstr);

				CTime t(sys_lktm_to_systm(fl->begin_tm));
				outstr.Format(_T("%04d/%02d/%02d %02d:%02d:%02d"), 
						t.GetYear(),
						t.GetMonth(),
						t.GetDay(),
						t.GetHour(),
						t.GetMinute(),
						t.GetSecond());
				m_lstFilelist.SetItemText(nRow, 2, outstr);
				m_lstFilelist.SetItemData(nRow, (DWORD_PTR)fl);
			}
		}
		g_fl_lock.Unlock();
	}

	int state = g_status.sys_state;
	if (state == sys_state_rec)
		GetDlgItem(IDC_BTN_REC)->SetWindowText(_T("停止采集"));
	else
		GetDlgItem(IDC_BTN_REC)->SetWindowText(_T("开始采集"));

	if (state == sys_state_play)
		GetDlgItem(IDC_BTN_PLAYBACK)->SetWindowText(_T("停止回放"));
	else
		GetDlgItem(IDC_BTN_PLAYBACK)->SetWindowText(_T("回放..."));

	GetDlgItem(IDC_BTN_DOWNLOAD)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_UPLOAD)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_SYSDOWN)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_DELETE)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_SYNCTIME)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_REC)->EnableWindow(state == sys_state_idle || state == sys_state_rec);
	GetDlgItem(IDC_BTN_FORMAT)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_GETFL)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_UPGRADE)->EnableWindow(state == sys_state_idle);
	GetDlgItem(IDC_BTN_PLAYBACK)->EnableWindow(state == sys_state_idle  || state == sys_state_play);
	GetDlgItem(IDOK)->EnableWindow(state == sys_state_idle);

	return 0;
}

void Csyslk_clientDlg::OnBnClickedBtnInquiry()
{
	cmi_cmd_disp_inquiry();
}


void Csyslk_clientDlg::OnBnClickedBtnGetfl()
{
	// TODO: Add your control notification handler code here
	cmi_func_getfilelist();
}


void Csyslk_clientDlg::OnBnClickedBtnRec()
{
	int ret;
	GetDlgItem(IDC_STC_SNAPSHOT)->SetWindowText(_T(""));
	if (g_status.sys_state == sys_state_idle) {
		ret = cmi_cmd_disp_rec(1);
		if (ret != CMI_CMDEXEC_SUCC) {
			MessageBox(_T("开始采集失败！"), _T("ERROR"), MB_OK|MB_ICONSTOP);
		}
	} else if (g_status.sys_state == sys_state_rec) {
		ret = cmi_cmd_disp_rec(0);
		if (ret != CMI_CMDEXEC_SUCC) {
			MessageBox(_T("停止采集失败！"), _T("ERROR"), MB_OK|MB_ICONSTOP);
		} else {
			OnBnClickedBtnGetfl();
		}
	}
}


void Csyslk_clientDlg::OnBnClickedBtnFormat()
{
	// TODO: Add your control notification handler code here
	if (IDYES == MessageBox(_T("  ！！！警告！！！\n\n  格式化将清除存储设备上的所有数据，耗时几分钟。        \n  是否初始化存储设备？\n"), _T("CONFIRM"), MB_YESNO|MB_ICONWARNING)) {
		cmi_cmd_disp_format();
		OnBnClickedBtnGetfl();
	}
}


void Csyslk_clientDlg::OnBnClickedBtnDownload()
{
	int selected = -1;
	for(int i=0; i<m_lstFilelist.GetItemCount(); i++) {
		if( m_lstFilelist.GetItemState(i, LVIS_SELECTED) == LVIS_SELECTED) {
			selected = i;
			break;
		}
	}

	if (selected < 0) {
		MessageBox(_T("请先在文件列表中选择需要下载的文件\n"), _T("ERROR"), MB_OK | MB_ICONSTOP);
		return;
	}

	g_fl_lock.Lock();
	cmi_work_list_t* fl = (cmi_work_list_t*)m_lstFilelist.GetItemData(selected);
	uint64_t file_sz = ((uint64_t)fl->file_sz_h) << 32 | fl->file_sz_l;
	file_sz *= 1024;
	if (file_sz <= 0) {
		g_fl_lock.Unlock();
		return;
	}

	HANDLE hSaveFile;
	CFileDialog dlg(false, "bin", NULL, OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT, "(*.bin)|*.bin||");
	if(dlg.DoModal() == IDOK)
	{
		hSaveFile = ::CreateFileA(dlg.GetPathName(),
									GENERIC_WRITE,
									FILE_SHARE_READ,
									NULL,
									CREATE_ALWAYS,
									FILE_ATTRIBUTE_NORMAL,
									NULL);
		if (hSaveFile == INVALID_HANDLE_VALUE) {
			MessageBox(_T("打开文件失败"), _T("ERROR"), MB_OK| MB_ICONSTOP);
		} else {
			g_cancel_req = 0;
			m_dlgProgress.init_progress(file_sz);
			int ret = cmi_func_download(hSaveFile, fl);
		}
		g_fl_lock.Unlock();
	}
}

void Csyslk_clientDlg::OnTimer(UINT_PTR nIDEvent)
{
	KillTimer(CMI_TIMERID);
	if (g_quit_req)
		return;
	cmi_cmd_disp_inquiry();

	if (g_status.sys_state == sys_state_rec)
	{
		cmi_cmd_disp_snapshot();
	}
	SetTimer(CMI_TIMERID, 1000, 0);

	CDialogEx::OnTimer(nIDEvent);
}


void Csyslk_clientDlg::OnBnClickedBtnDelete()
{
	int selected = -1;
	for(int i=0; i<m_lstFilelist.GetItemCount(); i++) {
		if( m_lstFilelist.GetItemState(i, LVIS_SELECTED) == LVIS_SELECTED) {
			selected = i;
			break;
		}
	}

	if (selected < 0) {
		MessageBox(_T("请先在文件列表中选择需要删除的文件\n"), _T("ERROR"), MB_OK | MB_ICONSTOP);
		return;
	}

	CString strout;
	g_fl_lock.Lock();
	cmi_work_list_t* fl = (cmi_work_list_t*)m_lstFilelist.GetItemData(selected);
	uint64_t file_sz = ((uint64_t)fl->file_sz_h) << 32 | fl->file_sz_l;
	file_sz *= 1024;

	strout.Format(_T("是否删除编号为 #%d 大小为 %s 的文件？"), fl->slot_id, format_number(file_sz));
	if (IDYES != MessageBox(strout, _T("CONFIRM"), MB_YESNO|MB_ICONQUESTION)) {
		g_fl_lock.Unlock();
		return;
	}

	cmi_cmd_disp_delete(fl->slot_id);
	g_fl_lock.Unlock();

	OnBnClickedBtnGetfl();
}

void Csyslk_clientDlg::OnBnClickedBtnSynctime()
{
	// TODO: Add your control notification handler code here
	if (IDYES == MessageBox(_T("是否同步主机时间至设备？"), _T("CONFIRM"), MB_YESNO|MB_ICONQUESTION)) {
		cmi_cmd_disp_synctime();
	}
}


void Csyslk_clientDlg::OnBnClickedBtnPlayback()
{
	if (g_status.sys_state == sys_state_idle) {
		int selected = -1;
		for(int i=0; i<m_lstFilelist.GetItemCount(); i++) {
			if( m_lstFilelist.GetItemState(i, LVIS_SELECTED) == LVIS_SELECTED) {
				selected = i;
				break;
			}
		}

		if (selected < 0) {
			MessageBox(_T("请先在文件列表中选择回放的文件\n"), _T("ERROR"), MB_OK | MB_ICONSTOP);
			return;
		}

		CString strout;
		g_fl_lock.Lock();
		cmi_work_list_t* fl = (cmi_work_list_t*)m_lstFilelist.GetItemData(selected);
		uint64_t file_sz = ((uint64_t)fl->file_sz_h) << 32 | fl->file_sz_l;
		file_sz *= 1024;

		strout.Format(_T("是否回放编号为 #%d 大小为 %s 的文件？"), fl->slot_id, format_number(file_sz));
		if (IDYES != MessageBox(strout, _T("CONFIRM"), MB_YESNO|MB_ICONQUESTION)) {
			g_fl_lock.Unlock();
			return;
		}

		cmi_cmd_disp_play(fl->slot_id);
		g_fl_lock.Unlock();
	} else {
		cmi_cmd_disp_stopplay();
	}
}


void Csyslk_clientDlg::OnBnClickedBtnAutofresh()
{
	// TODO: Add your control notification handler code here
	int checked = ((CButton*)GetDlgItem(IDC_BTN_AUTOFRESH))->GetCheck();
	if (checked) {
		GetDlgItem(IDC_BTN_INQUIRY)->EnableWindow(false);
		SetTimer(CMI_TIMERID, 1000, 0);
	} else {
		GetDlgItem(IDC_BTN_INQUIRY)->EnableWindow(true);
		KillTimer(CMI_TIMERID);
	}
}

void Csyslk_clientDlg::OnBnClickedBtnUpload()
{
	if (g_status.sys_state == sys_state_idle) {
		CFileDialog dlg(true, "*", NULL, OFN_HIDEREADONLY, "(*.*)|*.*||");
		if(dlg.DoModal() == IDOK)
		{
			HANDLE hFile = ::CreateFileA(dlg.GetPathName(),
										GENERIC_READ,
										FILE_SHARE_READ,
										NULL,
										OPEN_EXISTING,
										FILE_ATTRIBUTE_NORMAL,
										NULL);
			if (hFile == INVALID_HANDLE_VALUE) {
				MessageBox(_T("打开文件失败"), _T("ERROR"), MB_OK| MB_ICONSTOP);
			} else {
				g_cancel_req = 0;
				uint64_t file_sz = 0l;
				DWORD file_sz_l, file_sz_h;
				file_sz_l = GetFileSize(hFile, &file_sz_h);
				file_sz = (((uint64_t)file_sz_h) << 32) | file_sz_l;
				m_dlgProgress.init_progress(file_sz);
				cmi_func_upload(hFile);
			}
		}
	}
}


void Csyslk_clientDlg::OnBnClickedBtnSysdown()
{
	// TODO: Add your control notification handler code here
	if (IDYES == MessageBox(_T("是否关闭设备？"), _T("CONFIRM"), MB_YESNO|MB_ICONQUESTION)) {
		((CButton*)GetDlgItem(IDC_BTN_AUTOFRESH))->SetCheck(0);
		cmi_cmd_disp_sysdown();
		this->OnCancel();
	}
}


void Csyslk_clientDlg::OnBnClickedBtnUpgrade()
{
	if (g_status.sys_state == sys_state_idle) {
		CFileDialog dlg(true, "*", NULL, OFN_HIDEREADONLY, "(*.*)|*.*||");
		if(dlg.DoModal() == IDOK)
		{
			HANDLE hFile = ::CreateFileA(dlg.GetPathName(),
										GENERIC_READ,
										FILE_SHARE_READ,
										NULL,
										OPEN_EXISTING,
										FILE_ATTRIBUTE_NORMAL,
										NULL);
			if (hFile == INVALID_HANDLE_VALUE) {
				MessageBox(_T("打开文件失败"), _T("ERROR"), MB_OK| MB_ICONSTOP);
			} else {
				g_cancel_req = 0;
				uint64_t file_sz = 0l;
				DWORD file_sz_l, file_sz_h;
				file_sz_l = GetFileSize(hFile, &file_sz_h);
				file_sz = (((uint64_t)file_sz_h) << 32) | file_sz_l;
				m_dlgProgress.init_progress(file_sz);
				cmi_func_upgrade(hFile);
			}
		}
	}
}
