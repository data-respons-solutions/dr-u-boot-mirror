/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2019 NXP
 */

#ifndef __SDB8000_H
#define __SDB8000_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

/* SPL */
#define CONFIG_SPL_MAX_SIZE		(148 * 1024)
#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		0x920000
#define CONFIG_SPL_BSS_START_ADDR	0x910000
#define CONFIG_SPL_BSS_MAX_SIZE		SZ_8K	/* 8 KB */
#define CONFIG_SYS_SPL_MALLOC_START	0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE	SZ_512K	/* 512 KB */
#define CONFIG_MALLOC_F_ADDR		0x912000 /* pre-realloc */
#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_BD71837
#define CONFIG_SYS_I2C
#endif

/* iMX8MM */
#define CONFIG_SERIAL_TAG
#define CONFIG_IMX_BOOTAUX

/* nxp shenanigans */
#define CONFIG_SYS_MMC_ENV_DEV 1 /* Can't disable arch/arm/mach-imx/mmc_env.c */
#undef is_boot_from_usb /* If defined overwrites bootcmd */

/* boot configuration */
#define CONFIG_SYS_BOOTM_LEN (1024 * 1024 * 512) /* fit image might include kernel and ramdisk, increase size */
#define DEFAULT_USB_DEV "0"
#define DEFAULT_USB_PART "1"
#define SYS_BOOT_IFACE "mmc"
#define SYS_BOOT_DEV 2
#define FIT_ADDR "0x43400000"

#include "../../board/datarespons/common/include/configs/datarespons.h"

#define CONFIG_EXTRA_ENV_SETTINGS \
	DATARESPONS_BOOT_SCRIPTS

#define CONFIG_BOOTCOMMAND \
	"echo starting boot procedure...;" \
	"run bootpreloaded;" \
	"run bootsys;" \
	"echo no boot device found;"

/* memory */
#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM					0x40000000
#define PHYS_SDRAM_SIZE				0x80000000 /* 2GB DDR */
#define CONFIG_SYS_MONITOR_LEN		SZ_512K
#define CONFIG_LOADADDR				0x40600000
#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_INIT_RAM_ADDR	0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE	0x200000
#define CONFIG_SYS_INIT_SP_OFFSET	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)
#define CONFIG_SYS_MALLOC_LEN		SZ_32M

/* Monitor Command Prompt */
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)

/* UART */
#define CONFIG_MXC_UART_BASE	UART4_BASE_ADDR

/* USDHC */
#define CONFIG_FSL_USDHC

/* i2c */
#define CONFIG_SYS_I2C_SPEED	100000

/* Net */
#define CONFIG_ETHPRIME			"FEC"
#define PHY_ANEG_TIMEOUT		20000
#define CONFIG_FEC_XCV_TYPE		RGMII
#define CONFIG_FEC_MXC_PHYADDR		0
#define FEC_QUIRK_ENET_MAC
#define IMX_FEC_BASE			0x30BE0000

/* USB */
#define CONFIG_USBD_HS
#define CONFIG_USB_GADGET_VBUS_DRAW 2
#define CONFIG_MXC_USB_PORTSC  (PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_USB_MAX_CONTROLLER_COUNT 2

#endif // __SDB8000_H
