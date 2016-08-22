
// stdafx.cpp : source file that includes just the standard includes
// syslk_client.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"
#include "winsock.h"
#include "afxsock.h"

CString g_svr_ip = "192.168.248.128";
int g_svr_port = 1235;
int g_use_tcp = 1;
uint32_t g_sys_version = 0;
uint32_t g_agt_version = 0;

typedef struct {
	CEvent cmd_done;
	int cmd_type;
	int success;
} CMI_CMD_CTX;

typedef struct {
	uint16_t data_type;
	uint16_t data_tag;
	uint64_t total_len;
	uint64_t recv_len;
	char* buffer;
	uint32_t bufsize;
	CEvent* chunk_done;
	int (*handler)(char* buffer, uint32_t bufsize);
} CMI_CHUNK_CTX;

SOCKET g_sockfd = -1;
SOCKADDR_IN g_svr_addr;
cmi_status_t g_status;

CMutex	g_cur_cmd_lock;
CMI_CMD_CTX* g_cur_cmd = NULL;

CMutex	g_cur_data_lock;
CMI_CHUNK_CTX* g_cur_data = NULL;

CMutex	g_fl_lock;
cmi_data_filelist_t g_filelist;

HWND hwndProgress = NULL;
HWND hwndMain = NULL;

CMutex g_socksend_lock;

int g_cancel_req = 0;
int g_quit_req = 0;

void GetLastErrorEx(LPCTSTR reason)
{  
    DWORD dwMessageID = ::GetLastError();  
    TCHAR *szMessage = NULL;  
    TCHAR szDetail[1024] = {0};  

    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,  
        NULL, dwMessageID, 0, (LPSTR) &szMessage, 0, NULL);  
	
    _stprintf_s(szDetail, _T("  %s\n\n ::GetLastError()=%d\n  ErrMsg=%s\n"), reason, dwMessageID, szMessage);  
    AfxGetMainWnd()->MessageBox(szDetail, _T("ERROR"), MB_OK | MB_ICONSTOP);
} 


int cmi_init()
{
	int ret;

	if (g_use_tcp) {
		g_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	} else {
		g_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	}
	ASSERT(g_sockfd > 0);

	SOCKADDR_IN cli_addr;
	cli_addr.sin_addr.S_un.S_addr = INADDR_ANY; 
	cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(g_svr_port);

	ret = bind(g_sockfd, (SOCKADDR*)&cli_addr, sizeof(SOCKADDR));
	if (ret < 0) {
		GetLastErrorEx(_T("bind() Failed!"));
		ASSERT(0);
		return(ret);
	}

    g_svr_addr.sin_addr.S_un.S_addr = inet_addr(g_svr_ip);
	g_svr_addr.sin_family = AF_INET;
    g_svr_addr.sin_port = htons(g_svr_port);

	if (g_use_tcp) {
		ret = connect(g_sockfd, (SOCKADDR*)&g_svr_addr, sizeof(SOCKADDR));
		if (ret < 0) {
			GetLastErrorEx(_T("connect() Failed!"));
			return(ret);
		}
	}

	memset(&g_status, 0, sizeof(g_status));
	g_status.sys_state = sys_state_max;

	memset(&g_filelist, 0, sizeof(g_filelist));

	return(0);
}

int cmi_sock_send(char* buf, int size)
{
	int ret;

	g_socksend_lock.Lock();
	if (g_use_tcp) {
		int xferred = 0;
		while(xferred < size) {
			ret = send(g_sockfd, buf + xferred, size - xferred, 0);
			if (ret < 0)
				break;
			xferred += ret;
		}
		if (ret > 0) {
			ret = xferred;
		}
	} else {
		ret = sendto(g_sockfd, buf, size, 0, (SOCKADDR*)&g_svr_addr, sizeof(SOCKADDR));
	}
	if (ret < 0) {
		ret = ::GetLastError();
		ret = -ret;
	}
	g_socksend_lock.Unlock();
	return(ret);
}

int cmi_sock_recv(char* buf, int size)
{
	int ret = CMIERR_UNKNOWN;
	int addr_len = sizeof(SOCKADDR);
	cmi_msg_hdr_t* msg_hdr = (cmi_msg_hdr_t*)buf;

	if (g_use_tcp) {
		int recved = 0;
		while(recved < sizeof(cmi_msg_hdr_t)) {
			ret = recv(g_sockfd, buf + recved, sizeof(cmi_msg_hdr_t) - recved, 0);
			if (ret < 0)
				break;
			recved += ret;
		}
		if (recved == sizeof(cmi_msg_hdr_t)) {
			int msg_len = msg_hdr->msg_len;
			ASSERT(msg_len <= CMI_MAX_MSGSIZE);
			int remain = msg_len - sizeof(cmi_msg_hdr_t);
			char* pl_buf = buf + sizeof(cmi_msg_hdr_t);
			recved = 0;
			while(recved < remain) {
				ret = recv(g_sockfd, pl_buf + recved, remain - recved, 0);
				if (ret < 0)
					break;
				recved += ret;
			}
			ret = msg_len;
		}
	} else {
		ret = recvfrom(g_sockfd, buf, size, 0, (SOCKADDR*)&g_svr_addr, &addr_len);
	}
	if (ret < 0) {
		ret = ::GetLastError();
		ret = -ret;
	} else {
		// check packet
		if (msg_hdr->sync_tag != CMI_SYNC_TAG ||
			msg_hdr->msg_len <= sizeof(cmi_msg_hdr_t) ||
			msg_hdr->msg_len > size) {
			// illegal packet
			return(CMIERR_EPACKET);
		}
	}

	return(ret);
}

int cmi_cmd_dispatch(cmi_cmd_t* cmd, int size)
{
	int ret;

	g_cur_cmd_lock.Lock();
	if (g_cur_cmd) {
		// busy
		g_cur_cmd_lock.Unlock();
		return(CMIERR_EAGAIN);
	}

	g_cur_cmd = new CMI_CMD_CTX;
	g_cur_cmd->cmd_done.ResetEvent();
	g_cur_cmd->cmd_type = cmd->cmd_type;
	g_cur_cmd_lock.Unlock();

	CWaitCursor wait;

	ret = cmi_sock_send((char*)cmd, size);
	if (ret < 0) {
		SAFE_RELEASE(g_cur_cmd);
		return ret;
	}

	DWORD result = ::WaitForSingleObject((HANDLE)g_cur_cmd->cmd_done, 5*1000);
	g_cur_cmd_lock.Lock();
	ret = g_cur_cmd->success;
	SAFE_RELEASE(g_cur_cmd);
	g_cur_cmd_lock.Unlock();
	CString	str;
	if (result != WAIT_OBJECT_0) {
		str.Format(_T("接受命令反馈超时: cmd=0x%x"), cmd->cmd_type);
		AfxGetMainWnd()->MessageBox(str, _T("ERROR"), MB_OK | MB_ICONSTOP);
		return(CMIERR_BADSEQ);
	}
	if (ret != CMI_CMDEXEC_SUCC) {
		str.Format(_T("命令执行失败: cmd=0x%x, resp=0x%x"), cmd->cmd_type, ret);
		AfxGetMainWnd()->MessageBox(str, _T("ERROR"), MB_OK | MB_ICONSTOP);
	}
	return(ret);
}

int cmi_cmd_disp_rec(int start)
{
	CWaitCursor wc;
	cmi_cmd_t	cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	if (start)
		cmd.cmd_type = cmd_type_start_rec;
	else
		cmd.cmd_type = cmd_type_stop_rec;

	ret = cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t));

	cmi_cmd_disp_inquiry(); 
	return(ret);
}

int cmi_cmd_disp_snapshot()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_snapshot;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_init()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_init;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_sysdown()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_sysdown;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_inquiry()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_inquiry;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_format()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_format;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_synctime()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_sync_time;
	CTime t = CTime::GetCurrentTime();
	cmd.u.tm.lktime = sys_systm_to_lktm(t.GetTime());

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_delete(int slot_id)
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_delete;
	cmd.u.file.index = slot_id;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_play(int slot_id)
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_start_play;
	cmd.u.file.index = slot_id;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_stopplay()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_stop_play;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_getfilelist()
{
	CWaitCursor wc;
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_filelist;
	cmd.u.file.frag_id = (uint32_t)-1;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_dl(int start, int slot_id, uint32_t blk_start, uint32_t blk_num)
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = start?cmd_type_start_dl:cmd_type_stop_dl;
	cmd.u.file.index = slot_id;
	cmd.u.file.blk_start = blk_start;
	cmd.u.file.blk_num = blk_num;
	cmd.u.file.frag_id = (uint32_t)-1;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_upload(int start)
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = start?cmd_type_start_ul:cmd_type_stop_ul;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_cmd_disp_upgrade()
{
	cmi_cmd_t	cmd;

	memset(&cmd, 0, sizeof(cmi_cmd_t));
	cmd.hdr.sync_tag = CMI_SYNC_TAG;
	cmd.hdr.msg_code = msg_code_cmd;
	cmd.hdr.msg_len = sizeof(cmi_cmd_t);
	cmd.cmd_type = cmd_type_upgrade;

	return(cmi_cmd_dispatch(&cmd, sizeof(cmi_cmd_t)));
}

int cmi_data_parse(HWND hwndMain, cmi_data_t* msgdata, int size)
{
	cmi_msg_hdr_t* hdr = (cmi_msg_hdr_t*)msgdata;
	ASSERT(hdr->msg_len == sizeof(cmi_data_t));

	int data_type = msgdata->data_type;
	uint32_t frag_len = msgdata->frag_len;
	uint16_t iseof = msgdata->eof;

	if (data_type == data_type_snap) {
		::PostMessageA(hwndMain, WM_CMI_SNAPSHOT, (WPARAM)*(uint32_t*)&msgdata->frag_data[0], (WPARAM)*(uint32_t*)&msgdata->frag_data[4]);
	}

	g_cur_data_lock.Lock();
	if (!g_cur_data) {
		// no one care about our data
		// FIXME: just drop recved data
		TRACE("Drop %d bytes\n", size);
		g_cur_data_lock.Unlock();
		return(CMIERR_BADSEQ);
	}

	if (g_cur_data->data_type != data_type) {
		// wrong data type
		// FIXME: just drop recved data
		ASSERT(0);
	}

	ASSERT(g_cur_data->buffer);

	memcpy(g_cur_data->buffer + g_cur_data->bufsize, msgdata->frag_data, frag_len);
	g_cur_data->recv_len += frag_len;
	g_cur_data->bufsize += frag_len;
	::PostMessageA(hwndProgress, WM_CMI_PROGRESS, LOW32(g_cur_data->recv_len), HIGH32(g_cur_data->recv_len));
	g_cur_data_lock.Unlock();

	if (iseof || (g_cur_data->recv_len == g_cur_data->total_len)) {
		// all frags received
		if (iseof & 0x01) {
			ASSERT(g_cur_data->recv_len == g_cur_data->total_len);
		}
		g_cur_data->handler(g_cur_data->buffer, g_cur_data->bufsize);
		g_cur_data->bufsize = 0;
		g_cur_data->chunk_done->SetEvent();
	} else {
		ASSERT(g_cur_data->bufsize <= CMI_MAX_DL_SIZE);
		if (g_cur_data->bufsize == CMI_MAX_DL_SIZE) {
			g_cur_data->handler(g_cur_data->buffer, g_cur_data->bufsize);
			g_cur_data->bufsize = 0;
		}
	}

	return(0);
}

int cmi_cmdresp_parse(cmi_cmdresp_t* cmdresp, int size)
{
	cmi_msg_hdr_t* hdr = (cmi_msg_hdr_t*)cmdresp;
	ASSERT(hdr->msg_len == sizeof(cmi_cmdresp_t));

	g_cur_cmd_lock.Lock();
	if (!g_cur_cmd || 
		g_cur_cmd->cmd_type != cmdresp->cmd_type) {
		g_cur_cmd_lock.Unlock();
		return(CMIERR_BADSEQ);
	}
	g_cur_cmd->success = cmdresp->success;
	g_cur_cmd->cmd_done.SetEvent();
	if (cmdresp->cmd_type == cmd_type_init) {
		g_sys_version = cmdresp->u.version.sys_ver;
		g_agt_version = cmdresp->u.version.agt_ver;
	}

	g_cur_cmd_lock.Unlock();

	return(0);
}

int cmi_status_parse(cmi_status_t* status, int size)
{
	cmi_msg_hdr_t* hdr = (cmi_msg_hdr_t*)status;
	ASSERT(hdr->msg_len == sizeof(cmi_status_t));

	memcpy(&g_status, status, sizeof(cmi_status_t));

	return(0);
}

int __cmi_build_chunk(int data_type, uint64_t total_len, int (*handler)(char* buffer, uint32_t size))
{
	g_cur_data_lock.Lock();
	if (g_cur_data) {
		// busy
		AfxGetMainWnd()->MessageBox(_T("数据传输中"), _T("ERROR"), MB_OK | MB_ICONSTOP);
		g_cur_data_lock.Unlock();
		return(CMIERR_EAGAIN);
	}

	g_cur_data = new CMI_CHUNK_CTX;
	memset(g_cur_data, 0, sizeof(CMI_CHUNK_CTX));
	g_cur_data->chunk_done = new CEvent;
	g_cur_data->chunk_done->ResetEvent();
	g_cur_data->data_type = data_type;
	g_cur_data->total_len = total_len;
	g_cur_data->buffer = (char*)malloc(CMI_MAX_DL_SIZE);
	g_cur_data->bufsize = 0;
	g_cur_data->handler = handler;
	g_cur_data_lock.Unlock();

	return(CMI_SUCCESS);
}

static int cmi_cb_getfilelist(char* buf, uint32_t bufsize)
{
	ASSERT(bufsize == sizeof(cmi_data_filelist_t));
	memcpy(&g_filelist, g_cur_data->buffer, sizeof(cmi_data_filelist_t));
	return(0);
}
int cmi_func_getfilelist(void)
{
	CWaitCursor wait;
	int ret = 0;

	ret = __cmi_build_chunk(data_type_flist, sizeof(cmi_data_filelist_t), cmi_cb_getfilelist);
	if (ret == CMI_SUCCESS) {
		// trig cmd
		ret = cmi_cmd_disp_getfilelist();
		if (ret == CMI_CMDEXEC_SUCC) {
			// wait for result
			DWORD result = ::WaitForSingleObject((HANDLE)g_cur_data->chunk_done->m_hObject, 5*1000);
			if (result != WAIT_OBJECT_0) {
				AfxGetMainWnd()->MessageBox(_T("接受文件列表超时"), _T("ERROR"), MB_OK | MB_ICONSTOP);
				ret = CMIERR_TIMEOUT;
			} else {
				ret = CMI_SUCCESS;
			}
		}

		// clear
		g_cur_data_lock.Lock();
		SAFE_RELEASE(g_cur_data->buffer);
		delete g_cur_data->chunk_done;
		g_cur_data->chunk_done = NULL;
		SAFE_RELEASE(g_cur_data);
		g_cur_data_lock.Unlock();
	}

	if (ret == CMI_SUCCESS) {
		PostMessage(hwndMain, WM_CMI_UPDATE, CMI_UPDATE_FILELIST, 0);
	}

	return(ret);
}

HANDLE hUlThread = INVALID_HANDLE_VALUE;
DWORD WINAPI cmi_send_uldata(LPVOID lpParameter)
{
	HANDLE	hFile = (HANDLE)lpParameter;
	int ret = -1;
	uint32_t frag_id = 0;
	cmi_data_t	msg_data;
	uint64_t xferred = 0l;

	uint64_t file_sz = 0l;
	DWORD file_sz_l, file_sz_h;
	file_sz_l = GetFileSize(hFile, &file_sz_h);
	file_sz = (((uint64_t)file_sz_h) << 32) | file_sz_l;

	memset(&msg_data, 0, sizeof(cmi_data_t));
	msg_data.hdr.sync_tag = CMI_SYNC_TAG;
	msg_data.hdr.msg_code = msg_code_data;
	msg_data.hdr.msg_len = sizeof(cmi_data_t);
	msg_data.data_type = data_type_ul;
	
	cmi_cmd_disp_upload(1);
	
	while(xferred < file_sz) {
		DWORD xfer_req = (DWORD)min(file_sz-xferred, CMI_MAX_FRAGSIZE);
		DWORD xfer = 0;

		int ret;
		ret = ::ReadFile(hFile, msg_data.frag_data, CMI_MAX_FRAGSIZE, &xfer, NULL);
		if (ret) {
			ASSERT(xfer == xfer_req);
			msg_data.frag_id = frag_id++;
			msg_data.eof = (xferred + xfer >= file_sz);
			msg_data.frag_len = xfer;
			ret = cmi_sock_send((char*)&msg_data, sizeof(cmi_data_t));
			if (ret != sizeof(cmi_data_t)) {
				goto errout;
			}
			xferred += xfer;
			::PostMessageA(hwndProgress, WM_CMI_PROGRESS, LOW32(xferred), HIGH32(xferred));
		}
		if (g_cancel_req) {
			g_cancel_req = 0;
			cmi_cmd_disp_upload(0);
			break;
		}
		if (g_quit_req) {
			break;
		}
	};
	ret = 0;
errout:
	CloseHandle(hFile);

	int is_abort = (xferred < file_sz);
	ShowWindow(hwndProgress, SW_HIDE);

	cmi_func_getfilelist();

	CString strout;

	strout.Format(_T("文件上传%s  \n\n文件长度: %s\n上传长度: %s"), is_abort?_T("中断"):_T("完成"),
					format_number(file_sz), format_number(xferred));
	AfxGetMainWnd()->MessageBox(strout, _T("INFO"), MB_OK | (is_abort?MB_ICONWARNING:MB_ICONINFORMATION));
	hUlThread = INVALID_HANDLE_VALUE;
	return(CMI_SUCCESS);
}

int cmi_func_upload(HANDLE hFile)
{
	ASSERT(hUlThread == INVALID_HANDLE_VALUE);

	hUlThread = CreateThread(NULL, 0, cmi_send_uldata, (LPVOID)hFile, 0, NULL);
	ASSERT(hUlThread != INVALID_HANDLE_VALUE);

	return(CMI_SUCCESS);
}

HANDLE hUpgradeThread = INVALID_HANDLE_VALUE;
DWORD WINAPI cmi_send_upgrade(LPVOID lpParameter)
{
	HANDLE	hFile = (HANDLE)lpParameter;
	int ret = -1;
	uint32_t frag_id = 0;
	cmi_data_t	msg_data;
	uint64_t xferred = 0l;

	uint64_t file_sz = 0l;
	DWORD file_sz_l, file_sz_h;
	file_sz_l = GetFileSize(hFile, &file_sz_h);
	file_sz = (((uint64_t)file_sz_h) << 32) | file_sz_l;

	memset(&msg_data, 0, sizeof(cmi_data_t));
	msg_data.hdr.sync_tag = CMI_SYNC_TAG;
	msg_data.hdr.msg_code = msg_code_data;
	msg_data.hdr.msg_len = sizeof(cmi_data_t);
	msg_data.data_type = data_type_upgrade;
	
	cmi_cmd_disp_upgrade();
	
	while(xferred < file_sz) {
		DWORD xfer_req = (DWORD)min(file_sz-xferred, CMI_MAX_FRAGSIZE);
		DWORD xfer = 0;

		int ret;
		ret = ::ReadFile(hFile, msg_data.frag_data, CMI_MAX_FRAGSIZE, &xfer, NULL);
		if (ret) {
			ASSERT(xfer == xfer_req);
			msg_data.frag_id = frag_id++;
			msg_data.eof = (xferred + xfer >= file_sz);
			msg_data.frag_len = xfer;
			ret = cmi_sock_send((char*)&msg_data, sizeof(cmi_data_t));
			if (ret != sizeof(cmi_data_t)) {
				goto errout;
			}
			xferred += xfer;
			::PostMessageA(hwndProgress, WM_CMI_PROGRESS, LOW32(xferred), HIGH32(xferred));
		}
	};
	ret = 0;
errout:
	CloseHandle(hFile);

	ShowWindow(hwndProgress, SW_HIDE);

	CString strout;

	strout.Format(_T("更新文件上传完成  \n\n文件长度: %s\n上传长度: %s"),
					format_number(file_sz), format_number(xferred));
	AfxGetMainWnd()->MessageBox(strout, _T("INFO"), MB_OK | MB_ICONINFORMATION);
	hUpgradeThread = INVALID_HANDLE_VALUE;
	return(CMI_SUCCESS);
}

int cmi_func_upgrade(HANDLE hFile)
{
	ASSERT(hUpgradeThread == INVALID_HANDLE_VALUE);

	hUpgradeThread = CreateThread(NULL, 0, cmi_send_upgrade, (LPVOID)hFile, 0, NULL);
	ASSERT(hUpgradeThread != INVALID_HANDLE_VALUE);

	return(CMI_SUCCESS);
}

#ifdef ANALYZE_DATA
uint64_t fc_counter[4];
uint64_t fc_unknown;
uint64_t fc_illegal;
uint64_t fc_total;
void cmi_analyze_start()
{
	for (int i=0; i<4; i++) {
		fc_counter[i] = 0;
	}
	fc_unknown = fc_total = 0;
}

#define SIZE_4K	(4*1024)
void cmi_analyze(char* buf, uint64_t bufsz)
{
	// 0x18efdc0x
	for (int i=0; i<bufsz/SIZE_4K; i++) {
		fc_total++;
		uint32_t tag = *(((uint32_t*)(buf + i*SIZE_4K)) + 2);
		if ((tag & 0x00ffffff) == 0x00dcef18) {
			int ch = ((tag & 0xff000000) >> 24);
			if (ch >= 1 && ch <= 4) {
				fc_counter[ch-1]++;
			} else {
				fc_illegal++;
			}
		} else {
			fc_unknown++;
		}
	}
}

void cmi_analyze_end()
{
	CString outstr;

	outstr.Format(_T("total=%s\n\nfc_count0=%s\nfc_count1=%s\nfc_count2=%s\nfc_count3=%s\n\n"
					"unknown=%s\nillegal=%s\n"),
					format_number(fc_total),
					format_number(fc_counter[0]),
					format_number(fc_counter[1]),
					format_number(fc_counter[2]),
					format_number(fc_counter[3]),
					format_number(fc_unknown),
					format_number(fc_illegal));
	AfxGetMainWnd()->MessageBox(outstr, _T("statistics"), MB_OK);
}
#endif

HANDLE hDlThread = INVALID_HANDLE_VALUE;
HANDLE hDlFile = INVALID_HANDLE_VALUE;
static int cmi_cb_recvdldata(char* buf, uint32_t bufsize)
{
	DWORD dwWritenSize = 0;
	::WriteFile(hDlFile, g_cur_data->buffer, bufsize, &dwWritenSize, NULL);
#ifdef ANALYZE_DATA
	cmi_analyze(g_cur_data->buffer, bufsize);
#endif

	return(0);
}
DWORD WINAPI cmi_recv_dldata(LPVOID lpParameter)
{
	cmi_work_list_t* fl = (cmi_work_list_t*)lpParameter;
	int ret = -1;
	int slot_id = fl->slot_id;
	uint64_t file_sz = ((uint64_t)fl->file_sz_h) << 32 | fl->file_sz_l;
	file_sz *= 1024;
	uint64_t last_offset = 0;

#ifdef ANALYZE_DATA
	cmi_analyze_start();
#endif
	ret = __cmi_build_chunk(data_type_dl, file_sz, cmi_cb_recvdldata);
	// trig cmd
	ret = cmi_cmd_disp_dl(1, slot_id, 0, 0);
	if (ret == CMI_CMDEXEC_SUCC) {
		// wait for result
		while(1) {
			DWORD result = ::WaitForSingleObject((HANDLE)g_cur_data->chunk_done->m_hObject, 3*1000);
			if (g_cancel_req) {
				cmi_cmd_disp_dl(0, slot_id, 0, 0);
			}
			if (result != WAIT_OBJECT_0 && !g_cancel_req) {
				g_cur_data_lock.Lock();
				if (g_cur_data->recv_len > last_offset) {
					last_offset = g_cur_data->recv_len;
					g_cur_data_lock.Unlock();
					continue;
				}
				AfxGetMainWnd()->MessageBox(_T("接受下载超时"), _T("ERROR"), MB_OK | MB_ICONSTOP);
				ret = CMIERR_TIMEOUT;
				g_cur_data_lock.Unlock();
				break;
			} else {
				ret = CMI_SUCCESS;
				break;
			}
			if (g_quit_req) {
				break;
			}
		}
		if (g_cancel_req) {
			g_cancel_req = 0;
		}
	}

	int is_abort = (g_cur_data->recv_len < g_cur_data->total_len);
	ShowWindow(hwndProgress, SW_HIDE);

	CString strout;

	strout.Format(_T("文件下载%s  \n\n文件长度: %s\n下载长度: %s"), is_abort?_T("中断"):_T("完成"), format_number(g_cur_data->total_len), format_number(g_cur_data->recv_len));
	AfxGetMainWnd()->MessageBox(strout, _T("INFO"), MB_OK | (is_abort?MB_ICONWARNING:MB_ICONINFORMATION));

	// clear
	g_cur_data_lock.Lock();
	SAFE_RELEASE(g_cur_data->buffer);
	delete g_cur_data->chunk_done;
	g_cur_data->chunk_done = NULL;
	SAFE_RELEASE(g_cur_data);
	g_cur_data_lock.Unlock();

	CloseHandle(hDlFile);

#ifdef ANALYZE_DATA
	cmi_analyze_end();
#endif

	hDlFile = INVALID_HANDLE_VALUE;
	hDlThread = INVALID_HANDLE_VALUE;
	return(ret);
}

int cmi_func_download(HANDLE hSaveFile, cmi_work_list_t* fl)
{
	ASSERT(hDlThread == INVALID_HANDLE_VALUE);

	hDlFile = hSaveFile;
	hDlThread = CreateThread(NULL, 0, cmi_recv_dldata, (LPVOID)fl, 0, NULL);
	ASSERT(hDlThread != INVALID_HANDLE_VALUE);

	return(CMI_SUCCESS);
}

DWORD WINAPI cmi_recv_thread(LPVOID lpParameter)
{
	char recv_buf[CMI_MAX_MSGSIZE];
	cmi_msg_hdr_t* hdr = (cmi_msg_hdr_t*)recv_buf;
	int ret;
	int parse_ret = -1;
	HWND hwnd = (HWND)lpParameter;

	while(1) {
		ret = cmi_sock_recv(recv_buf, CMI_MAX_MSGSIZE);
		if (ret < 0) {
			GetLastErrorEx(_T("与服务器连接异常，程序即将退出"));
			g_quit_req = 1;
			PostMessage(hwndMain, WM_CLOSE, 0, 0);
			return 0;
		}
		if (ret > 0) {
			if (hdr->msg_code == msg_code_data) {
				parse_ret = cmi_data_parse(hwnd, (cmi_data_t*)recv_buf, ret);
			} else if (hdr->msg_code == msg_code_cmdresp) {
				parse_ret = cmi_cmdresp_parse((cmi_cmdresp_t*)recv_buf, ret);
			} else if (hdr->msg_code == msg_code_status) {
				parse_ret = cmi_status_parse((cmi_status_t*)recv_buf, ret);
				::PostMessageA(hwnd, WM_CMI_UPDATE, CMI_UPDATE_STATUS, 0);
			}
		}
	}

	return(0);
}

uint64_t sys_lktm_to_systm(uint32_t lktm)
{
    uint64_t systm;

    struct tm local_time;
    memset(&local_time, 0, sizeof(struct tm));
    
    local_time.tm_sec  = (lktm >> 0) & 0x3f;
    local_time.tm_min  = (lktm >> 6) & 0x3f;
    local_time.tm_hour = (lktm >> 12) & 0x1f;
    local_time.tm_mday = (lktm >> 17) & 0x1f;
    local_time.tm_mon  = ((lktm >> 22) & 0x0f) - 1;
    local_time.tm_year = (lktm >> 26) & 0x3f;
    local_time.tm_year += 100;

    systm = mktime(&local_time);
    return(systm);
}

uint32_t sys_systm_to_lktm(uint64_t systm)
{
    uint32_t lk_tm;
    
	CTime t(systm);
    
	lk_tm = t.GetSecond();
	lk_tm |= ((t.GetMinute())  << 6);
	lk_tm |= ((t.GetHour()) << 12);
	lk_tm |= ((t.GetDay()) << 17);
	lk_tm |= ((t.GetMonth())  << 22);
	lk_tm |= ((t.GetYear()-2000) << 26);

    return(lk_tm);
}

double format_number_mega(uint64_t byte)
{
	double ret = (double)byte;
	ret /= 1000*1000;
	return(ret);
}

CString format_number(int64_t number)
{
	CString strnum;
	CString strret = _T("");
	CString temp;

	strnum.Format(_T("%I64d"), number);

	while(strlen(strnum) > 0) {
		strret = strnum.Right(3) + strret;
		strnum = strnum.Left(strlen(strnum)-3);
		if (strlen(strnum) > 0)
			strret = ',' + strret;
	}

	return(strret);
}
