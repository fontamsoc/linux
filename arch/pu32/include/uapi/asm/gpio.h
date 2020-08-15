// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
// (c) William Fonkou Tambe

#ifndef __UAPI_ASM_PU32_GPIO_H
#define __UAPI_ASM_PU32_GPIO_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define GPIO_IOCTL_BASE 'g'

#define GPIO_IOCTL_USE_BIN  _IOW(GPIO_IOCTL_BASE, 0x1, unsigned long)
#define GPIO_IOCTL_USE_WQ   _IOW(GPIO_IOCTL_BASE, 0x2, unsigned long)
#define GPIO_IOCTL_MASK     _IOW(GPIO_IOCTL_BASE, 0x3, unsigned long)
#define GPIO_IOCTL_IODIR    _IOW(GPIO_IOCTL_BASE, 0x4, unsigned long)
#define GPIO_IOCTL_DBNCR_HZ _IOW(GPIO_IOCTL_BASE, 0x5, unsigned long)
#define GPIO_IOCTL_IOCNT    _IOW(GPIO_IOCTL_BASE, 0x6, unsigned long)

#endif /* __UAPI_ASM_PU32_GPIO_H */
