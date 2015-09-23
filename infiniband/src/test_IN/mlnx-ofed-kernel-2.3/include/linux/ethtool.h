#ifndef __COMPAT_ETHTOOL_H
#define __COMPAT_ETHTOOL_H

#include_next <linux/ethtool.h>

/* EEPROM Standards for plug in modules */
#ifndef ETH_MODULE_SFF_8079
#define ETH_MODULE_SFF_8079		0x1
#define ETH_MODULE_SFF_8079_LEN		256
#endif

#ifndef ETH_MODULE_SFF_8472
#define ETH_MODULE_SFF_8472		0x2
#define ETH_MODULE_SFF_8472_LEN		512
#endif

#ifndef ETH_MODULE_SFF_8636
#define ETH_MODULE_SFF_8636		0x3
#define ETH_MODULE_SFF_8636_LEN		256
#endif

#ifndef ETH_MODULE_SFF_8436
#define ETH_MODULE_SFF_8436		0x4
#define ETH_MODULE_SFF_8436_LEN		256
#endif

#endif
