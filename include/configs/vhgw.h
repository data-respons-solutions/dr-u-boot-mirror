// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2022, Data Respons Solutions AB
 *
 */

#ifndef VHGW_H__
#define VHGW_H__

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#define CONFIG_SYS_MONITOR_LEN		SZ_512K
#define CONFIG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

/* boot configuration */
#define FIT_ADDR 0x43400000
#define FIT_IMAGE "/boot/fitImage"

/*
 * Boot order:
 * - USB partition with label TESTDRIVE
 * - mmc2 partition with label rootfs1
 */
#define CONFIG_BOOTCOMMAND \
	"echo starting boot procedure...;" \
	"if usb start; then " \
		"if system_load usb 0 --label TESTDRIVE; then " \
			"usb stop;" \
			"system_boot;" \
		"else " \
			"usb stop;" \
		"fi;" \
	"fi;" \
	"if system_load mmc 2; then " \
		"system_boot;" \
	"fi;" \
	"echo no boot device found;"

/* Link Definitions */
#define CONFIG_SYS_INIT_RAM_ADDR	0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE	0x200000
#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM					0x40000000
#define PHYS_SDRAM_SIZE				0x40000000 /* 1GB DDR */

#endif // VHGW_H__
