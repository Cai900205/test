#ifndef _COMPAT_IF_ARP_H_
#define _COMPAT_IF_ARP_H_

#if defined(COMPAT_VMWARE)
#define ARPHRD_INFINIBAND 32		/* InfiniBand			*/

/* ARP protocol HARDWARE identifiers. */
#define ARPHRD_NETROM      0     /* from KA9Q: NET/ROM pseudo     */
#define ARPHRD_ETHER       1     /* Ethernet 10Mbps               */
#define ARPHRD_EETHER      2     /* Experimental Ethernet         */
#define ARPHRD_AX25        3     /* AX.25 Level 2                 */
#define ARPHRD_PRONET      4     /* PROnet token ring             */
#define ARPHRD_CHAOS       5     /* Chaosnet                      */
#define ARPHRD_IEEE802     6     /* IEEE 802.2 Ethernet/TR/TB     */
#define ARPHRD_ARCNET      7     /* ARCnet                        */
#define ARPHRD_APPLETLK    8     /* APPLEtalk                     */
#define ARPHRD_DLCI        15    /* Frame Relay DLCI              */
#define ARPHRD_ATM         19    /* ATM                           */
#define ARPHRD_METRICOM    23    /* Metricom STRIP (new IANA id)  */
#define ARPHRD_IEEE1394    24    /* IEEE 1394 IPv4 - RFC 2734     */

/* ARP protocol opcodes. */
#define  ARPOP_REQUEST     1     /* ARP request       */
#define  ARPOP_REPLY       2     /* ARP reply         */
#define  ARPOP_RREQUEST    3     /* RARP request      */
#define  ARPOP_RREPLY      4     /* RARP reply        */
#define  ARPOP_InREQUEST   8     /* InARP request     */
#define  ARPOP_InREPLY     9     /* InARP reply       */
#define  ARPOP_NAK         10    /* (ATM)ARP NAK      */

struct arphdr {
	unsigned short ar_hrd; /* format of hardware address */
	unsigned short ar_pro; /* format of protocol address */
	unsigned char ar_hln; /* length of hardware address */
	unsigned char ar_pln; /* length of protocol address */
	unsigned short ar_op; /* ARP opcode (command)    */
};

#else
#include_next <linux/if_arp.h>
#endif

#endif /* _COMPAT_IF_ARP_H_ */
