/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This file is part of ibsim.
 *
 * ibsim is available to you under a choice of one of two licenses.
 * You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>

#include <ibsim.h>
#include "sim.h"

#undef DEBUG
#define PDEBUG	if (parsedebug) IBWARN
#define DEBUG	if (simverb > 1 || ibdebug) IBWARN

extern void free_core(void);
extern void free_tables(void);
extern int sim_init_net(char *, FILE *);

extern Node *nodes;
extern Switch *switches;
extern Port *ports;
extern Port **lids;
extern int netnodes, netports, netswitches;
extern char *netfile;
extern FILE *outfile;

#define NAMELEN	NODEIDLEN

static const char *portstates[] = {
	"-", "Down", "Init", "Armed", "Active",
};

static const char *physstates[] = {
	"-", "Sleep", "Polling", "Disabled", "Training", "LinkUp",
	    "ErrorRecovery",
};

static const char *portlinkwidth[] = {
	"-", " 1x", " 4x", "-", " 8x", "-", "-", "-", "12x",
};

static const char *portlinkspeed[] = {
	"-", " 2.5G", " 5.0G", "-", "10.0G",
};

static const char *portlinkspeedext[] = {
	"0", " 14G", " 25G",
};

static const char *portlinkmlnxspeed[] = {
	"0", " FDR10",
};

#define PORTSTATE(i) (((i) < 1 || (i) > 4) ? "?" : portstates[(i)])
#define PHYSSTATE(i) (((i) < 1 || (i) > 6) ? "?" : physstates[(i)])
#define PORTLINKWIDTH(i) (((i) < 1 || (i) > 8) ? "?" : portlinkwidth[(i)])
#define PORTLINKSPEED(i) (((i) < 1 || (i) > 4) ? "?" : portlinkspeed[(i)])
#define PORTLINKSPEEDEXT(i) (((i) < 0 || (i) > 2) ? "?" : portlinkspeedext[(i)])
#define PORTMLNXLINKSPEED(i) (((i) < 0 || (i) > 1) ? "?" : portlinkmlnxspeed[(i)])

static int do_link(FILE * f, char *line)
{
	Port *lport, *rport;
	Node *lnode, *rnode;
	char *orig = 0;
	char *lnodeid = 0;
	char *rnodeid = 0;
	char *s = line, name[NAMELEN], *sp;
	int lportnum = -1, rportnum = -1;

	// parse local
	if (strsep(&s, "\""))
		orig = strsep(&s, "\"");

	lnodeid = expand_name(orig, name, &sp);
	if (!sp && s && *s == '[')
		sp = s + 1;

	DEBUG("lnodeid %s port [%s", lnodeid, sp);
	if (!(lnode = find_node(lnodeid))) {
		fprintf(f, "# nodeid \"%s\" (%s) not found\n", orig, lnodeid);
		return -1;
	}

	if (sp) {
		lportnum = strtoul(sp, &sp, 0);
		if (lportnum < 1 || lportnum > lnode->numports) {
			fprintf(f, "# nodeid \"%s\": bad port %d\n",
				lnodeid, lportnum);
			return -1;
		}
	} else {
		fprintf(f, "# no local port\n");
		return -1;
	}

	lport = node_get_port(lnode, lportnum);

	// parse remote
	if (strsep(&s, "\""))
		orig = strsep(&s, "\"");

	rnodeid = expand_name(orig, name, &sp);
	if (!sp && s && *s == '[')
		sp = s + 1;

	DEBUG("rnodeid %s port [%s", rnodeid, sp);
	if (!(rnode = find_node(rnodeid))) {
		fprintf(f, "# nodeid \"%s\" (%s) not found\n", orig, rnodeid);
		return -1;
	}

	if (sp) {
		rportnum = strtoul(sp, &sp, 0);
		if (rportnum < 1 || rportnum > rnode->numports) {
			fprintf(f, "# nodeid \"%s\": bad port %d\n",
				rnodeid, rportnum);
			return -1;
		}
	} else {
		fprintf(f, "# no remote port\n");
		return -1;
	}

	rport = node_get_port(rnode, rportnum);

	if (link_ports(lport, rport) < 0)
		return -fprintf(f,
				"# can't link: local/remote port are already connected\n");

	lport->previous_remotenode = NULL;
	rport->previous_remotenode = NULL;

	return 0;
}

static int do_relink(FILE * f, char *line)
{
	Port *lport, *rport, *e;
	Node *lnode;
	char *orig = 0;
	char *lnodeid = 0;
	char *s = line, name[NAMELEN], *sp;
	int lportnum = -1;
	int numports, relinked = 0;

	// parse local
	if (strsep(&s, "\""))
		orig = strsep(&s, "\"");

	lnodeid = expand_name(orig, name, &sp);
	if (!sp && s && *s == '[')
		sp = s + 1;

	DEBUG("lnodeid %s port [%s", lnodeid, sp);
	if (!(lnode = find_node(lnodeid))) {
		fprintf(f, "# nodeid \"%s\" (%s) not found\n", orig, lnodeid);
		return -1;
	}

	if (sp) {
		lportnum = strtoul(sp, &sp, 0);
		if (lportnum < 1 || lportnum > lnode->numports) {
			fprintf(f, "# nodeid \"%s\": bad port %d\n",
				lnodeid, lportnum);
			return -1;
		}
	}
	numports = lnode->numports;

	if (lnode->type == SWITCH_NODE)
		numports++;	// To make the for-loop below run up to last port
	else
		lportnum--;
	
	if (lportnum >= 0) {
		lport = ports + lnode->portsbase + lportnum;

		if (!lport->previous_remotenode) {
			fprintf(f, "# no previous link stored\n");
			return -1;
		}

		rport = node_get_port(lport->previous_remotenode,
				      lport->previous_remoteport);

		if (link_ports(lport, rport) < 0)
			return -fprintf(f,
					"# can't link: local/remote port are already connected\n");

		lport->previous_remotenode = NULL;
		rport->previous_remotenode = NULL;

		return 1;
	}

	for (lport = ports + lnode->portsbase, e = lport + numports; lport < e;
	     lport++) {
		if (!lport->previous_remotenode)
			continue; 

		rport = node_get_port(lport->previous_remotenode,
				      lport->previous_remoteport);

		if (link_ports(lport, rport) < 0)
			continue;

		lport->previous_remotenode = NULL;
		rport->previous_remotenode = NULL;

		relinked++;
	}

	return relinked;
}

static void unlink_port(Node * lnode, Port * lport, Node * rnode, int rportnum)
{
	Port *rport = node_get_port(rnode, rportnum);
	Port *endport;

	/* save current connection for potential relink later */
	lport->previous_remotenode = lport->remotenode;
	lport->previous_remoteport = lport->remoteport;
	rport->previous_remotenode = rport->remotenode;
	rport->previous_remoteport = rport->remoteport;

	lport->remotenode = rport->remotenode = 0;
	lport->remoteport = rport->remoteport = 0;
	lport->remotenodeid[0] = rport->remotenodeid[0] = 0;
	lport->state = rport->state = 1;	// Down
	lport->physstate = rport->physstate = 2;	// Polling
	if (lnode->sw)
		lnode->sw->portchange = 1;
	if (rnode->sw)
		rnode->sw->portchange = 1;

	if (lnode->type == SWITCH_NODE) {
		endport = node_get_port(lnode, 0);
		send_trap(endport, TRAP_128);
	}

	if (rnode->type == SWITCH_NODE) {
		endport = node_get_port(rnode, 0);
		send_trap(endport, TRAP_128);
	}
}

static void port_change_lid(Port * port, int lid, int lmc)
{
	port->lid = lid;
	if (lmc > 0)
		port->lmc = lmc;

	if (port->node->type == SWITCH_NODE) {
		if (port->node->sw)
			port->node->sw->portchange = 1;
	} else if (port->remotenode && port->remotenode->sw)
		port->remotenode->sw->portchange = 1;
}

static int do_seterror(FILE * f, char *line)
{
	Port *port, *e;
	Node *node;
	char *s = line;
	char *nodeid = 0, name[NAMELEN], *sp, *orig = 0;
	int portnum = -1;	// def - all ports
	int numports, set = 0, rate = 0;
	uint16_t attr = 0;

	if (strsep(&s, "\""))
		orig = strsep(&s, "\"");

	if (!s) {
		fprintf(f, "# set error: bad parameter in \"%s\"\n", line);
		return -1;
	}

	nodeid = expand_name(orig, name, &sp);
	if (!sp && *s == '[')
		sp = s + 1;

	if (!(node = find_node(nodeid))) {
		fprintf(f, "# nodeid \"%s\" (%s) not found\n", orig, nodeid);
		return -1;
	}

	if (sp) {
		portnum = strtoul(sp, 0, 0);
		if (portnum < 1 || portnum > node->numports) {
			fprintf(f, "# bad port number %d at nodeid \"%s\"\n",
				portnum, nodeid);
			return -1;
		}
	}
	strsep(&s, " \t");
	if (!s) {
		fprintf(f, "# error rate is missing\n");
		return -1;
	}

	rate = strtoul(s, 0, 0);

	if (rate > 100) {
		fprintf(f, "# error rate must be in [0..100] range (%d)\n",
			rate);
		return -1;
	}

	DEBUG("error rate is %d", rate);

	strsep(&s, " \t");
	if (s) {
		attr = strtoul(s, 0, 0);
		DEBUG("error attr is %u", attr);
	}

	numports = node->numports;

	if (node->type == SWITCH_NODE)
		numports++;	// To make the for-loop below run up to last port
	else
		portnum--;

	if (portnum >= 0) {
		port = ports + node->portsbase + portnum;
		port->errrate = rate;
		port->errattr = attr;
		return 1;
	}

	for (port = ports + node->portsbase, e = port + numports; port < e;
	     port++) {
		port->errrate = rate;
		port->errattr = attr;
		set++;
	}

	return set;
}

static int do_unlink(FILE * f, char *line, int clear)
{
	Port *port, *e;
	Node *node;
	char *s = line;
	char *nodeid = 0, name[NAMELEN], *sp, *orig = 0;
	int portnum = -1;	// def - all ports
	int numports, unlinked = 0;

	if (strsep(&s, "\""))
		orig = strsep(&s, "\"");

	if (!s) {
		fprintf(f, "# unlink: bad parameter in \"%s\"\n", line);
		return -1;
	}

	nodeid = expand_name(orig, name, &sp);
	if (!sp && *s == '[')
		sp = s + 1;

	if (!(node = find_node(nodeid))) {
		fprintf(f, "# nodeid \"%s\" (%s) not found\n", orig, nodeid);
		return -1;
	}

	if (sp) {
		portnum = strtoul(sp, 0, 0);
		if (portnum < 1 || portnum > node->numports) {
			fprintf(f, "# can't unlink port %d at nodeid \"%s\"\n",
				portnum, nodeid);
			return -1;
		}
	}
	numports = node->numports;

	if (node->type == SWITCH_NODE)
		numports++;	// To make the for-loop below run up to last port
	else
		portnum--;

	if (portnum >= 0) {
		port = ports + node->portsbase + portnum;
		if (!clear && !port->remotenode) {
			fprintf(f, "# port %d at nodeid \"%s\" is not linked\n",
				portnum, nodeid);
			return -1;
		}
		if (port->remotenode)
			unlink_port(node, port, port->remotenode,
				    port->remoteport);
		if (clear)
			reset_port(port);
		return 1;
	}

	for (port = ports + node->portsbase, e = port + numports; port < e;
	     port++) {
		if (!clear && !port->remotenode)
			continue;
		if (port->remotenode)
			unlink_port(node, port, port->remotenode,
				    port->remoteport);
		if (clear)
			reset_port(port);
		unlinked++;
	}

	return unlinked;
}

static int do_set_guid(FILE * f, char *line)
{
	char name[NAMELEN];
	uint64_t new_guid;
	Node *node;
	Port *port = NULL;
	char *s = line, *end;
	char *nodeid = 0, *sp, *orig = 0;
	int portnum = -1;

	if (strsep(&s, "\""))
		orig = strsep(&s, "\"");

	if (!s) {
		fprintf(f, "# set_guid: bad parameter in \"%s\"\n", line);
		return -1;
	}

	nodeid = expand_name(orig, name, &sp);
	if (!sp && *s == '[')
		sp = s + 1;

	if (!(node = find_node(nodeid))) {
		fprintf(f, "# nodeid \"%s\" (%s) not found\n", orig, nodeid);
		return -1;
	}


	if (sp) {
		portnum = strtoul(sp, 0, 0);
		if ((node->type != SWITCH_NODE && portnum < 1)
		    || portnum > node->numports) {
			fprintf(f, "# can't parse port %d at nodeid \"%s\"\n",
				portnum, nodeid);
			return -1;
		}
		if (node->type != SWITCH_NODE)
			port = ports + node->portsbase + portnum - 1;
	}

	while (isspace(*s))
		s++;

	if (!s)
		return 0;

	new_guid = strtoull(s, &end, 0);
	if (*end && !isspace(*end))
		return 0;

	if (port)
		port->portguid = new_guid;
	else {
		node->nodeguid = new_guid;
		mad_encode_field(node->nodeinfo, IB_NODE_GUID_F,
				 &node->nodeguid);
	}

	return 1;
}

static void dump_switch(FILE * f, Switch * sw)
{
	int i, j, top;

	fprintf(f, "#\tlinearcap %d FDBtop %d portchange %d\n",
		sw->linearcap, sw->linearFDBtop, sw->portchange);

	for (i = 0; i < sw->linearFDBtop; i += 16) {
		top = i + 16;
		if (top >= sw->linearFDBtop)
			top = sw->linearFDBtop + 1;

		fprintf(f, "#\tForwarding table %d-%d:", i, top - 1);
		for (j = i; j < top; j++)
			fprintf(f, " [%d]%X", j, (unsigned char)sw->fdb[j]);
		fprintf(f, "\n");
	}
}

static void dump_comment(Port * port, char *comment)
{
	int n = 0;
	if (port->errrate)
		n += sprintf(comment, "\t# err_rate %d", port->errrate);
	if (port->errattr)
		n += sprintf(comment+n, "\t# err_attr %d", port->errattr);
}

static void dump_port(FILE * f, Port * port, int type)
{
	char comment[100] = "";
	static const char *link_speed_str;

	dump_comment(port, comment);

	if (port->state == 1)
		fprintf(f, "%" PRIx64 "\t[%d]\t\t", port->portguid,
			port->portnum);
	else
		fprintf(f, "%" PRIx64 "\t[%d]\t\"%s\"[%d]",
			port->portguid, port->portnum,
			port->remotenode ? port->remotenode->
			nodeid : "Sma Port", port->remoteport);

	if (port->mlnx_linkspeed)
		link_speed_str = PORTMLNXLINKSPEED(port->mlnx_linkspeed);
	else if (port->linkspeedext)
		link_speed_str = PORTLINKSPEEDEXT(port->linkspeedext);
	else
		link_speed_str = PORTLINKSPEED(port->linkspeed);

	if (type == SWITCH_NODE && port->portnum)
		fprintf(f, "\t %s %s %s/%s%s\n",
			PORTLINKWIDTH(port->linkwidth),
			link_speed_str,
			PORTSTATE(port->state), PHYSSTATE(port->physstate),
			comment);
	else
		fprintf(f, "\t lid %u lmc %d smlid %u %s %s %s/%s%s\n",
			port->lid, port->lmc, port->smlid,
			PORTLINKWIDTH(port->linkwidth),
			link_speed_str,
			PORTSTATE(port->state), PHYSSTATE(port->physstate),
			comment);
}

static int dump_net(FILE * f, char *line)
{
	Node *node, *e;
	int nports, i;
	char *s = line;
	char name[NAMELEN], *sp;
	char *nodeid = 0;
	int nnodes = 0;

	time_t t = time(0);

	if (strsep(&s, "\""))
		nodeid = expand_name(strsep(&s, "\""), name, &sp);

	fprintf(f, "# Net status - %s", ctime(&t));
	for (node = nodes, e = node + netnodes; node < e; node++) {
		if (nodeid && strcmp(nodeid, node->nodeid))
			continue;

		fprintf(f, "\n%s %d \"%s\"",
			node_type_name(node->type),
			node->numports, node->nodeid);
		fprintf(f, "\tnodeguid %" PRIx64 "\tsysimgguid %" PRIx64 "\n",
			node->nodeguid, node->sysguid);
		nports = node->numports;
		if (node->type == SWITCH_NODE) {
			nports++;
			dump_switch(f, node->sw);
		}
		for (i = 0; i < nports; i++)
			dump_port(f, ports + node->portsbase + i, node->type);
		nnodes++;
	}

	if (nodeid && !nnodes)
		return -fprintf(f, "# nodeid \"%s\" not found\n", nodeid);
	else
		fprintf(f, "#  dumped %d nodes\n", nnodes);

	fflush(f);

	return 0;
}

static Port *find_port(int lid)
{
	Port *port = 0;
	int i, l;

	for (l = lid, i = 256; i-- && l > 0; l--) {
		if ((port = lids[l]))
			break;
	}
	if (port && (port->lid + (1 << port->lmc)) > lid)
		return port;
	return 0;
}

static int do_change_baselid(FILE * f, char *line)
{
	Port *port;
	Node *node;
	char *s = line;
	char *nodeid = 0, name[NAMELEN], *sp, *orig = 0;
	int portnum = -1;	// def - all ports
	int lid = 0, lmc = -1;

	if (strsep(&s, "\""))
		orig = strsep(&s, "\"");

	if (!s) {
		fprintf(f, "# change baselid: bad parameter in \"%s\"\n", line);
		return -1;
	}

	nodeid = expand_name(orig, name, &sp);
	if (!sp && *s == '[')
		sp = s + 1;

	if (!(node = find_node(nodeid))) {
		fprintf(f, "# nodeid \"%s\" (%s) not found\n", orig, nodeid);
		return -1;
	}

	if (!sp) {
		fprintf(f, "# change baselid: missing portnum");
		return -1;
	}

	portnum = strtoul(sp, &sp, 0);
	if ((portnum < 1 && node->type != SWITCH_NODE)
	    || portnum > node->numports) {
		fprintf(f,
			"# can't change baselid for port %d at nodeid \"%s\"\n",
			portnum, nodeid);
		return -1;
	}

	if (node->type != SWITCH_NODE)
		portnum--;

	port = ports + node->portsbase + portnum;

	if (!sp || *sp != ']') {
		fprintf(f, "# change baselid: missing ']'\n");
		return -1;
	}

	sp++;

	if (sp && *sp)
		while (isspace(*sp))
			sp++;
	lid = strtoul(sp, &sp, 0);

	if (!lid) {
		fprintf(f, "# change baselid: bad lid\n");
		return -1;
	}

	if (sp && *sp)
		while (isspace(*sp))
			sp++;

	if (sp && *sp)
		lmc = strtoul(sp, 0, 0);

	port_change_lid(port, lid, lmc);
	return 1;
}

static int dump_route(FILE * f, char *line)
{
	char *s = line, *p1, *p2;
	int from, to;
	int maxhops = MAXHOPS;
	Node *node;
	Port *port, *fromport, *toport;
	int portnum, outport;

	if (!strsep(&s, "\t ") || !(p1 = strsep(&s, "\t "))
	    || !(p2 = strsep(&s, "\t "))) {
		fprintf(f, "bad params. Usage: route from-lid lid\n");
		return -1;
	}
	from = strtoul(p1, 0, 0);
	to = strtoul(p2, 0, 0);

	if (!from || !to) {
		fprintf(f, "bad lid value. Usage: route from-lid to-lid\n");
		return -1;
	}
	fromport = find_port(from);
	toport = find_port(to);

	if (!fromport || !toport) {
		fprintf(f,
			"to/from lid unconfigured. Usage: route from-lid to-lid\n");
		return -1;
	}

	node = fromport->node;
	port = fromport;
	portnum = port->portnum;
	fprintf(f, "From node \"%s\" port %d lid %u\n", node->nodeid, portnum,
		from);
	while (maxhops--) {
		if (port->state != 4)
			goto badport;
		if (port == toport)
			break;	// found
		outport = portnum;
		if (node->type == SWITCH_NODE) {
			if ((outport = node->sw->fdb[to]) < 0
			    || to > node->sw->linearFDBtop)
				goto badtbl;
			port = ports + node->portsbase + outport;
			if (outport == 0) {
				if (port != toport)
					goto badtbl;
				else
					break;	// found SMA port
			}
			if (port->state != 4)
				goto badoutport;
		}
		node = port->remotenode;
		port = ports + node->portsbase + port->remoteport;
		if (node->type != SWITCH_NODE)
			port--;
		portnum = port->portnum;
		fprintf(f, "[%d] -> \"%s\"[%d]\n", outport, node->nodeid,
			portnum);
	}

	if (maxhops <= 0) {
		fprintf(f, "no route found after %d hops\n", MAXHOPS);
		return -1;
	}
	fprintf(f, "To node \"%s\" port %d lid %u\n", node->nodeid, portnum,
		to);
	return 0;

      badport:
	fprintf(f, "Bad port state found: node \"%s\" port %d state %d\n",
		node->nodeid, portnum, port->state);
	return -1;
      badoutport:
	fprintf(f,
		"Bad out port state found: node \"%s\" outport %d state %d\n",
		node->nodeid, outport, port->state);
	return -1;
      badtbl:
	fprintf(f,
		"Bad forwarding table entry found at: node \"%s\" lid entry %d is %d (top %d)\n",
		node->nodeid, to, outport, node->sw->linearFDBtop);
	return -1;
}

static int change_verbose(FILE * f, char *line)
{
	char *s = line;

	if (strsep(&s, "\t ") && s)
		simverb = strtoul(s, 0, 0);
	fprintf(f, "simulator verbose level is %d\n", simverb);
	return 0;
}

static int do_wait(FILE * f, char *line)
{
	char *s = line;
	long sec = 0;

	if (strsep(&s, "\t ") && s)
		sec = strtoul(s, 0, 0);
	if (sec <= 0)
		return -fprintf(f, "wait: bad param %ld\n", sec);
	else
		sleep(sec);
	return 0;
}

static int dump_help(FILE * f)
{
	fprintf(f, "sim> Commands:\n");
	fprintf(f, "\t!<filename> - run commands from the file\n");
	fprintf(f, "\tStart network\n");
	fprintf(f, "\tDump [nodeid] (def all network)\n");
	fprintf(f, "\tRoute <from-lid> <to-lid>\n");
	fprintf(f, "\tLink \"nodeid\"[port] \"remoteid\"[port]\n");
	fprintf(f, "\tReLink \"nodeid\" : restore previously unconnected link(s) of the node\n");
	fprintf(f, "\tReLink \"nodeid\"[port] : restore previously unconnected link\n");
	fprintf(f, "\tUnlink \"nodeid\" : remove all links of the node\n");
	fprintf(f, "\tUnlink \"nodeid\"[port]\n");
	fprintf(f,
		"\tClear \"nodeid\" : unlink & reset all links of the node\n");
	fprintf(f, "\tClear \"nodeid\"[port] : unlink & reset port\n");
	fprintf(f, "\tGuid \"nodeid\" : set GUID value for this node\n");
	fprintf(f, "\tGuid \"nodeid\"[port] : set GUID value for this port\n");
	fprintf(f,
		"\tError \"nodeid\"[port] <error-rate> [attribute]: set error rate for\n"
		"\t\t\tport/node, optionally for specified attribute ID\n"
		"\t\t\tSome common attribute IDs:\n"
		"\t\t\t\tNodeDescription : 16\n"
		"\t\t\t\tNodeInfo        : 17\n"
		"\t\t\t\tSwitchInfo      : 18\n"
		"\t\t\t\tPortInfo        : 19\n"
		);
	fprintf(f,
		"\tBaselid \"nodeid\"[port] <lid> [lmc] : change port's lid (lmc)\n");
	fprintf(f, "\tReload : reload net topology file\n");
	fprintf(f, "\tVerbose [newlevel] - show/set simulator verbosity\n");
	fprintf(f, "\t\t\t0 - silent\n");
	fprintf(f, "\t\t\t1 - debug verbose\n");
	fprintf(f, "\tWait <sec> : suspend simulator prompt\n");
	fprintf(f, "\tAttached : list attached clients\n");
	fprintf(f, "\tX <client num> : (force) disconnect client\n");
	fprintf(f, "\t#... : comment line (for scripts) - ignored\n");
	fprintf(f, "\tHelp/?\n");
	fprintf(f, "\tQuit\n");
	return 0;
}

static int do_disconnect_client(FILE * out, int id)
{
	if (disconnect_client(id)) {
		fprintf(out, "disconnect client: bad clientid %d\n", id);
		return -1;
	}
	return 0;
}

int netstarted;

int do_cmd(char *buf, FILE *f)
{
	unsigned int cmd_len = 0;
	char *line;
	int r = 0;
	Node *nd, *e;
	Port *port;

	for (line = buf; *line && isspace(*line); line++) ;

	while (!isspace(line[cmd_len]))
		cmd_len++;

	if (*line == '#')
		fprintf(f, "%s", line);
	else if (*line == '!')
		r = sim_cmd_file(f, line);
	else if (!strncasecmp(line, "Dump", cmd_len))
		r = dump_net(f, line);
	else if (!strncasecmp(line, "Route", cmd_len))
		r = dump_route(f, line);
	else if (!strncasecmp(line, "Link", cmd_len))
		r = do_link(f, line);
	else if (!strncasecmp(line, "Unlink", cmd_len))
		r = do_unlink(f, line, 0);
	else if (!strncasecmp(line, "Clear", cmd_len))
		r = do_unlink(f, line, 1);
	else if (!strncasecmp(line, "Guid", cmd_len))
		r = do_set_guid(f, line);
	else if (!strncasecmp(line, "Error", cmd_len))
		r = do_seterror(f, line);
	else if (!strncasecmp(line, "Baselid", cmd_len))
		r = do_change_baselid(f, line);
	else if (!strncasecmp(line, "Start", cmd_len)) {
		if (!netstarted) {
			DEBUG("starting...");
			netstarted = 1;
			return 0;
		}
	}
	else if (!strncasecmp(line, "Verbose", cmd_len))
		r = change_verbose(f, line);
	else if (!strncasecmp(line, "Wait", cmd_len))
		r = do_wait(f, line);
	else if (!strncasecmp(line, "Attached", cmd_len))
		r = list_connections(f);
	else if (!strncasecmp(line, "X", cmd_len))
		r = do_disconnect_client(f, strtol(line + 2, 0, 0));
	else if (!strncasecmp(line, "Help", cmd_len)
		 || !strncasecmp(line, "?", cmd_len))
		r = dump_help(f);
	else if (!strncasecmp(line, "Quit", cmd_len)) {
		fprintf(f, "Exiting network simulator.\n");
		free_core();
		exit(0);
	}
	/* commands specified above support legacy single
	 * character options.  For example, 'g' or 'G' for "Guid"
	 * and 'l' or 'L' for "Link".
	 *
	 * please specify new command support below this comment.
	 */
	else if (!strncasecmp(line, "ReLink", cmd_len))
		r = do_relink(f, line);
	else if (!strncasecmp(line, "Reload", cmd_len)) {
		/* free switch and port tables allocation */
		free_tables();

		r = sim_init_net(netfile, outfile);

		/* search for a the first switch in order to send trap */
		for (nd = nodes, e = nodes + netnodes; nd < e; nd++)
			if (nd->type == SWITCH_NODE)
				break;

		port = node_get_port(nd, 0);
		send_trap(port, TRAP_128);
	}
	else if (*line != '\n' && *line != '\0')
		fprintf(f, "command \'%s\' unknown - skipped\n", line);

	return r;
}
