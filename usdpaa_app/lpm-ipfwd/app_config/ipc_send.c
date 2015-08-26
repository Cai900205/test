/**
 \file ipc_send.c
 \brief Basic IPfwd Config Tool
 */
/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <usdpaa/of.h>

#include <usdpaa/compat.h>

#include "ipc_send.h"
#include "ip/ip_appconf.h"

#include <mqueue.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>

unsigned int g_mndtr_param;
error_t g_parse_error;
mqd_t mq_fd_wr, mq_fd_rd;

static error_t parse_opt(int key, char *arg, struct argp_state *state);
static error_t parse_route_add_opt(int key, char *arg,
				   struct argp_state *state);
static error_t parse_arp_add_opt(int key, char *arg, struct argp_state *state);
static error_t parse_intf_conf_opt(int key, char *arg,
				struct argp_state *state);
static error_t parse_show_intf_opt(int key, char *arg,
				struct argp_state *state);
static struct argp route_add_argp = {
	route_add_options, parse_route_add_opt, NULL, NULL, NULL, NULL, NULL };

static struct argp route_del_argp = {
	route_del_options, parse_route_add_opt, NULL, NULL, NULL, NULL, NULL };

static struct argp arp_add_argp = {
	arp_add_options, parse_arp_add_opt, NULL, NULL, NULL, NULL, NULL };

static struct argp arp_del_argp = {
	arp_del_options, parse_arp_add_opt, NULL, NULL, NULL, NULL, NULL };

static struct argp intf_conf_argp = {
	intf_conf_options, parse_intf_conf_opt, NULL, NULL, NULL, NULL, NULL };

static struct argp show_intf_argp = {
	show_intf_options, parse_show_intf_opt, NULL, NULL, NULL, NULL, NULL };

static struct argp_option options[] = {
	{"routeadd", 'B', "TYPE", 0, "adding a route", 0},
	{"routedel", 'C', "TYPE", 0, "deleting a route", 0},
	{"arpadd", 'G', "TYPE", 0, "adding a arp entry", 0},
	{"arpdel", 'H', "TYPE", 0, "deleting a arp entry", 0},
	{"intfconf", 'F', "TYPE", 0, "change intf config", 0},
	{"showintf", 'E', "TYPE", 0, "show interfaces", 0},
	{"PID", 'P', "TYPE", 0, "pid to hook with (Mandatory param)", 0},
	{}
};

/*
   The ARGP structure itself.
*/

static struct argp argp = {
	options, parse_opt, NULL, NULL, NULL, NULL, NULL };

/**
 \brief Sends data from the saInfo structure to the message queue.
 \param[in] saInfo Pointer to the structure that needs to be sent
 to the message queue
 \return none
 */
void send_to_mq(struct app_ctrl_op_info *saInfo)
{
	int ret;

	saInfo->state = IPC_CTRL_CMD_STATE_BUSY;
	/* Send message to message queue */
	ret = mq_send(mq_fd_wr, (char *)saInfo, sizeof(*saInfo), 10);
	if (ret != 0)
		pr_err("%s : Error in sending mesage on MQ\n", __FILE__);
}
/**
 \brief Processes the Route Add/ Delete Request
 \param[in] argc Number of arguments
 \param[in] argv Arguments
 \param[in] type Message Type for the CP request - Route Add or delete
 \return none
 */
void ipc_add_del_command(int argc, char **argv, unsigned type)
{
	unsigned int i = 0;
	struct app_ctrl_op_info route_info;
	unsigned int mndtr_param_map[] = { IPC_CTRL_ROUTE_ADD_MDTR_PARAM_MAP,
		IPC_CTRL_ROUTE_DEL_MDTR_PARAM_MAP
	};
	struct argp *route_argp[] = { &route_add_argp, &route_del_argp };

	/*
	 ** Initializing the route info structure
	 */
	memset(&route_info, 0, sizeof(route_info));
	route_info.state = IPC_CTRL_CMD_STATE_IDLE;
	route_info.msg_type = type;
	route_info.result = IPC_CTRL_RSLT_FAILURE;

	g_mndtr_param = 0;
	g_parse_error = 0;

	/* Where the magic happens */
	argp_parse(route_argp
		   [route_info.msg_type - IPC_CTRL_CMD_TYPE_ROUTE_ADD], argc,
		   argv, 0, 0, &route_info);

	if (g_parse_error != 0)
		return;

	/*
	 ** If all the mandatory parameters for the operation are present,
	 ** send the data to message queue
	 */
	if ((g_mndtr_param &
	     mndtr_param_map[route_info.msg_type -
			     IPC_CTRL_CMD_TYPE_ROUTE_ADD]) ==
	    mndtr_param_map[route_info.msg_type - IPC_CTRL_CMD_TYPE_ROUTE_ADD])
		goto copy;

	/*
	 ** Check for the mandatory parameter which is misisng
	 */
	for (i = 0; i < IPC_CTRL_PARAM_MAX_IP_BIT_NO; i++) {
		if (((mndtr_param_map
		      [route_info.msg_type -
		       IPC_CTRL_CMD_TYPE_ROUTE_ADD] & (1 << i)) != 0)
		    && ((g_mndtr_param & (1 << i)) == 0)) {
			pr_err
			    ("Route fail mandatory param missing;--%s or -%c\n",
			     route_add_options[i].name,
			     route_add_options[i].key);
			return;
		}
	} /* end of for (i = 0; i < IPC_CTRL_PARAM_MAX_B .... */

copy:
	send_to_mq(&route_info);

	return;
}

/**
 \brief Processes the ARP Add/ Delete Request
 \param[in] argc Number of arguments
 \param[in] argv Arguments
 \param[in] type Message Type for the CP request - ARP Add or delete
 \return none
 */
void ipc_arp_add_del_command(int argc, char **argv, unsigned type)
{
	unsigned int i = 0;
	struct app_ctrl_op_info route_info;
	unsigned int mndtr_param_map[] = { IPC_CTRL_ARP_ADD_MDTR_PARAM_MAP,
		IPC_CTRL_ARP_DEL_MDTR_PARAM_MAP
	};
	struct argp *route_argp[] = { &arp_add_argp, &arp_del_argp };

	pr_debug("\r\n%s: Enter", __func__);
	/*
	 ** Initializing the route info structure
	 */
	memset(&route_info, 0, sizeof(route_info));
	route_info.state = IPC_CTRL_CMD_STATE_IDLE;
	route_info.msg_type = type;
	route_info.result = IPC_CTRL_RSLT_FAILURE;

	g_mndtr_param = 0;

	if (route_info.msg_type == IPC_CTRL_CMD_TYPE_ARP_ADD)
		route_info.ip_info.replace_entry = 0;

	/* Where the magic happens */
	argp_parse(route_argp[route_info.msg_type - IPC_CTRL_CMD_TYPE_ARP_ADD],
		   argc, argv, 0, 0, &route_info);

	/*
	 ** If all the mandatory parameters for the operation are present,
	 ** send the data to message queue
	 */
	if ((g_mndtr_param &
	     mndtr_param_map[route_info.msg_type -
			     IPC_CTRL_CMD_TYPE_ARP_ADD]) ==
	    mndtr_param_map[route_info.msg_type - IPC_CTRL_CMD_TYPE_ARP_ADD])
		goto copy;

	/*
	 ** Check for the mandatory parameter which is misisng
	 */
	for (i = 0; i < IPC_CTRL_PARAM_ARP_MAX_BIT_NO; i++) {
		if (((mndtr_param_map
		      [route_info.msg_type -
		       IPC_CTRL_CMD_TYPE_ARP_ADD] & (1 << i)) != 0)
		    && ((g_mndtr_param & (1 << i)) == 0)) {
			pr_err
			    ("ARP fail mandatory param missing;--%s or -%c\n",
			     arp_add_options[i].name, arp_add_options[i].key);
			return;
		}
	} /* end of for (i = 0; i < IPC_CTRL_PARAM_MAX_B .... */

copy:
	send_to_mq(&route_info);

	pr_debug("\r\n%s: Exit", __func__);
	return;
}

/**
 \brief Processes the IP Interface Config Request
 \param[in] argc Number of arguments
 \param[in] argv Arguments
 \param[in] type Message Type for the CP request - IP Intf Config
 \return none
 */
void ipc_ip_intf_chng_command(int argc, char **argv, char *type)
{
	unsigned int i = 0;
	struct app_ctrl_op_info route_info;
	unsigned int mndtr_param_map = IPC_CTRL_INTF_CONF_MDTR_PARAM_MAP;
	struct argp *route_argp = &intf_conf_argp;

	pr_debug("\r\n%s: Enter", __func__);

	/*
	 ** Initializing the route info structure
	 */
	memset(&route_info, 0, sizeof(route_info));
	route_info.state = IPC_CTRL_CMD_STATE_IDLE;
	route_info.msg_type = (typeof(route_info.msg_type))(uintptr_t)type;
	route_info.result = IPC_CTRL_RSLT_FAILURE;

	g_mndtr_param = 0;
	g_parse_error = 0;

	/* Where the magic happens */
	argp_parse(route_argp, argc, argv, 0, 0, &route_info);

	if (g_parse_error != 0)
		return;
	/*
	 ** If all the mandatory parameters for the operation are present,
	 ** Copy the data onto the shared memory area
	 */
	if ((g_mndtr_param & mndtr_param_map) == mndtr_param_map)
		goto copy;

	/*
	 ** Check for the mandatory parameter which is misisng
	 */
	for (i = 0; i < IPC_CTRL_PARAM_MAX_INTF_BIT_NO; i++) {
		if (((mndtr_param_map & (1 << i)) != 0)
		    && ((g_mndtr_param & (1 << i)) == 0)) {
			printf
			    ("Interface Config Operation failed as mandatory"
				"parameters missing; --%s or -%c\n",
			     route_add_options[i].name,
			     route_add_options[i].key);
			return;
		}
	}	/* end of for (i = 0; i < IPC_CTRL_PARAM_MAX_B .... */

copy:
	send_to_mq(&route_info);

	pr_debug("\r\n%s: Exit", __func__);
	return;
}

/**
 \brief Processes the IP Interface Show Request
 \param[in] argc Number of arguments
 \param[in] argv Arguments
 \param[in] type Message Type for the CP request - IP Show Config
 \return none
 */
void ipc_show_intf_command(int argc, char **argv, char *type)
{
	struct app_ctrl_op_info route_info;
	struct argp *route_argp = &show_intf_argp;

	pr_debug("\r\n%s: Enter", __func__);

	/*
	 ** Initializing the route info structure
	 */
	memset(&route_info, 0, sizeof(route_info));
	route_info.state = IPC_CTRL_CMD_STATE_IDLE;
	route_info.msg_type = (typeof(route_info.msg_type))(uintptr_t)type;
	route_info.result = IPC_CTRL_RSLT_FAILURE;


	/* Where the magic happens */
	argp_parse(route_argp, argc, argv, 0, 0, &route_info);

	send_to_mq(&route_info);

	pr_debug("\r\n%s: Exit", __func__);
	return;
}

/* opens message queue to talk to the application */
static int create_mq(int pid)
{
	char name[10];

	sprintf(name, "/mq_rcv_%d", pid);
	/* Opens message queue to write */
	mq_fd_wr = mq_open(name,  O_WRONLY);
	if (mq_fd_wr == -1) {
		pr_err("SND mq err in opening the msgque errno\n");
		return -1;
	}

	sprintf(name, "/mq_snd_%d", pid);
	/* Opens message queue to read */
	mq_fd_rd = mq_open(name, O_RDONLY);
	if (mq_fd_rd == -1) {
		pr_err("RX mq err in opening the msgque errno\n");
		return -1;
	}

	return 0;
}

/**
 \brief Defines actions for parsing the ipfwd command options - add/ delete;
	it is called for each option parsed
 \param[in] key For each option that is parsed, parser is called with a value of
		key from that option's key field in the option vector
 \param[in] arg If key is an option, arg is its given value.
 \param[in] state state points to a struct argp_state, containing pointer to
 sa_info structure
 \return 0 for success, ARGP_ERR_UNKNOWN if the value of key is not handled by
 this parser function
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	int ret;
	struct app_ctrl_op_info *sa_info = state->input;

	switch (key) {
		/* Request for Route Entry Addition */
	case 'B':
		sa_info->msg_type = IPC_CTRL_CMD_TYPE_ROUTE_ADD;
		pr_debug
		    ("\n::FILE:%s : LINE:%d :IN PARSE_OPT::ADD OPTION SELECTED",
		     __FILE__, __LINE__);
		break;

		/* Request for route Entry Deletion */
	case 'C':
		sa_info->msg_type = IPC_CTRL_CMD_TYPE_ROUTE_DEL;
		pr_debug
		    ("\nFILE:%s  LINE:%d :IN PARSE_OPT::DELETE OPTION SELECTED",
		     __FILE__, __LINE__);
		break;

	case 'G':
		sa_info->msg_type = IPC_CTRL_CMD_TYPE_ARP_ADD;
		pr_debug
		    ("\nFILE:%s :LINE:%d :IN PARSE_OPT::ARP ADD OPT SELECTED",
		     __FILE__, __LINE__);
		break;

	case 'H':
		sa_info->msg_type = IPC_CTRL_CMD_TYPE_ARP_DEL;
		pr_debug
		    ("\nFILE:%s : LINE: %d :IN PARSE_OPT::ARP DEL OPT SELECTED",
		     __FILE__, __LINE__);
		break;

	case 'F':
		sa_info->msg_type = IPC_CTRL_CMD_TYPE_INTF_CONF_CHNG;
		pr_debug
		    ("\nFILE: %s : LINE: %d :IN PARSE_OPT::"
			"INTF CONF CHNG OPTION SELECTED",
			__FILE__, __LINE__);
		break;

	case 'E':
		sa_info->msg_type = IPC_CTRL_CMD_TYPE_SHOW_INTF;
		pr_debug
		    ("\nFILE: %s : LINE: %d :IN PARSE_OPT::"
			"SHOW INTF OPTION SELECTED",
			__FILE__, __LINE__);
		break;

	case 'P':
		sa_info->pid = atoi(arg);
		ret = create_mq(atoi(arg));
		if (ret == -1)
			argp_usage(state);
		return 0;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return ARGP_KEY_END;
}

/**
 \brief Defines actions for parsing the add/ delete command options;
	it is called for each option parsed
 \param[in] key For each option that is parsed, parser is called with a value of
		key from that option's key field in the option vector
 \param[in] arg If key is an option, arg is its given value.
 \param[in] state state points to a struct argp_state, containing pointer to
 route_info structure
 \return 0 for success, ARGP_ERR_UNKNOWN if the value of key is not handled by
 this parser function
	ARG_KEY_ERROR if the interface name is too long
 */
static error_t parse_route_add_opt(int key, char *arg, struct argp_state *state)
{
	struct app_ctrl_op_info *route_info = state->input;
	struct in_addr in_addr;

	switch (key) {

	case 'c':
		if ((atoi(arg) < 1) ||
		   (atoi(arg) > (2 * 1024 * 1024))) {
			printf("Invalid Value \"%s\" for '%c'\n", arg, key);
			g_parse_error = ERANGE;
			return -ERANGE;
		}
		g_mndtr_param |= LWE_CTRL_PARAM_BMASK_FIBCNT;
		route_info->ip_info.fib_cnt = atoi(arg);
		break;

	case 'd':
		inet_aton(arg, &in_addr);
		route_info->ip_info.dst_ipaddr = in_addr.s_addr;
		g_mndtr_param |= IPC_CTRL_PARAM_BMASK_DESTIP;
		pr_debug("\nkey = %c; value = %s", key, arg);
		break;

	case 'n':
		if ((atoi(arg) < 0) ||
		   (atoi(arg) > 32)) {
			printf("Invalid Value \"%s\" for '%c'\n", arg, key);
			g_parse_error = ERANGE;
			return -ERANGE;
		}
		g_mndtr_param |= LWE_CTRL_PARAM_BMASK_NETMASK;
		route_info->ip_info.mask = atoi(arg);
		break;

	case 'g':

		inet_aton(arg, &in_addr);
		route_info->ip_info.gw_ipaddr = in_addr.s_addr;
		g_mndtr_param |= IPC_CTRL_PARAM_BMASK_GWIP;
		pr_debug("\nkey = %c; value = %s", key, arg);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/**
 \brief Defines actions for parsing the add/ delete command options;
	it is called for each option parsed
 \param[in] key For each option that is parsed, parser is called with a value of
		key from that option's key field in the option vector
 \param[in] arg If key is an option, arg is its given value.
 \param[in] state state points to a struct argp_state, containing pointer to
 route_info structure
 \return 0 for success, ARGP_ERR_UNKNOWN if the value of key is not handled by
 this parser function
 */
static error_t parse_arp_add_opt(int key, char *arg, struct argp_state *state)
{
	struct app_ctrl_op_info *route_info = state->input;
	struct in_addr in_addr;

	switch (key) {

	case 's':
		inet_aton(arg, &in_addr);
		route_info->ip_info.src_ipaddr = in_addr.s_addr;
		g_mndtr_param |= IPC_CTRL_PARAM_BMASK_ARP_IPADDR;
		break;

	case 'm':
		{
			char *pch;
			uint32_t l = 0;
			uint32_t i = 0;
			pch = strtok(arg, ":");
			while (pch != NULL) {
				sscanf(pch, "%x", &l);
				route_info->ip_info.mac_addr.ether_addr_octet[i]
					= (uint8_t)l;
				pch = strtok(NULL, ":");
				i++;
			}
			g_mndtr_param |= IPC_CTRL_PARAM_BMASK_ARP_MACADDR;
			break;
		}

	case 'r':
		if (strcmp(arg, "true") == 0)
			route_info->ip_info.replace_entry = 1;
		else
			route_info->ip_info.replace_entry = 0;
		g_mndtr_param |= IPC_CTRL_PARAM_BMASK_ARP_REPLACE;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/**
 \brief Defines actions for parsing the interface config command options;
	it is called for each option parsed
 \param[in] key For each option that is parsed, parser is called with a value of
		key from that option's key field in the option vector
 \param[in] arg If key is an option, arg is its given value.
 \param[in] state state points to a struct argp_state, containing pointer to
route_info structure
 \return 0 for success, ARGP_ERR_UNKNOWN if the value of key is not handled by
 this parser function
		ARG_KEY_ERROR if the interface name is too long
 */
static error_t parse_intf_conf_opt(int key, char *arg, struct argp_state *state)
{
	struct app_ctrl_op_info *route_info = state->input;
	struct in_addr in_addr;
	int val;
	const char *name;
	const char *str = "mac interface";
	switch (key) {

	case 'a':
		inet_aton(arg, &in_addr);
		route_info->ip_info.intf_conf.ip_addr = in_addr.s_addr;
		g_mndtr_param |= IPC_CTRL_PARAM_BMASK_IPADDR;
		break;

	case 'i':
		val = atoi(arg);
		if ((val < IPC_CTRL_IFNUM_MIN) ||
			(val > IPC_CTRL_IFNUM_MAX)) {
			printf("Invalid Value \"%s\" for '%c'\n", arg, key);
			g_parse_error = ERANGE;
			return -ERANGE;
		}
		route_info->ip_info.intf_conf.ifnum = val;
		strcpy(route_info->ip_info.intf_conf.ifname, str);
		g_mndtr_param |= IPC_CTRL_PARAM_BMASK_IFNUM;
		break;

	case 'n':
		name = arg;
		strcpy(route_info->ip_info.intf_conf.ifname, name);
		g_mndtr_param |= IPC_CTRL_PARAM_BMASK_IFNAME;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	route_info->ip_info.intf_conf.bitmask = g_mndtr_param;

	return 0;
}

static error_t parse_show_intf_opt(int key, char *arg, struct argp_state *state)
{
	struct app_ctrl_op_info *route_info = state->input;
	switch (key) {

	case 'a':
		if (strcmp(arg, "true") == 0)
			route_info->ip_info.all = 1;
		else
			route_info->ip_info.all = 0;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/**
 \brief Receives data from message queue
 \param[in]message queue data structure mqd_t
 \return none
 */
static int receive_from_mq(mqd_t mqdes)
{
	ssize_t size;
	struct app_ctrl_op_info ip_info;
	struct mq_attr attr;
	int ret;
	unsigned int result = IPC_CTRL_RSLT_SUCCESSFULL;

	/* Get attributes of the Receive Message queue */
	ret = mq_getattr(mqdes, &attr);
	if (ret) {
		pr_err("%s:Error getting attributes\n",
				__FILE__);
	}
	/* Read the message from receive queue */
	size = mq_receive(mqdes, (char *)&ip_info, attr.mq_msgsize, 0);
	if (unlikely(size < 0)) {
		pr_err("%s:Rcv msgque error\n", __FILE__);
		return -errno;
	}
	assert(size == sizeof(ip_info));
	result = ip_info.result;
	if (ip_info.msg_type == IPC_CTRL_CMD_TYPE_ROUTE_ADD) {
		if (result == IPC_CTRL_RSLT_SUCCESSFULL)
			pr_info("Route Entry Added successfully\n");
		else
			pr_info("Route Entry Addition failed\n");
	} else if (ip_info.msg_type == IPC_CTRL_CMD_TYPE_ROUTE_DEL) {
		if (result == IPC_CTRL_RSLT_SUCCESSFULL)
			pr_info("Route Entry Deleted successfully\n");
		else
			pr_info("Route Entry Deletion failed\n");
	} else if (ip_info.msg_type == IPC_CTRL_CMD_TYPE_ARP_ADD) {
		if (result == IPC_CTRL_RSLT_SUCCESSFULL)
			pr_info("ARP Entry Added successfully\n");
		else
			pr_info("ARP Entry Addition failed\n");
	} else if (ip_info.msg_type == IPC_CTRL_CMD_TYPE_ARP_DEL) {
		if (result == IPC_CTRL_RSLT_SUCCESSFULL)
			pr_info("ARP Entry Deleted successfully\n");
		else
			pr_info("ARP Entry Deletion failed\n");
	} else if (ip_info.msg_type == IPC_CTRL_CMD_TYPE_INTF_CONF_CHNG) {
		if (result == IPC_CTRL_RSLT_SUCCESSFULL)
			pr_info("Intf Configuration Changed successfully\n");
		else
			pr_info("Intf Configuration Change failed\n");
	} else if (ip_info.msg_type == IPC_CTRL_CMD_TYPE_SHOW_INTF) {
		if (result == IPC_CTRL_RSLT_SUCCESSFULL)
			pr_info("Are all the Enabled Interfaces\n");
		else
			pr_info("Show Interfaces failed\n");
	}

	return 0;
}

/**
 \brief The main function. The only function call needed to process
 all command-line options and arguments nicely is argp_parse.
 \param[in] argc
 \param[in] argv
*/
int main(int argc, char **argv)
{
	struct app_ctrl_op_info sa_info;
	int ret;

	memset(&sa_info, 0, sizeof(struct app_ctrl_op_info));

	if (argc == 1) {
		pr_info("Mandatory Parameter missing\n");
		pr_info("Try `ipfwd_config --help' for more information\n");
		goto _close;
	}

	/* Where the magic happens */
	argp_parse(&argp, argc, argv, 0, NULL, &sa_info);

	if (sa_info.pid == 0) {
		pr_info("Either Mandatory Parameter missing : -P\n");
		pr_info("or -P :Not given as foremost option with the cmd\n");
		pr_info("Try `lpm_ipfwd_config --help' for more information\n");
		return 0;
	}

	switch (sa_info.msg_type) {
	case IPC_CTRL_CMD_TYPE_ROUTE_ADD:
		{
			pr_debug
			    ("\nFILE:%s:LINE%d:TYPE PROVIDED FOR ADD OPT:%d",
			     __FILE__, __LINE__, sa_info.msg_type);
			ipc_add_del_command(argc - 3, &argv[3],
					    sa_info.msg_type);
		}
		break;

	case IPC_CTRL_CMD_TYPE_ROUTE_DEL:
		{
			pr_debug
			    ("\nFILE:%s:LINE%d:TYPE PROVIDED FOR DELETE OPT:%d",
			     __FILE__, __LINE__, sa_info.msg_type);
			ipc_add_del_command(argc - 3, &argv[3],
					    sa_info.msg_type);
		}
		break;

	case IPC_CTRL_CMD_TYPE_ARP_ADD:
		{
			pr_debug
			    ("\nFILE:%s:LINE%d:TYPE PROVIDED FOR ADD OPT: %d",
			     __FILE__, __LINE__, sa_info.msg_type);
			ipc_arp_add_del_command(argc - 3, &argv[3],
						sa_info.msg_type);
		}
		break;

	case IPC_CTRL_CMD_TYPE_ARP_DEL:
		{
			pr_debug
			    ("\nFILE:%s:LINE %d:TYPE PROVIDED FOR DEL OPT:%d",
			     __FILE__, __LINE__, sa_info.msg_type);
			ipc_arp_add_del_command(argc - 3, &argv[3],
						sa_info.msg_type);
		}
		break;

	case IPC_CTRL_CMD_TYPE_INTF_CONF_CHNG:
		{
			pr_debug
			    ("\nFILE: %s : LINE %d : IN MAIN : THE TYPE"
				"PROVIDED FOR INTF CONF OPTION IS : %d",
				__FILE__, __LINE__, sa_info.msg_type);
			ipc_ip_intf_chng_command(argc - 3, &argv[3],
					 (char *)(uintptr_t)sa_info.msg_type);
		}
		break;

	case IPC_CTRL_CMD_TYPE_SHOW_INTF:
		{
			pr_debug
			    ("\nFILE: %s : LINE %d : IN MAIN : THE TYPE"
				"PROVIDED FOR SHOW INTF OPTION IS : %d",
				__FILE__, __LINE__, sa_info.msg_type);
			ipc_show_intf_command(argc - 3, &argv[3],
				      (char *)(uintptr_t)sa_info.msg_type);
		}
		break;

	default:
		pr_debug("Invalid Option\n");
	}

	receive_from_mq(mq_fd_rd);
_close:
	ret = mq_close(mq_fd_wr);
	if (ret) {
		pr_err("%s: %d error in closing MQ: errno = %d\n",
					__FILE__, __LINE__, errno);
	}
	ret = mq_close(mq_fd_rd);
	if (ret) {
		pr_err("%s: %d error in closing MQ: errno = %d\n",
					__FILE__, __LINE__, errno);
	}
	return 0;
}
