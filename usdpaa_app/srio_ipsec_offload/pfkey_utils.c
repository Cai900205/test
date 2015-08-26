/* Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/ipsec.h>
#include <linux/pfkeyv2.h>

#include <stdbool.h>
#include "fsl_fman.h"
#include "usdpaa/fsl_dpa_ipsec.h"
#include "usdpaa/fsl_dpa_ipsec_algs.h"
#include "xfrm_km.h"

#define CALLOC(size, cast) (cast)calloc(1, (size))
#define PFKEY_UNUNIT64(a)	((a) << 3)
#define PFKEY_UNIT64(a)		((a) >> 3)

#define PFKEY_ALIGN8(a) (1 + (((a) - 1) | (8 - 1)))
#define PFKEY_EXTLEN(msg) \
	PFKEY_UNUNIT64(((struct sadb_ext *)(msg))->sadb_ext_len)
#define PFKEY_ADDR_PREFIX(ext) \
	(((struct sadb_address *)(ext))->sadb_address_prefixlen)
#define PFKEY_ADDR_PROTO(ext) \
	(((struct sadb_address *)(ext))->sadb_address_proto)
#define PFKEY_ADDR_SADDR(ext) \
	((struct sockaddr *)((caddr_t)(ext) + sizeof(struct sadb_address)))

static inline int alg_suite(int aalg, int ealg)
{
	int i;
	for (i = 0; i < (sizeof(dpa_algs)/sizeof(dpa_algs[0])); i++)
		if (aalg == dpa_algs[i].aalg && ealg == dpa_algs[i].ealg)
			return dpa_algs[i].dpa_alg;
	return -1;
}

int get_algs_by_name(const char *cipher_alg_name, const char *auth_alg_name)
{
	int i;

	for (i = 0; i < (sizeof(dpa_algs)/sizeof(dpa_algs[0])); i++)
		if (!strcmp(cipher_alg_name, dpa_algs[i].ealg_s) &&
		    !strcmp(auth_alg_name, dpa_algs[i].aalg_s))
			return dpa_algs[i].dpa_alg;

	return -1;
}

static inline void get_auth_info(struct sadb_key *m_auth,
				 struct dpa_ipsec_sa_params *sa_params)
{
	sa_params->crypto_params.auth_key_len = (uint8_t)
						    (m_auth->sadb_key_bits / 8);
	sa_params->crypto_params.auth_key =(uint8_t *)
				    ((caddr_t)(void *)m_auth + sizeof(*m_auth));
}

static inline void get_crypt_info(struct sadb_key *m_enc,
				  struct dpa_ipsec_sa_params *sa_params)
{
	sa_params->crypto_params.cipher_key_len = (uint8_t)
						   (m_enc->sadb_key_bits / 8);
	sa_params->crypto_params.cipher_key = (uint8_t *)(
				       (caddr_t)(void *)m_enc + sizeof(*m_enc));
}

void kdebug_sadb(struct sadb_msg *base)
{
	struct sadb_ext *ext;
	int tlen, extlen;

	if (base == NULL) {
		fprintf(stderr, "kdebug_sadb: NULL pointer was passed.\n");
		return;
	}

	printf("sadb_msg{ version=%u type=%u errno=%u satype=%u\n",
	    base->sadb_msg_version, base->sadb_msg_type,
	    base->sadb_msg_errno, base->sadb_msg_satype);
	printf("  len=%u reserved=%u seq=%u pid=%u\n",
	    base->sadb_msg_len, base->sadb_msg_reserved,
	    base->sadb_msg_seq, base->sadb_msg_pid);

	tlen = PFKEY_UNUNIT64(base->sadb_msg_len) - sizeof(struct sadb_msg);
	ext = (void *)((caddr_t)(void *)base + sizeof(struct sadb_msg));

	while (tlen > 0) {
		printf("sadb_ext{ len=%u type=%u }\n",
		    ext->sadb_ext_len, ext->sadb_ext_type);

		if (ext->sadb_ext_len == 0) {
			printf("kdebug_sadb: invalid ext_len=0 was passed.\n");
			return;
		}
		if (ext->sadb_ext_len > tlen) {
			printf("kdebug_sadb: ext_len exceeds end of buffer.\n");
			return;
		}
		extlen = PFKEY_UNUNIT64(ext->sadb_ext_len);
		tlen -= extlen;
		ext = (void *)((caddr_t)(void *)ext + extlen);
	}
}


int pfkey_open(void)
{
	int so;
	so = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);
	if (so < 0)
		return -1;
	return so;
}

void pfkey_close(int so)
{
	close(so);
	return;
}

int pfkey_send(int so, struct sadb_msg *msg, int len)
{
	len = send(so, (void *)msg, (socklen_t)len, 0);
	if (len < 0) {
		fprintf(stderr, "%s ret -1\n", strerror(errno));
		return -1;
	}
	return len;
}

static inline u_int8_t sysdep_sa_len(const struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	}
	return sizeof(struct sockaddr_in);
}

static inline void sa_getaddr(const struct sockaddr *sa,
			      xfrm_address_t *xaddr)
{
	switch (sa->sa_family) {
	case AF_INET:
		xaddr->a4 = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
		return;
	case AF_INET6:
		memcpy(&xaddr->a6,
		       &((struct sockaddr_in6 *)sa)->sin6_addr,
		       sizeof(struct in6_addr));
		return;
	}
}


static caddr_t pfkey_setsadbmsg(caddr_t buf, caddr_t lim, u_int type,
				u_int tlen, u_int satype, u_int32_t seq,
				pid_t pid)
{
	struct sadb_msg *p;
	u_int len;

	p = (void *)buf;
	len = sizeof(struct sadb_msg);

	if (buf + len > lim)
		return NULL;

	memset(p, 0, len);
	p->sadb_msg_version = PF_KEY_V2;
	p->sadb_msg_type = type;
	p->sadb_msg_errno = 0;
	p->sadb_msg_satype = satype;
	p->sadb_msg_len = PFKEY_UNIT64(tlen);
	p->sadb_msg_reserved = 0;
	p->sadb_msg_seq = seq;
	p->sadb_msg_pid = (u_int32_t)pid;

	return buf + len;
}

static caddr_t pfkey_setsadbsa(caddr_t buf, caddr_t lim, u_int32_t spi,
			       u_int wsize, u_int auth, u_int enc,
			       u_int32_t flags)
{
	struct sadb_sa *p;
	u_int len;

	p = (void *)buf;
	len = sizeof(struct sadb_sa);

	if (buf + len > lim)
		return NULL;

	memset(p, 0, len);
	p->sadb_sa_len = PFKEY_UNIT64(len);
	p->sadb_sa_exttype = SADB_EXT_SA;
	p->sadb_sa_spi = spi;
	p->sadb_sa_replay = wsize;
	p->sadb_sa_state = SADB_SASTATE_LARVAL;
	p->sadb_sa_auth = auth;
	p->sadb_sa_encrypt = enc;
	p->sadb_sa_flags = flags;

	return buf + len;
}

static caddr_t pfkey_setsadbaddr(caddr_t buf, caddr_t lim, u_int exttype,
	struct sockaddr *saddr, u_int prefixlen, u_int ul_proto)
{
	struct sadb_address *p;
	u_int len;

	p = (void *)buf;
	len = sizeof(struct sadb_address) + PFKEY_ALIGN8(sysdep_sa_len(saddr));

	if (buf + len > lim)
		return NULL;

	memset(p, 0, len);
	p->sadb_address_len = PFKEY_UNIT64(len);
	p->sadb_address_exttype = exttype & 0xffff;
	p->sadb_address_proto = ul_proto & 0xff;
	p->sadb_address_prefixlen = prefixlen;
	p->sadb_address_reserved = 0;

	memcpy(p + 1, saddr, (size_t)sysdep_sa_len(saddr));

	return buf + len;
}

/* sending SADB_X_SPDGET */
static int pfkey_send_spdget(int so, u_int32_t spid)
{
	struct sadb_msg *newmsg;
	struct sadb_x_policy xpl;
	int len;
	caddr_t p;
	caddr_t ep;

	/* create new sadb_msg to reply. */
	len = sizeof(struct sadb_msg) + sizeof(xpl);
	newmsg = CALLOC((size_t)len, struct sadb_msg *);
	if (newmsg == NULL)
		return -1;
	ep = ((caddr_t)(void *)newmsg) + len;

	p = pfkey_setsadbmsg((void *)newmsg, ep, SADB_X_SPDGET, (u_int)len,
	    SADB_SATYPE_UNSPEC, 0, getpid());
	if (!p) {
		free(newmsg);
		return -1;
	}

	if (p + sizeof(xpl) != ep) {
		free(newmsg);
		return -1;
	}
	memset(&xpl, 0, sizeof(xpl));
	xpl.sadb_x_policy_len = PFKEY_UNIT64(sizeof(xpl));
	xpl.sadb_x_policy_exttype = SADB_X_EXT_POLICY;
	xpl.sadb_x_policy_id = spid;
	memcpy(p, &xpl, sizeof(xpl));

	/* send message */
	len = pfkey_send(so, newmsg, len);
	free(newmsg);

	if (len < 0)
		return -1;

	return len;
}

/* sending SADB_DELETE or SADB_GET message to the kernel */
/*ARGSUSED*/
static int
pfkey_send_sadbget(int so, u_int satype, u_int mode,
	struct sockaddr *src, struct sockaddr *dst, u_int32_t spi)
{
	struct sadb_msg *newmsg;
	int len;
	caddr_t p;
	int plen;
	caddr_t ep;

	/* validity check */
	if (src == NULL || dst == NULL)
		return -1;
	if (src->sa_family != dst->sa_family)
		return -1;
	switch (src->sa_family) {
	case AF_INET:
		plen = sizeof(struct in_addr) << 3;
		break;
	case AF_INET6:
		plen = sizeof(struct in6_addr) << 3;
		break;
	default:
		return -1;
	}

	/* create new sadb_msg to reply. */
	len = sizeof(struct sadb_msg)
		+ sizeof(struct sadb_sa)
		+ sizeof(struct sadb_address)
		+ PFKEY_ALIGN8(sysdep_sa_len(src))
		+ sizeof(struct sadb_address)
		+ PFKEY_ALIGN8(sysdep_sa_len(dst));

	newmsg = CALLOC((size_t)len, struct sadb_msg *);
	if (newmsg == NULL)
		return -1;
	ep = ((caddr_t)(void *)newmsg) + len;

	p = pfkey_setsadbmsg((void *)newmsg, ep,
				SADB_GET, (u_int)len, satype, 0,
				getpid());
	if (!p) {
		free(newmsg);
		return -1;
	}
	p = pfkey_setsadbsa(p, ep, spi, 0, 0, 0, 0);
	if (!p) {
		free(newmsg);
		return -1;
	}
	p = pfkey_setsadbaddr(p, ep, SADB_EXT_ADDRESS_SRC, src, (u_int)plen,
	    IPSEC_ULPROTO_ANY);
	if (!p) {
		free(newmsg);
		return -1;
	}
	p = pfkey_setsadbaddr(p, ep, SADB_EXT_ADDRESS_DST, dst, (u_int)plen,
	    IPSEC_ULPROTO_ANY);
	if (!p || p != ep) {
		free(newmsg);
		return -1;
	}
	/* send message */
	len = pfkey_send(so, newmsg, len);
	free(newmsg);

	if (len < 0)
		return -1;

	return len;
}

struct sadb_msg *
pfkey_recv(int so, struct sadb_msg **newmsg)
{
	struct sadb_msg buf;
	struct sadb_msg *tmp;
	int len, reallen;

	while ((len = recv(so, (void *)&buf, sizeof(buf), MSG_PEEK)) < 0) {
		if (errno == EINTR)
			continue;
		return NULL;
	}

	if (len < sizeof(buf)) {
		recv(so, (void *)&buf, sizeof(buf), 0);
		return NULL;
	}

	/* read real message */
	reallen = PFKEY_UNUNIT64(buf.sadb_msg_len);
	tmp = (struct sadb_msg *)realloc(*newmsg, reallen);
	if (tmp == 0) {
		free(*newmsg);
		return NULL;
	}
	*newmsg = tmp;

	while ((len = recv(so, (void *)(*newmsg), (socklen_t)reallen, 0)) < 0) {
		if (errno == EINTR)
			continue;
		return NULL;
	}

	if (len != reallen)
		return NULL;

	/* don't trust what the kernel says, validate! */
	if (PFKEY_UNUNIT64((*newmsg)->sadb_msg_len) != len)
		return NULL;
	return *newmsg;
}


int
pfkey_align(struct sadb_msg *msg, caddr_t *mhp)
{
	struct sadb_ext *ext;
	int i;
	caddr_t p;
	caddr_t ep;

	/* validity check */
	if (msg == NULL || mhp == NULL)
		return -1;

	/* initialize */
	for (i = 0; i < SADB_EXT_MAX + 1; i++)
		mhp[i] = NULL;

	mhp[0] = (void *)msg;

	/* initialize */
	p = (void *) msg;
	ep = p + PFKEY_UNUNIT64(msg->sadb_msg_len);

	/* skip base header */
	p += sizeof(struct sadb_msg);

	while (p < ep) {
		ext = (void *)p;
		if (ep < p + sizeof(*ext) || PFKEY_EXTLEN(ext) < sizeof(*ext) ||
		    ep < p + PFKEY_EXTLEN(ext)) {
			/* invalid format */
			break;
		}

		/* duplicate check */
		/* XXX Are there duplication either KEY_AUTH or KEY_ENCRYPT ?*/
		if (mhp[ext->sadb_ext_type] != NULL)
			return -1;

		/* set pointer */
		switch (ext->sadb_ext_type) {
		case SADB_EXT_SA:
		case SADB_EXT_LIFETIME_CURRENT:
		case SADB_EXT_LIFETIME_HARD:
		case SADB_EXT_LIFETIME_SOFT:
		case SADB_EXT_ADDRESS_SRC:
		case SADB_EXT_ADDRESS_DST:
		case SADB_EXT_ADDRESS_PROXY:
		case SADB_EXT_KEY_AUTH:
			/* XXX should to be check weak keys. */
		case SADB_EXT_KEY_ENCRYPT:
			/* XXX should to be check weak keys. */
		case SADB_EXT_IDENTITY_SRC:
		case SADB_EXT_IDENTITY_DST:
		case SADB_EXT_SENSITIVITY:
		case SADB_EXT_PROPOSAL:
		case SADB_EXT_SUPPORTED_AUTH:
		case SADB_EXT_SUPPORTED_ENCRYPT:
		case SADB_EXT_SPIRANGE:
		case SADB_X_EXT_POLICY:
		case SADB_X_EXT_SA2:
#ifdef SADB_X_EXT_NAT_T_TYPE
		case SADB_X_EXT_NAT_T_TYPE:
		case SADB_X_EXT_NAT_T_SPORT:
		case SADB_X_EXT_NAT_T_DPORT:
		case SADB_X_EXT_NAT_T_OA:
#endif
#ifdef SADB_X_EXT_TAG
		case SADB_X_EXT_TAG:
#endif
#ifdef SADB_X_EXT_PACKET
		case SADB_X_EXT_PACKET:
#endif
#ifdef SADB_X_EXT_KMADDRESS
		case SADB_X_EXT_KMADDRESS:
#endif
#ifdef SADB_X_EXT_SEC_CTX
		case SADB_X_EXT_SEC_CTX:
#endif
			mhp[ext->sadb_ext_type] = (void *)ext;
			break;
		default:
			return -1;
		}

		p += PFKEY_EXTLEN(ext);
	}

	if (p != ep)
		return -1;

	return 0;
}

static int
ipsec_dump_ipsecrequest(char *buf, size_t len,
			struct sadb_x_ipsecrequest *xisr,
			int bound,
			xfrm_address_t *saddr, xfrm_address_t *daddr,
			int *sa_af)
{
	if (xisr->sadb_x_ipsecrequest_len > bound)
		return -1;

	switch (xisr->sadb_x_ipsecrequest_proto) {
	case IPPROTO_ESP:
	case IPPROTO_AH:
	case IPPROTO_COMP:
		break;
	default:
		return -1;
	}

	switch (xisr->sadb_x_ipsecrequest_mode) {
	case IPSEC_MODE_ANY:
	case IPSEC_MODE_TRANSPORT:
	case IPSEC_MODE_TUNNEL:
		break;
	default:
		return -1;
	}

	switch (xisr->sadb_x_ipsecrequest_level) {
	case IPSEC_LEVEL_DEFAULT:
	case IPSEC_LEVEL_USE:
	case IPSEC_LEVEL_REQUIRE:
	case IPSEC_LEVEL_UNIQUE:
		break;
	default:
		return -1;
	}

	if (xisr->sadb_x_ipsecrequest_len > sizeof(*xisr)) {
		struct sockaddr *sa1, *sa2;
		caddr_t p;
		const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;
		char host1[NI_MAXHOST], host2[NI_MAXHOST];
		char serv1[NI_MAXSERV], serv2[NI_MAXHOST];

		p = (void *)(xisr + 1);
		sa1 = (void *)p;
		sa2 = (void *)(p + sysdep_sa_len(sa1));
		if (sizeof(*xisr) + sysdep_sa_len(sa1) + sysdep_sa_len(sa2) !=
		    xisr->sadb_x_ipsecrequest_len) {
			return -1;
		}
		if (getnameinfo(sa1, (socklen_t)sysdep_sa_len(sa1),
			host1, sizeof(host1),
			serv1, sizeof(serv1), niflags) != 0)
			return -1;
		if (getnameinfo(sa2, (socklen_t)sysdep_sa_len(sa2),
			host2, sizeof(host2),
			serv2, sizeof(serv2), niflags) != 0)
			return -1;
		sa_getaddr(sa1, saddr);
		sa_getaddr(sa2, daddr);
		*sa_af = sa1->sa_family;
	}

	return 0;
}


static int
ipsec_dump_policy(void *policy,
		xfrm_address_t *saddr, xfrm_address_t *daddr,
		int *sa_af)
{
	struct sadb_x_policy *xpl = policy;
	struct sadb_x_ipsecrequest *xisr;
	size_t off;
	char isrbuf[1024];

	/* count length of buffer for use */
	off = sizeof(*xpl);
	while (off < PFKEY_EXTLEN(xpl)) {
		xisr = (void *)((caddr_t)(void *)xpl + off);
		off += xisr->sadb_x_ipsecrequest_len;
	}

	/* validity check */
	if (off != PFKEY_EXTLEN(xpl))
		return -1;
	off = sizeof(*xpl);
	while (off < PFKEY_EXTLEN(xpl)) {
		xisr = (void *)((caddr_t)(void *)xpl + off);

		if (ipsec_dump_ipsecrequest(isrbuf, sizeof(isrbuf), xisr,
		    PFKEY_EXTLEN(xpl) - off, saddr, daddr, sa_af) < 0) {
			return -1;
		}
		off += xisr->sadb_x_ipsecrequest_len;
	}
	return 0;
}

static int
pfkey_spdump(struct sadb_msg *m,
		xfrm_address_t *saddr, xfrm_address_t *daddr,
		int *sa_af)
{
	caddr_t mhp[SADB_EXT_MAX + 1];
	struct sadb_x_policy *m_xpl;

	if (pfkey_align(m, mhp))
		return -1;

	m_xpl = (void *)mhp[SADB_X_EXT_POLICY];
	/* policy */
	if (m_xpl == NULL)
		return -1;

	return ipsec_dump_policy(m_xpl, saddr, daddr, sa_af);
}

int
pfkey_sadump(struct sadb_msg *m,
		struct dpa_ipsec_sa_params *sa_params,
		struct xfrm_encap_tmpl *encap)
{
	caddr_t mhp[SADB_EXT_MAX + 1];
	struct sadb_sa *m_sa;
	struct sadb_key *m_auth, *m_enc;
	struct sadb_x_nat_t_port *natt_sport, *natt_dport;

	if (pfkey_align(m, mhp))
		return -1;

	m_sa = (void *)mhp[SADB_EXT_SA];
	m_auth = (void *)mhp[SADB_EXT_KEY_AUTH];
	m_enc = (void *)mhp[SADB_EXT_KEY_ENCRYPT];
	natt_sport = (void *)mhp[SADB_X_EXT_NAT_T_SPORT];
	natt_dport = (void *)mhp[SADB_X_EXT_NAT_T_DPORT];

	if (!m_sa)
		return -1;
	if (!m_enc)
		return -1;
	if (!m_auth)
		return -1;

	get_crypt_info(m_enc, sa_params);
	get_auth_info(m_auth, sa_params);
	sa_params->crypto_params.alg_suite =
		alg_suite(m_sa->sadb_sa_auth, m_sa->sadb_sa_encrypt);
	if (natt_sport && natt_dport) {
		encap->encap_sport =
			ntohs(natt_sport->sadb_x_nat_t_port_port);
		encap->encap_dport =
			ntohs(natt_dport->sadb_x_nat_t_port_port);
	}

	return 0;
}

struct sadb_msg
*do_spdget(int spid, xfrm_address_t *saddr, xfrm_address_t *daddr, int *sa_af)
{
	int ret;
	static struct sadb_msg *m;

	int so = pfkey_open();
	if (so < 0) {
		fprintf(stderr, "Failed to open PF_KEY socket\n");
		return NULL;
	}
	ret = pfkey_send_spdget(so, spid);
	if (ret < 0) {
		fprintf(stderr, "Failed to send SADB_X_SPDGET\n");
		return NULL;
	}

	m = pfkey_recv(so, &m);
	if (!m) {
		fprintf(stderr, "Failed to receive from PF_KEY socket\n");
		pfkey_close(so);
		return NULL;
	}
	ret = pfkey_spdump(m, saddr, daddr, sa_af);
	if (ret < 0) {
		pfkey_close(so);
		return NULL;
	}

	pfkey_close(so);
	return m;
}

struct sadb_msg
*do_sadbget(uint32_t spi, int af,
		xfrm_address_t saddr, xfrm_address_t daddr,
		struct dpa_ipsec_sa_params *sa_params,
		struct xfrm_encap_tmpl *encap)
{
	int ret;
	static struct sadb_msg *m;
	struct sockaddr_in src_in, dst_in;
	struct sockaddr_in6 src_in6, dst_in6;
	struct sockaddr *src, *dst;

	int so = pfkey_open();
	if (so < 0) {
		fprintf(stderr, "Failed to open PF_KEY socket\n");
		return NULL;
	}
	if (af == AF_INET) {
		bzero(&src_in, sizeof(src_in));
		bzero(&dst_in, sizeof(dst_in));
		src_in.sin_family = AF_INET;
		dst_in.sin_family = AF_INET;
		src_in.sin_addr.s_addr = saddr.a4;
		dst_in.sin_addr.s_addr = daddr.a4;
		src = (struct sockaddr *)&src_in;
		dst = (struct sockaddr *)&dst_in;
	} else if (af == AF_INET6) {
		bzero(&src_in6, sizeof(src_in6));
		bzero(&dst_in6, sizeof(dst_in6));
		src_in6.sin6_family = AF_INET6;
		dst_in6.sin6_family = AF_INET6;
		memcpy(&src_in6.sin6_addr, saddr.a6, sizeof(src_in6.sin6_addr));
		memcpy(&dst_in6.sin6_addr, daddr.a6, sizeof(dst_in6.sin6_addr));
		src = (struct sockaddr *)&src_in6;
		dst = (struct sockaddr *)&dst_in6;
	} else {
		pfkey_close(so);
		return NULL;
	}

	ret = pfkey_send_sadbget(so, SADB_SATYPE_ESP, 0, src, dst, spi);
	if (ret < 0) {
		fprintf(stderr, "Failed to send SADB_GET\n");
		return NULL;
	}
	m = pfkey_recv(so, &m);
	if (!m) {
		fprintf(stderr, "Failed to receive from PF_KEY socket\n");
		pfkey_close(so);
		return NULL;
	}
	ret = pfkey_sadump(m, sa_params, encap);
	if (ret < 0) {
		pfkey_close(so);
		return NULL;
	}
	pfkey_close(so);
	return m;
}
