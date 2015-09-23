/*
 * Copyright (c) 2009-2014 Intel Corporation.  All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */
#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_ib_util.h"
#include "dapl_osd.h"

#include <stdlib.h>
#include <ifaddrs.h>

int g_dapl_loopback_connection = 0;


#if defined(_WIN64) || defined(_WIN32)
#include "..\..\..\..\..\etc\user\comp_channel.cpp"
#include <rdma\winverbs.h>

int getipaddr_netdev(char *name, char *addr, int addr_len)
{
	IWVProvider *prov;
	WV_DEVICE_ADDRESS devaddr;
	struct addrinfo *res, *ai;
	HRESULT hr;
	int index;

	if (strncmp(name, "rdma_dev", 8)) {
		return EINVAL;
	}

	index = atoi(name + 8);

	hr = WvGetObject(&IID_IWVProvider, (LPVOID *) &prov);
	if (FAILED(hr)) {
		return hr;
	}

	hr = getaddrinfo("..localmachine", NULL, NULL, &res);
	if (hr) {
		goto release;
	}

	for (ai = res; ai; ai = ai->ai_next) {
		hr = prov->lpVtbl->TranslateAddress(prov, ai->ai_addr, &devaddr);
		if (SUCCEEDED(hr) && (ai->ai_addrlen <= addr_len) && (index-- == 0)) {
			memcpy(addr, ai->ai_addr, ai->ai_addrlen);
			goto free;
		}
	}
	hr = ENODEV;

free:
	freeaddrinfo(res);
release:
	prov->lpVtbl->Release(prov);
	return hr;
}

DAT_RETURN getlocalipaddr(char *addr, int addr_len)
{
	struct sockaddr_in *sin;
	struct addrinfo *res, hint, *ai;
	int ret;
	char hostname[256];
	char *netdev = getenv("DAPL_SCM_NETDEV");

retry:
	/* use provided netdev instead of default hostname */
	if (netdev != NULL) {
		ret = getipaddr_netdev(netdev, addr, addr_len);
		if (ret) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " getlocalipaddr: NETDEV = %s"
				 " but not configured on system? ERR = %s\n",
				 netdev, strerror(ret));
			return dapl_convert_errno(ret, "getlocalipaddr");
		} else
			return DAT_SUCCESS;
	}

	if (addr_len < sizeof(*sin)) {
		return DAT_INTERNAL_ERROR;
	}

	ret = gethostname(hostname, 256);
	if (ret)
		return dapl_convert_errno(ret, "gethostname");

	memset(&hint, 0, sizeof hint);
	hint.ai_flags = AI_PASSIVE;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;

	ret = getaddrinfo(hostname, NULL, &hint, &res);
	if (ret) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " getaddrinfo ERR: %d %s\n", ret, gai_strerror(ret));
		return DAT_INVALID_ADDRESS;
	}

	ret = DAT_INVALID_ADDRESS;
	for (ai = res; ai; ai = ai->ai_next) {
		sin = (struct sockaddr_in *)ai->ai_addr;
		if (*((uint32_t *) & sin->sin_addr) != htonl(0x7f000001)) {
			*((struct sockaddr_in *)addr) = *sin;
			ret = DAT_SUCCESS;
			break;
		}
	}

	freeaddrinfo(res);

	/* only loopback found, retry netdev eth0 */
	if (ret == DAT_INVALID_ADDRESS) {
		netdev = "eth0";
		goto retry;
	}

	return ret;
}
#else				// _WIN64 || WIN32

/* Get IP address using network device name */
int getipaddr_netdev(char *name, char *addr, int addr_len)
{
	struct ifreq ifr;
	int skfd, ret, len;

	/* Fill in the structure */
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", name);

	/* Create a socket fd */
	skfd = socket(PF_INET, SOCK_STREAM, 0);
	ret = ioctl(skfd, SIOCGIFADDR, &ifr);
	if (ret)
		goto bail;

	switch (ifr.ifr_addr.sa_family) {
#ifdef	AF_INET6
	case AF_INET6:
		len = sizeof(struct sockaddr_in6);
		break;
#endif
	case AF_INET:
	default:
		len = sizeof(struct sockaddr);
		break;
	}

	if (len <= addr_len)
		memcpy(addr, &ifr.ifr_addr, len);
	else
		ret = EINVAL;

      bail:
	close(skfd);
	return ret;
}

/* IPv4 only, use IB if netdev set or it's the only interface */
DAT_RETURN getlocalipaddr (char *addr, int addr_len)
{
	struct ifaddrs *ifap, *ifa;
	int ret, found=0, ib_ok=0;
	char *netdev = getenv("DAPL_SCM_NETDEV");

	if (netdev != NULL) {
		ret = getipaddr_netdev(netdev, addr, addr_len);
		if (ret) {
			dapl_log(DAPL_DBG_TYPE_ERR, " ERR: NETDEV = %s"
				 " but not configured on system?\n", netdev);
			return dapl_convert_errno(errno, "getlocalipaddr");
		} else {
			dapl_log(DAPL_DBG_TYPE_UTIL," my_addr %s NETDEV = %s\n",
				 inet_ntoa(((struct sockaddr_in *)addr)->sin_addr),
				 netdev);
			return DAT_SUCCESS;
		}
	}

	if ((ret = getifaddrs (&ifap)))
		return dapl_convert_errno(errno, "getifaddrs");

retry:
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			if (!found && !(ifa->ifa_flags & IFF_LOOPBACK) &&
			    ((!ib_ok && dapl_os_pstrcmp("ib", ifa->ifa_name)) ||
			     (ib_ok && !dapl_os_pstrcmp("ib", ifa->ifa_name)))) {
				memcpy(addr, ifa->ifa_addr, sizeof(struct sockaddr_in));
				found++;
			}
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " getifaddrs: %s -> %s\n", ifa->ifa_name,
				 inet_ntoa(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr));
		}
	}
	if (!found && !ib_ok) {
		ib_ok = 1;
		goto retry;
	}
	dapl_log(DAPL_DBG_TYPE_UTIL," my_addr %s\n",
		 inet_ntoa(((struct sockaddr_in *)addr)->sin_addr));

	freeifaddrs(ifap);
	return (found ? DAT_SUCCESS:DAT_INVALID_ADDRESS);
}
#endif

enum ibv_mtu dapl_ib_mtu(int mtu)
{
	switch (mtu) {
	case 256:
		return IBV_MTU_256;
	case 512:
		return IBV_MTU_512;
	case 1024:
		return IBV_MTU_1024;
	case 2048:
		return IBV_MTU_2048;
	case 4096:
		return IBV_MTU_4096;
	default:
		return IBV_MTU_1024;
	}
}

const char *dapl_ib_mtu_str(enum ibv_mtu mtu)
{
	switch (mtu) {
	case IBV_MTU_256:
		return "256";
	case IBV_MTU_512:
		return "512";
	case IBV_MTU_1024:
		return "1024";
	case IBV_MTU_2048:
		return "2048";
	case IBV_MTU_4096:
		return "4096";
	default:
		return "1024";
	}
}

const char *dapl_ib_port_str(enum ibv_port_state state)
{
	switch (state) {
	case IBV_PORT_NOP:
		return "NOP";
	case IBV_PORT_DOWN:
		return "DOWN";
	case IBV_PORT_INIT:
		return "INIT";
	case IBV_PORT_ARMED:
		return "ARMED";
	case IBV_PORT_ACTIVE:
		return "ACTIVE";
	case IBV_PORT_ACTIVE_DEFER:
		return "DEFER";
	default:
		return "UNKNOWN";
	}
}

/*
 * dapls_ib_query_hca
 *
 * Query the hca attribute
 *
 * Input:
 *	hca_handl		hca handle	
 *	ia_attr			attribute of the ia
 *	ep_attr			attribute of the ep
 *	ip_addr			ip address of DET NIC
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 */

DAT_RETURN dapls_ib_query_hca(IN DAPL_HCA * hca_ptr,
			      OUT DAT_IA_ATTR * ia_attr,
			      OUT DAT_EP_ATTR * ep_attr,
			      OUT DAT_SOCK_ADDR6 * ip_addr)
{
	struct ibv_device_attr dev_attr;
	struct ibv_port_attr port_attr;

	/* local IP address of device, set during ia_open */
	if (ip_addr)
		memcpy(ip_addr, &hca_ptr->hca_address, sizeof(DAT_SOCK_ADDR6));

	if (ia_attr == NULL && ep_attr == NULL)
		return DAT_SUCCESS;

	if (ia_attr != NULL) /* setup address ptr, even with no device */
		ia_attr->ia_address_ptr = (DAT_IA_ADDRESS_PTR) &hca_ptr->hca_address;

	if (hca_ptr->ib_hca_handle == NULL) /* no open device, query mode */
		return DAT_SUCCESS;

	/* query verbs for this device and port attributes */
	if (ibv_query_device(hca_ptr->ib_hca_handle, &dev_attr) ||
	    ibv_query_port(hca_ptr->ib_hca_handle,
			   hca_ptr->port_num, &port_attr))
		return (dapl_convert_errno(errno, "ib_query_hca"));

	dev_attr.max_qp_wr = DAPL_MIN(dev_attr.max_qp_wr,
				      dapl_os_get_env_val("DAPL_WR_MAX", dev_attr.max_qp_wr));

#ifdef _OPENIB_MCM_
	/* Adjust for CCL Proxy; limited sge's, no READ support, reduce QP and RDMA limits */
	dev_attr.max_sge = DAPL_MIN(dev_attr.max_sge, DAT_MIX_SGE_MAX);
	dev_attr.max_qp_wr = DAPL_MIN(dev_attr.max_qp_wr,
				      dapl_os_get_env_val("DAPL_MCM_WR_MAX", DAT_MIX_WR_MAX));
	port_attr.max_msg_sz = DAPL_MIN(port_attr.max_msg_sz,
					dapl_os_get_env_val("DAPL_MCM_MSG_MAX", DAT_MIX_RDMA_MAX));
#endif

	if (ia_attr != NULL) {
		(void)dapl_os_memzero(ia_attr, sizeof(*ia_attr));
		strncpy(ia_attr->adapter_name,
		        ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
		        DAT_NAME_MAX_LENGTH - 1);
		ia_attr->adapter_name[DAT_NAME_MAX_LENGTH - 1] = '\0';
		ia_attr->vendor_name[DAT_NAME_MAX_LENGTH - 1] = '\0';
		ia_attr->ia_address_ptr = (DAT_IA_ADDRESS_PTR) &hca_ptr->hca_address;

		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: %s %s \n",
			     ia_attr->adapter_name,
			     inet_ntoa(((struct sockaddr_in *)
					&hca_ptr->hca_address)->sin_addr));

		ia_attr->hardware_version_major = dev_attr.hw_ver;
		/* ia_attr->hardware_version_minor   = dev_attr.fw_ver; */
		ia_attr->max_eps = dev_attr.max_qp;
		ia_attr->max_dto_per_ep = dev_attr.max_qp_wr;
		ia_attr->max_rdma_read_in = dev_attr.max_qp_rd_atom;
		ia_attr->max_rdma_read_out = dev_attr.max_qp_init_rd_atom;
		ia_attr->max_rdma_read_per_ep_in = dev_attr.max_qp_rd_atom;
		ia_attr->max_rdma_read_per_ep_out = dev_attr.max_qp_init_rd_atom;
		ia_attr->max_rdma_read_per_ep_in_guaranteed = DAT_TRUE;
		ia_attr->max_rdma_read_per_ep_out_guaranteed = DAT_TRUE;
		ia_attr->max_evds = dev_attr.max_cq;
		ia_attr->max_evd_qlen = dev_attr.max_cqe;
		ia_attr->max_iov_segments_per_dto = dev_attr.max_sge;
		ia_attr->max_lmrs = dev_attr.max_mr;
		/* 32bit attribute from 64bit, 4G-1 limit, DAT v2 needs fix */
		ia_attr->max_lmr_block_size = 
		    (dev_attr.max_mr_size >> 32) ? ~0 : dev_attr.max_mr_size;
		ia_attr->max_rmrs = dev_attr.max_mw;
		ia_attr->max_lmr_virtual_address = dev_attr.max_mr_size;
		ia_attr->max_rmr_target_address = dev_attr.max_mr_size;
		ia_attr->max_pzs = dev_attr.max_pd;
		ia_attr->max_message_size = port_attr.max_msg_sz;
		ia_attr->max_rdma_size = port_attr.max_msg_sz;
		/* iWARP spec. - 1 sge for RDMA reads */
		if (hca_ptr->ib_hca_handle->device->transport_type == IBV_TRANSPORT_IWARP)
			ia_attr->max_iov_segments_per_rdma_read = 1;
		else
			ia_attr->max_iov_segments_per_rdma_read = dev_attr.max_sge;
		ia_attr->max_iov_segments_per_rdma_write = dev_attr.max_sge;
		ia_attr->num_transport_attr = 0;
		ia_attr->transport_attr = NULL;
		ia_attr->num_vendor_attr = 0;
		ia_attr->vendor_attr = NULL;
#ifdef DAT_EXTENSIONS
		ia_attr->extension_supported = DAT_EXTENSION_IB;
		ia_attr->extension_version = DAT_IB_EXTENSION_VERSION;
#endif
		/* save key device attributes for CM exchange */
		hca_ptr->ib_trans.rd_atom_in  = dev_attr.max_qp_rd_atom;
		hca_ptr->ib_trans.rd_atom_out = dev_attr.max_qp_init_rd_atom;
		
		hca_ptr->ib_trans.mtu = DAPL_MIN(port_attr.active_mtu,
						 hca_ptr->ib_trans.mtu);
		hca_ptr->ib_trans.ack_timer =
		    DAPL_MAX(dev_attr.local_ca_ack_delay,
			     hca_ptr->ib_trans.ack_timer);

		/* set provider/transport specific named attributes */
		hca_ptr->ib_trans.na.dev = ia_attr->adapter_name;
		hca_ptr->ib_trans.na.mtu = dapl_ib_mtu_str(hca_ptr->ib_trans.mtu);
		hca_ptr->ib_trans.na.port = dapl_ib_port_str(port_attr.state);
		hca_ptr->ib_trans.guid = ntohll(ibv_get_device_guid(hca_ptr->ib_trans.ib_dev));
		sprintf(hca_ptr->ib_trans.guid_str, "%04x:%04x:%04x:%04x",
			(unsigned) (hca_ptr->ib_trans.guid >> 48) & 0xffff,
			(unsigned) (hca_ptr->ib_trans.guid >> 32) & 0xffff,
			(unsigned) (hca_ptr->ib_trans.guid >> 16) & 0xffff,
			(unsigned) (hca_ptr->ib_trans.guid >>  0) & 0xffff);
#ifdef _OPENIB_MCM_
		hca_ptr->ib_trans.sys_guid = dev_attr.sys_image_guid; /* network order */
		if (hca_ptr->ib_trans.self.node)
			hca_ptr->ib_trans.na.mode = "PROXY";
		else
			hca_ptr->ib_trans.na.mode = "DIRECT";
		hca_ptr->ib_trans.na.read = "FALSE";
#else
		hca_ptr->ib_trans.na.mode = "DIRECT";
		hca_ptr->ib_trans.na.read = "TRUE";
#endif
		if (hca_ptr->ib_hca_handle->device->transport_type != IBV_TRANSPORT_IB)
			goto skip_ib;

               /* set SL, PKEY values, defaults = 0 */
               hca_ptr->ib_trans.pkey_idx = 0;
               hca_ptr->ib_trans.pkey = htons(dapl_os_get_env_val("DAPL_IB_PKEY", 0));
               hca_ptr->ib_trans.sl = dapl_os_get_env_val("DAPL_IB_SL", 0);

		/* index provided, get pkey; pkey provided, get index */
		if (hca_ptr->ib_trans.pkey) {
			int i; uint16_t pkey = 0;
			for (i=0; i < dev_attr.max_pkeys; i++) {
				if (ibv_query_pkey(hca_ptr->ib_hca_handle,
						   hca_ptr->port_num,
						   i, &pkey)) {
					i = dev_attr.max_pkeys;
					break;
				}
				if (pkey == hca_ptr->ib_trans.pkey) {
					hca_ptr->ib_trans.pkey_idx = i;
					break;
				}
			}
			if (i == dev_attr.max_pkeys) {
				dapl_log(DAPL_DBG_TYPE_ERR,
					 " ERR: new pkey(0x%x), query (%s)"
					 " err or key !found, using default pkey_idx=0\n",
					 ntohs(hca_ptr->ib_trans.pkey), strerror(errno));
			}
		}
skip_ib:

#ifdef DEFINE_ATTR_LINK_LAYER
#ifndef _OPENIB_CMA_
		if (port_attr.link_layer == IBV_LINK_LAYER_ETHERNET)
			hca_ptr->ib_trans.global = 1;

		dapl_log(DAPL_DBG_TYPE_UTIL,
			 " query_hca: port.link_layer = 0x%x\n", 
			 port_attr.link_layer);
#endif
#endif

#ifdef _WIN32
#ifndef _OPENIB_CMA_
		if (port_attr.transport != IBV_TRANSPORT_IB)
			hca_ptr->ib_trans.global = 1;

		dapl_log(DAPL_DBG_TYPE_UTIL,
			 " query_hca: port.transport %d ib_trans.global %d\n",
			 port_attr.transport, hca_ptr->ib_trans.global);
#endif
#endif
		dapl_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: (%x.%x) eps %d, sz %d evds %d,"
			     " sz %d mtu %d - pkey %x p_idx %d sl %d\n",
			     ia_attr->hardware_version_major,
			     ia_attr->hardware_version_minor,
			     ia_attr->max_eps, ia_attr->max_dto_per_ep,
			     ia_attr->max_evds, ia_attr->max_evd_qlen,
			     128 << hca_ptr->ib_trans.mtu,
			     ntohs(hca_ptr->ib_trans.pkey),
			     hca_ptr->ib_trans.pkey_idx,
			     hca_ptr->ib_trans.sl);

		dapl_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: msg %llu rdma %llu iov %d lmr %d rmr %d"
			     " ack_time %d mr %u ia_addr_ptr %p\n",
			     ia_attr->max_message_size, ia_attr->max_rdma_size,
			     ia_attr->max_iov_segments_per_dto,
			     ia_attr->max_lmrs, ia_attr->max_rmrs,
			     hca_ptr->ib_trans.ack_timer,
			     ia_attr->max_lmr_block_size,
			     ia_attr->ia_address_ptr);
	}

	if (ep_attr != NULL) {
		(void)dapl_os_memzero(ep_attr, sizeof(*ep_attr));
		ep_attr->max_message_size = port_attr.max_msg_sz;
		ep_attr->max_rdma_size = port_attr.max_msg_sz;
		ep_attr->max_recv_dtos = dev_attr.max_qp_wr;
		ep_attr->max_request_dtos = dev_attr.max_qp_wr;
		ep_attr->max_recv_iov = dev_attr.max_sge;
		ep_attr->max_request_iov = dev_attr.max_sge;
		ep_attr->max_rdma_read_in = dev_attr.max_qp_rd_atom;
		ep_attr->max_rdma_read_out = dev_attr.max_qp_init_rd_atom;
		ep_attr->max_rdma_read_iov = dev_attr.max_sge;
		ep_attr->max_rdma_write_iov = dev_attr.max_sge;
		dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
			     " query_hca: MAX msg %llu mtu %d qsz %d iov %d"
			     " rdma i%d,o%d\n",
			     ep_attr->max_message_size,
			     128 << hca_ptr->ib_trans.mtu,
			     ep_attr->max_recv_dtos, 
			     ep_attr->max_recv_iov,
			     ep_attr->max_rdma_read_in,
			     ep_attr->max_rdma_read_out);
	}
	return DAT_SUCCESS;
}

/*
 * dapls_ib_setup_async_callback
 *
 * Set up an asynchronous callbacks of various kinds
 *
 * Input:
 *	ia_handle		IA handle
 *	handler_type		type of handler to set up
 *	callback_handle 	handle param for completion callbacks
 *	callback		callback routine pointer
 *	context 		argument for callback routine
 *
 * Output:
 *	none
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *
 */
DAT_RETURN dapls_ib_setup_async_callback(IN DAPL_IA * ia_ptr,
					 IN DAPL_ASYNC_HANDLER_TYPE
					 handler_type, IN DAPL_EVD * evd_ptr,
					 IN ib_async_handler_t callback,
					 IN void *context)
{
	ib_hca_transport_t *hca_ptr;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " setup_async_cb: ia %p type %d handle %p cb %p ctx %p\n",
		     ia_ptr, handler_type, evd_ptr, callback, context);

	hca_ptr = &ia_ptr->hca_ptr->ib_trans;
	switch (handler_type) {
	case DAPL_ASYNC_UNAFILIATED:
		hca_ptr->async_unafiliated = (ib_async_handler_t) callback;
		hca_ptr->async_un_ctx = context;
		break;
	case DAPL_ASYNC_CQ_ERROR:
		hca_ptr->async_cq_error = (ib_async_cq_handler_t) callback;
		break;
	case DAPL_ASYNC_CQ_COMPLETION:
		hca_ptr->async_cq = (ib_async_dto_handler_t) callback;
		break;
	case DAPL_ASYNC_QP_ERROR:
		hca_ptr->async_qp_error = (ib_async_qp_handler_t) callback;
		break;
	default:
		break;
	}
	return DAT_SUCCESS;
}

void dapli_async_event_cb(struct _ib_hca_transport *hca)
{
	struct ibv_async_event event;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " async_event(%p)\n", hca);

	if (hca->destroy)
		return;

	if (!ibv_get_async_event(hca->ib_ctx, &event)) {

		switch (event.event_type) {
		case IBV_EVENT_CQ_ERR:
		{
			struct dapl_evd *evd_ptr =
				event.element.cq->cq_context;

			dapl_log(DAPL_DBG_TYPE_ERR,
				 "dapl async_event CQ (%p) ERR %d\n",
				 evd_ptr, event.event_type);

			/* report up if async callback still setup */
			if (hca->async_cq_error)
				hca->async_cq_error(hca->ib_ctx,
						    evd_ptr->ib_cq_handle,
						    &event,
						    (void *)evd_ptr);
			break;
		}
		case IBV_EVENT_COMM_EST:
		{
			/* Received msgs on connected QP before RTU */
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " async_event COMM_EST(%p) rdata beat RTU\n",
				 event.element.qp);

			break;
		}
		case IBV_EVENT_QP_FATAL:
		case IBV_EVENT_QP_REQ_ERR:
		case IBV_EVENT_QP_ACCESS_ERR:
		case IBV_EVENT_QP_LAST_WQE_REACHED:
		case IBV_EVENT_SRQ_ERR:
		case IBV_EVENT_SRQ_LIMIT_REACHED:
		case IBV_EVENT_SQ_DRAINED:
		{
			struct dapl_ep *ep_ptr =
				event.element.qp->qp_context;

			dapl_log(DAPL_DBG_TYPE_ERR,
				 "dapl async_event QP (%p) ERR %d\n",
				 ep_ptr, event.event_type);

			/* report up if async callback still setup */
			if (hca->async_qp_error)
				hca->async_qp_error(hca->ib_ctx,
						    ep_ptr->qp_handle,
						    &event,
						    (void *)ep_ptr);
			break;
		}
		case IBV_EVENT_PATH_MIG:
		case IBV_EVENT_PATH_MIG_ERR:
		case IBV_EVENT_DEVICE_FATAL:
		case IBV_EVENT_PORT_ACTIVE:
		case IBV_EVENT_PORT_ERR:
		case IBV_EVENT_LID_CHANGE:
		case IBV_EVENT_PKEY_CHANGE:
		case IBV_EVENT_SM_CHANGE:
		{
			dapl_log(DAPL_DBG_TYPE_WARN,
				 "dapl async_event: DEV ERR %d\n",
				 event.event_type);

			/* report up if async callback still setup */
			if (hca->async_unafiliated)
				hca->async_unafiliated(hca->ib_ctx, 
						       &event,	
						       hca->async_un_ctx);
			break;
		}
		case IBV_EVENT_CLIENT_REREGISTER:
			/* no need to report this event this time */
			dapl_log(DAPL_DBG_TYPE_WARN,
				 " WARNING: IBV_CLIENT_REREGISTER\n");
			break;

		default:
			dapl_log(DAPL_DBG_TYPE_WARN,
				 "dapl async_event: %d UNKNOWN\n",
				 event.event_type);
			break;

		}
		ibv_ack_async_event(&event);
	}
}

/*
 * dapls_query_provider_specific_attrs
 *
 * Common for openib providers: cma, ucm, scm, mcm
 *
 * Input:
 *      attr_ptr        Pointer provider specific attributes
 *
 * Output:
 *      none
 *
 * Returns:
 *      void
 */
DAT_NAMED_ATTR ib_attrs[] = {
	{
	 "DAT_IB_PROVIDER_NAME", PROVIDER_NAME}
	,
	{
	 "DAT_IB_DEVICE_NAME", "OFA_HCA_0000"}
	,
	{
	 "DAT_IB_CONNECTIVITY_MODE", "DIRECT"}
	,
	{
	 "DAT_IB_RDMA_READ", "TRUE"}
	,
	{
	 "DAT_IB_NODE_GUID", "xxxx:xxxx:xxxx:xxxx"}
	,
	{
	 "DAT_IB_TRANSPORT_MTU", "2048"}
	,
	{
	 "DAT_IB_PORT_STATUS", "UNKNOWN"}
	,
#ifdef DAT_EXTENSIONS
	{
	 "DAT_EXTENSION_INTERFACE", "TRUE"}
	,
#ifndef _OPENIB_MCM_
	{
	 DAT_IB_ATTR_FETCH_AND_ADD, "TRUE"}
	,
	{
	 DAT_IB_ATTR_CMP_AND_SWAP, "TRUE"}
	,
#endif
	{
	 DAT_IB_ATTR_IMMED_DATA, "TRUE"}
	,
#ifdef DAT_IB_COLLECTIVES
	{
	 DAT_IB_COLL_BARRIER, "TRUE"}
	,
	{
	 DAT_IB_COLL_BROADCAST, "TRUE"}
	,
	{
	 DAT_IB_COLL_REDUCE, "TRUE"}
	,
	{
	 DAT_IB_COLL_ALLREDUCE, "TRUE"}
	,
	{
	 DAT_IB_COLL_ALLGATHER, "TRUE"}
	,
	{
	 DAT_IB_COLL_ALLGATHERV, "TRUE"}
	,
#endif /* DAT_IB_COLLECTIVES */
#if !defined(_OPENIB_CMA_) && !defined(_OPENIB_MCM_)
	{
	 DAT_IB_ATTR_UD, "TRUE"}
	,
#endif
#ifdef DAPL_COUNTERS
	{
	 DAT_ATTR_COUNTERS, "TRUE"}
	,
#endif				/* DAPL_COUNTERS */
#endif
};

#define SPEC_ATTR_SIZE( x )     (sizeof( x ) / sizeof( DAT_NAMED_ATTR))

void dapls_query_provider_specific_attr(IN DAPL_IA * ia_ptr,
					IN DAT_PROVIDER_ATTR * attr_ptr)
{

	attr_ptr->num_provider_specific_attr = SPEC_ATTR_SIZE(ib_attrs);
	attr_ptr->provider_specific_attr = ib_attrs;

	dapl_log(DAPL_DBG_TYPE_UTIL,
		 " prov_attr: %p sz %d\n", ib_attrs, SPEC_ATTR_SIZE(ib_attrs));

	/* update common attributes from providers */
	ib_attrs[1].value = ia_ptr->hca_ptr->ib_trans.na.dev;
	ib_attrs[2].value = ia_ptr->hca_ptr->ib_trans.na.mode;
	ib_attrs[3].value = ia_ptr->hca_ptr->ib_trans.na.read;
	ib_attrs[4].value = ia_ptr->hca_ptr->ib_trans.guid_str;
	ib_attrs[5].value = ia_ptr->hca_ptr->ib_trans.na.mtu;
	ib_attrs[6].value = ia_ptr->hca_ptr->ib_trans.na.port;

}

/*
 * Map all socket CM event codes to the DAT equivelent. Common to all providers
 */
#define DAPL_IB_EVENT_CNT	13

static struct ib_cm_event_map {
	const ib_cm_events_t ib_cm_event;
	DAT_EVENT_NUMBER dat_event_num;
} ib_cm_event_map[DAPL_IB_EVENT_CNT] = {
/* 00 */ {IB_CME_CONNECTED, 
	  DAT_CONNECTION_EVENT_ESTABLISHED},
/* 01 */ {IB_CME_DISCONNECTED, 
	  DAT_CONNECTION_EVENT_DISCONNECTED},
/* 02 */ {IB_CME_DISCONNECTED_ON_LINK_DOWN,
	  DAT_CONNECTION_EVENT_DISCONNECTED},
/* 03 */ {IB_CME_CONNECTION_REQUEST_PENDING, 
	  DAT_CONNECTION_REQUEST_EVENT},
/* 04 */ {IB_CME_CONNECTION_REQUEST_PENDING_PRIVATE_DATA,
	  DAT_CONNECTION_REQUEST_EVENT},
/* 05 */ {IB_CME_CONNECTION_REQUEST_ACKED,
	  DAT_CONNECTION_EVENT_ESTABLISHED},
/* 06 */ {IB_CME_DESTINATION_REJECT,
	  DAT_CONNECTION_EVENT_NON_PEER_REJECTED},
/* 07 */ {IB_CME_DESTINATION_REJECT_PRIVATE_DATA,
	  DAT_CONNECTION_EVENT_PEER_REJECTED},
/* 08 */ {IB_CME_DESTINATION_UNREACHABLE, 
	  DAT_CONNECTION_EVENT_UNREACHABLE},
/* 09 */ {IB_CME_TOO_MANY_CONNECTION_REQUESTS,
	  DAT_CONNECTION_EVENT_NON_PEER_REJECTED},
/* 10 */ {IB_CME_BROKEN, 
	  DAT_CONNECTION_EVENT_BROKEN},
/* 11 */ {IB_CME_TIMEOUT, 
	  DAT_CONNECTION_EVENT_TIMED_OUT},
/* 12 */ {IB_CME_LOCAL_FAILURE,		/* always last */
	  DAT_CONNECTION_EVENT_BROKEN}
};

/*
 * dapls_ib_get_cm_event
 *
 * Return a DAT connection event given a provider CM event.
 *
 * Input:
 *	dat_event_num	DAT event we need an equivelent CM event for
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	ib_cm_event of translated DAPL value
 */
DAT_EVENT_NUMBER
dapls_ib_get_dat_event(IN const ib_cm_events_t ib_cm_event,
		       IN DAT_BOOLEAN active)
{
	DAT_EVENT_NUMBER dat_event_num;
	int i;

	active = active;

	if (ib_cm_event > IB_CME_LOCAL_FAILURE)
		return (DAT_EVENT_NUMBER) 0;

	dat_event_num = 0;
	for (i = 0; i < DAPL_IB_EVENT_CNT; i++) {
		if (ib_cm_event == ib_cm_event_map[i].ib_cm_event) {
			dat_event_num = ib_cm_event_map[i].dat_event_num;
			break;
		}
	}
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "dapls_ib_get_dat_event: event translate(%s) ib=0x%x dat=0x%x\n",
		     active ? "active" : "passive", ib_cm_event, dat_event_num);

	return dat_event_num;
}

/*
 * dapls_ib_get_dat_event
 *
 * Return a DAT connection event given a provider CM event.
 * 
 * Input:
 *	ib_cm_event	event provided to the dapl callback routine
 *	active		switch indicating active or passive connection
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_EVENT_NUMBER of translated provider value
 */
ib_cm_events_t dapls_ib_get_cm_event(IN DAT_EVENT_NUMBER dat_event_num)
{
	ib_cm_events_t ib_cm_event;
	int i;

	ib_cm_event = 0;
	for (i = 0; i < DAPL_IB_EVENT_CNT; i++) {
		if (dat_event_num == ib_cm_event_map[i].dat_event_num) {
			ib_cm_event = ib_cm_event_map[i].ib_cm_event;
			break;
		}
	}
	return ib_cm_event;
}

