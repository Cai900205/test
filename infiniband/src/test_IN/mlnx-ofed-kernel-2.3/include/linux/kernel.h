#ifndef _COMPAT_KERNEL_
#define _COMPAT_KERNEL_

#include_next <linux/kernel.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0))

#include <linux/compiler.h>

/*
 * This backports:
 *
 *   From a3860c1c5dd1137db23d7786d284939c5761d517 Mon Sep 17 00:00:00 2001
 *   From: Xi Wang <xi.wang@gmail.com>
 *   Date: Thu, 31 May 2012 16:26:04 -0700
 *   Subject: [PATCH] introduce SIZE_MAX
 */

#define SIZE_MAX    (~(size_t)0)

#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18) || defined(COMPAT_VMWARE))
extern __printf(2, 3)
char *kasprintf(gfp_t gfp, const char *fmt, ...);
extern char *kvasprintf(gfp_t gfp, const char *fmt, va_list args);
#endif

#endif /* _COMPAT_KERNEL_ */
