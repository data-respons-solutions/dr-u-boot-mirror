/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2018 NXP
 */

#ifndef __VHGW_H
#define __VHGW_H

#include <linux/sizes.h>
#include <linux/stringify.h>
#include <asm/arch/imx-regs.h>

#define CONFIG_SPL_MAX_SIZE		(208 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		0x96fff0
#define CONFIG_SPL_BSS_START_ADDR      0x954000
#define CONFIG_MALLOC_F_ADDR 0x970000
#define CONFIG_SPL_BSS_MAX_SIZE		SZ_8K	/* 8 KB */
#define CONFIG_SYS_SPL_MALLOC_START	0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE	SZ_512K	/* 512 KB */

/* For RAW image gives a error info not panic */
#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#endif

#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG
#define CONFIG_FASTBOOT_USB_DEV 0

#define CONFIG_REMAKE_ELF
/* ENET Config */
/* ENET1 */
#if defined(CONFIG_FEC_MXC)
#define CONFIG_ETHPRIME                 "FEC"
#define PHY_ANEG_TIMEOUT 20000

#define CONFIG_FEC_XCV_TYPE             RGMII
#define CONFIG_FEC_MXC_PHYADDR          0

#define IMX_FEC_BASE			0x30BE0000
#endif

/* nxp shenanigans */
#define CONFIG_SYS_MMC_ENV_DEV 2 /* Can't disable arch/arm/mach-imx/mmc_env.c */
#undef is_boot_from_usb /* If defined overwrites bootcmd */

/* boot configuration */
#define CONFIG_SYS_BOOTM_LEN (1024 * 1024 * 512) /* fit image might include kernel and ramdisk, increase size */
#define SYS_BOOT_IFACE "mmc" /* Required by android_boot */
#define SYS_BOOT_DEV 2 /* Required by android_boot */
#define FIT_ADDR 0x43400000
#define FIT_IMAGE "/boot/fitImage"


/*
 * Boot order:
 * - USB partition with label TESTDRIVE
 * - mmc2 partition with label rootfs1
 */
#define CONFIG_BOOTCOMMAND \
	"echo starting boot procedure...;" \
	"i2c dev 1;" \
	"i2c mw 0x50 0x23 0x77 1;" \
	"sleep 1;" \
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

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		\
	"soc_type=imx8mm\0" /* Required by Android */


/* Link Definitions */
#define CONFIG_SYS_INIT_RAM_ADDR        0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE        0x80000
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_MMCROOT			"/dev/mmcblk1p2"  /* USDHC2 */

#define CONFIG_SYS_SDRAM_BASE           0x40000000
#define PHYS_SDRAM                      0x40000000
#define PHYS_SDRAM_SIZE					0x40000000 /* 1GB DDR */


#define CONFIG_MXC_UART_BASE		UART2_BASE_ADDR

/* Monitor Command Prompt */
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)

#define CONFIG_IMX_BOOTAUX

/* USDHC */

#define CONFIG_SYS_FSL_USDHC_NUM	2
#define CONFIG_SYS_FSL_ESDHC_ADDR	0

/* USB configs */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_USBD_HS
#endif

#define CONFIG_USB_GADGET_VBUS_DRAW 2

#define CONFIG_MXC_USB_PORTSC  (PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_USB_MAX_CONTROLLER_COUNT         2

#endif // __VHGW_H
