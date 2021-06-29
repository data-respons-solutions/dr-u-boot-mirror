/*
 * Copyright (C) 2017 Data Respons AS
 *
 * Author: Hans Christian Lonstad <hcl@datarespons.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/iomux.h>
#include <asm/arch/mx6-pins.h>
#include <asm/mach-imx/spi.h>
#include <linux/errno.h>
#include <asm/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/video.h>
#include <mmc.h>
#include <fsl_esdhc.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/arch/mxc_hdmi.h>
#include <asm/arch/crm_regs.h>
#include <asm/io.h>
#include <asm/arch/sys_proto.h>
#include <i2c.h>
#include <input.h>
#include <power/pmic.h>
#include <power/pfuze100_pmic.h>
#include <usb.h>
#include <usb/ehci-ci.h>
#include <environment.h>
#include <usb.h>
#include <pwm.h>
#include <version.h>
#include <watchdog.h>

#include <asm/mach-imx/hab.h>
#include <vsprintf.h>

DECLARE_GLOBAL_DATA_PTR;

#include "../lm-common/lm_common_defs.h"
#include "cargotec_pins.h"
#include "cargotec_gpio.h"

struct fsl_esdhc_cfg usdhc_cfg[2] = {
	{USDHC3_BASE_ADDR},
	{USDHC4_BASE_ADDR},
};

#ifndef CONFIG_SPL_BUILD
static const char* hw_string[8] = {
	"REVA",
	"REVB",
	"REVC",
	"REVD",
	"REVE",
	"FUTURE",
	"FUTURE",
	"FUTURE",
};
#endif

static int get_version(void)
{
	return (((!gpio_get_value(GPIO_HW_SETTING0)) << 2) |
			((!gpio_get_value(GPIO_HW_SETTING1)) << 1) |
			(!gpio_get_value(GPIO_HW_SETTING2)) ) & 7;
}

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();
	return 0;
}


int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC3_BASE_ADDR:
	case USDHC4_BASE_ADDR:
		ret = 1;
		break;
	}

	return ret;
}

int board_mmc_init(bd_t *bis)
{
#ifndef CONFIG_SPL_BUILD
	int ret;


		SETUP_IOMUX_PADS(usdhc3_pads);
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		SETUP_IOMUX_PADS(usdhc4_pads);
		usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);

		ret = fsl_esdhc_initialize(bis, &usdhc_cfg[0]);
		if (ret)
			return ret;
		ret = fsl_esdhc_initialize(bis, &usdhc_cfg[1]);
		if (ret)
			return ret;

	return 0;
#else
	struct src *psrc = (struct src *)SRC_BASE_ADDR;
	unsigned reg = readl(&psrc->sbmr1) >> 11;
	/*
	 * Upon reading BOOT_CFG register the following map is done:
	 * Bit 11 and 12 of BOOT_CFG register can determine the current
	 * mmc port
	 * 0x1                  SD1
	 * 0x2                  SD2
	 * 0x3                  SD4
	 */

	switch (reg & 0x3) {
	case 0x2:
		SETUP_IOMUX_PADS(usdhc3_pads);
		usdhc_cfg[0].esdhc_base = USDHC3_BASE_ADDR;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		gd->arch.sdhc_clk = usdhc_cfg[0].sdhc_clk;
		break;
	case 0x3:
		SETUP_IOMUX_PADS(usdhc4_pads);
		usdhc_cfg[0].esdhc_base = USDHC4_BASE_ADDR;
		usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);
		gd->arch.sdhc_clk = usdhc_cfg[0].sdhc_clk;
		break;
	}

	return fsl_esdhc_initialize(bis, &usdhc_cfg[0]);
#endif
}

int board_usb_phy_mode(int port)
{
	unsigned int bmode = readl(&src_base->sbmr2);

	switch(port) {
	case 0:
#ifdef CONFIG_FACTORY_BOOT
		return USB_INIT_DEVICE;
#else
		if (((bmode >> 24) & 0x03) == 0x01)	{
			printf("USB OTG in serial download mode\n");
			return USB_INIT_DEVICE;
		}
		else
			return USB_INIT_HOST;
#endif
		break;
	case 1:
		return USB_INIT_HOST;
		break;
	default:
		return USB_INIT_DEVICE;
		break;
	}
}

int mx6_rgmii_rework(struct phy_device *phydev)
{
	unsigned short val;
	printf("%s: Setting enet ref clock to 125 MHz\n", __func__);
	/* To enable AR8031 ouput a 125MHz clk from CLK_25M */
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x7);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, 0x8016);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x4007);

	val = phy_read(phydev, MDIO_DEVAD_NONE, 0xe);
	val &= 0xffe3;
	val |= 0x18;
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, val);

	/* introduce tx clock delay */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x5);
	val = phy_read(phydev, MDIO_DEVAD_NONE, 0x1e);
	val |= 0x0100;
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, val);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	mx6_rgmii_rework(phydev);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}


#ifndef CONFIG_SPL_BUILD

static void setup_iomux_enet(void)
{
	SETUP_IOMUX_PADS(enet_pads);
	/* Reset AR8031 PHY */
	gpio_direction_output(IMX_GPIO_NR(1, 25) , 0);

	udelay(500);
	gpio_set_value(IMX_GPIO_NR(1, 25), 1);
}



int board_eth_init(bd_t *bis)
{
	setup_iomux_enet();
	return cpu_eth_init(bis);
}
#endif
#if !defined(CONFIG_SPL_BUILD) && defined(CONFIG_USB_EHCI_MX6)
#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_OVER_CUR_POL	(1 << 8) /* OTG Polarity of Overcurrent */
#define UCTRL_PWR_POL		(1 << 9)



int board_ehci_hcd_init(int port)
{
	u32 *usbnc_usb_ctrl;

	if (port > 2)
		return -EINVAL;

	usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
				 port * 4);
	if (port == 0)
	{
		setbits_le32(usbnc_usb_ctrl, UCTRL_PWR_POL);
		clrbits_le32(usbnc_usb_ctrl, UCTRL_OVER_CUR_POL);
	}

	return 0;
}

int board_ehci_power(int port, int on) {
	int isPowered;
	if (port == 0 && board_usb_phy_mode(0) == USB_INIT_HOST) {
		isPowered = gpio_get_value(GPIO_OTG_PWR) ? 1 : 0;
		gpio_set_value(GPIO_OTG_PWR, on);
		if (!isPowered && on)
			mdelay(600);
	}
	return 0;
}



static void setup_usb(void)
{
	/*
	 * set daisy chain for otg_pin_id on 6q.
	 * for 6dl, this bit is reserved
	 */
	if (is_mx6dq())
		imx_iomux_set_gpr_register(1, 13, 1, 0);
}
#endif

int board_early_init_f(void)
{
	SETUP_IOMUX_PADS(hw_settings_pads);
	gpio_direction_input(GPIO_HW_SETTING0);
	gpio_direction_input(GPIO_HW_SETTING1);
	gpio_direction_input(GPIO_HW_SETTING2);

	SETUP_IOMUX_PADS(otg_pads);

	SETUP_IOMUX_PADS(uart1_pads);
	SETUP_IOMUX_PADS(uart2_pads);
	SETUP_IOMUX_PADS(uart3_pads);
	SETUP_IOMUX_PADS(uart4_pads);

	SETUP_IOMUX_PADS(can1_pads);
	SETUP_IOMUX_PADS(can2_pads);
	SETUP_IOMUX_PADS(ecspi1_pads);
	SETUP_IOMUX_PADS(ecspi2_pads);
	SETUP_IOMUX_PADS(other_pads);
	SETUP_IOMUX_PADS(wwan_pads);

	/* gpio_direction_output(GP_ENABLE_LC_UART, 0);	 Disable LC uart driver */
	gpio_direction_output(TPM_RESET_N, 0);
	gpio_direction_output(GPIO_WL_BAT_PWR_EN, 0);
	gpio_direction_output(GPIO_WL_VDDIO_EN, 0);
	gpio_direction_output(GPIO_WL_DEV_WAKE, 0);

	gpio_direction_output(GPIO_WL_REG_ON, 0);		/* WiFI off */
	gpio_direction_output(GPIO_BT_REG_ON, 0);		/* Bluetooth off */

	gpio_direction_input(GPIO_PWR_BTN);

	gpio_direction_input(CAN1_WAKE);
	gpio_direction_output(CAN1_EN, 0);
	gpio_direction_output(CAN1_STB_N, 1);
	gpio_direction_output(CAN1_RES_EN, 0);

	gpio_direction_input(CAN2_WAKE);
	gpio_direction_output(CAN2_EN, 0);
	gpio_direction_output(CAN2_STB_N, 1);
	gpio_direction_output(CAN2_RES_EN, 0);

	gpio_direction_output(EN_ANI1, 0);
	gpio_direction_output(EN_ANI2, 0);

	gpio_direction_input(GP_NINT_MCU);


	gpio_direction_output(GP_EN_GPO1, 0);
	gpio_direction_output(GP_EN_GPO2, 0);
	gpio_direction_output(GP_EN_PWR_ANT_GPS, 0);
	gpio_direction_output(GP_PRST_WWAN_N, 0);
	gpio_direction_output(GP_DISABLE_WWAN_N, 0);

	int version = get_version();

	if (version == 0)
		SETUP_IOMUX_PADS(uart5_pads_dte);
	else
		SETUP_IOMUX_PADS(uart5_pads_dce);

	if (version >= 2) {
		SETUP_IOMUX_PADS(revc_pads);
		gpio_direction_input(STATUS_CPU);
		gpio_direction_output(GP_LED_G_STATUS, 0);
		gpio_direction_output(GP_LED_R_STATUS, 0);
		gpio_direction_output(GP_NRST_MCU, 0);
		gpio_direction_output(GPIO_OTG_PWR, 0);
	}
	else {
		SETUP_IOMUX_PADS(revab_pads);
		gpio_direction_output(STATUS_CPU, 1);
		gpio_direction_output(GP_NRST_MCU, 1);
	}

	if (is_mx6dq()) {
		setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6q_i2c_pad_info0);
		setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6q_i2c_pad_info1);
		setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6q_i2c_pad_info2);
	}
	else {
		setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6dl_i2c_pad_info0);
		setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6dl_i2c_pad_info1);
		setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6dl_i2c_pad_info2);
		if (version == 0)
			setup_i2c(3, CONFIG_SYS_I2C_SPEED, 0x7f, &mx6dl_i2c_pad_info3);
	}
	return 0;
}

#ifndef CONFIG_SPL_BUILD
static char * const usbcmd[] = {"usb", "start"};


int board_init(void)
{
	int rep;
	ulong ticks;
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;
	gpio_set_value(TPM_RESET_N, 1);
	setup_usb();
	gpio_set_value(GP_PRST_WWAN_N, 1);
	gpio_set_value(GP_DISABLE_WWAN_N, 1);
	cmd_process(0, 2, usbcmd, &rep, &ticks);
	return 0;
}
#endif	/* CONFIG_SPL_BUILD */

int board_spi_cs_gpio(unsigned bus, unsigned cs)
{
	switch (bus)
	{
	case 0:
		return cs == 1 ? SPI_CS_GPIO : -1;
		break;
	default:
		return -1;
	}
}


#ifndef CONFIG_SPL_BUILD


int board_late_init(void)
{
	int version = get_version();
	switch (version)
	{
	case 0:
		env_set("fdt_file", "/boot/cargotec-gw-revA.dtb");
		break;

	case 1:
		if (is_mx6dl())
			env_set("fdt_file", "/boot/cargotec-gw-revB-dl.dtb");
		else
			env_set("fdt_file", "/boot/cargotec-gw-revB-q.dtb");
		break;

	default:
		if (is_mx6dl())
			env_set("fdt_file", "/boot/cargotec-gw-revC-dl.dtb");
		else
			env_set("fdt_file", "/boot/cargotec-gw-revC-q.dtb");
		break;

	}

	printf("CARGOTEC GW version: %s\n", hw_string[version]);

	printf("U-BOOT version [%s]\n", U_BOOT_VERSION);
#ifdef CONFIG_SECURE_BOOT
	if (imx_hab_is_enabled())
	{
		printf("HAB enabled, setting up secure bootscript\n");
		env_set("bootscript", BOOTSCRIPT_SECURE);
		env_set("zimage", ZIMAGE_SECURE);
		env_set("initrd_file", "/boot/initrd-ivt_signed");
#ifdef CONFIG_BOOT_USB_INITRD
		env_set("bootscript_usb", BOOTSCRIPT_USB_SECURE);
#else
		env_set("bootscript_usb", BOOTSCRIPT_SECURE);
#endif
		//env_set("bootdelay", "0");
	}
	else
	{
		printf("HAB disabled, setting up regular bootscript\n");
		env_set("bootscript", BOOTSCRIPT_NOSECURE);
		env_set("bootscript_usb", BOOTSCRIPT_NOSECURE);
	}
#else
	env_set("bootscript", BOOTSCRIPT_NOSECURE);
	env_set("bootscript_usb", BOOTSCRIPT_NOSECURE);
#endif

	return 0;
}
#endif

int checkboard(void)
{
	puts("Board: Cargotec GW\n");
	return 0;
}

static int do_led(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	if (argc < 2)
		return CMD_RET_USAGE;
	if (get_version() < 2)
		return 0;

	if (strncmp(argv[1], "red", 3) == 0)
	{
		gpio_set_value(GP_LED_G_STATUS, 0);
		gpio_set_value(GP_LED_R_STATUS, 1);
		return 0;
	}

	if (strncmp(argv[1], "green", 5) == 0)
	{
		gpio_set_value(GP_LED_G_STATUS, 1);
		gpio_set_value(GP_LED_R_STATUS, 0);
		return 0;
	}
	if (strncmp(argv[1], "off", 3) == 0)
	{
		gpio_set_value(GP_LED_G_STATUS, 0);
		gpio_set_value(GP_LED_R_STATUS, 0);
		return 0;
	}
	if (strncmp(argv[1], "orange", 6) == 0)
	{
		gpio_set_value(GP_LED_G_STATUS, 1);
		gpio_set_value(GP_LED_R_STATUS, 1);
		return 0;
	}
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	set_led, 2, 1, do_led, "LED control",
	"set_led color [red,green,off]\n"
	);

#ifdef CONFIG_SPL_BUILD
#include <asm/arch/mx6-ddr.h>
#include <spl.h>

const struct mx6sdl_iomux_ddr_regs mx6dl_ddr_ioregs = {
	.dram_sdclk_0 =  0x00020030,
	.dram_sdclk_1 =  0x00020030,
	.dram_cas =  0x00020030,
	.dram_ras =  0x00020030,
	.dram_reset =  0x00020030,
	.dram_sdcke0 =  0x00003000,
	.dram_sdcke1 =  0x00003000,
	.dram_sdba2 =  0x00000000,
	.dram_sdodt0 =  0x00003030,
	.dram_sdodt1 =  0x00003030,
	.dram_sdqs0 =  0x00000030,
	.dram_sdqs1 =  0x00000030,
	.dram_sdqs2 =  0x00000030,
	.dram_sdqs3 =  0x00000030,
	.dram_sdqs4 =  0x00000030,
	.dram_sdqs5 =  0x00000030,
	.dram_sdqs6 =  0x00000030,
	.dram_sdqs7 =  0x00000030,
	.dram_dqm0 =  0x00020030,
	.dram_dqm1 =  0x00020030,
	.dram_dqm2 =  0x00020030,
	.dram_dqm3 =  0x00020030,
	.dram_dqm4 =  0x00020030,
	.dram_dqm5 =  0x00020030,
	.dram_dqm6 =  0x00020030,
	.dram_dqm7 =  0x00020030,
};

const struct mx6sdl_iomux_grp_regs mx6dl_grp_ioregs = {
	.grp_ddr_type =  0x000C0000,
	.grp_ddrmode_ctl =  0x00020000,
	.grp_ddrpke =  0x00000000,
	.grp_addds =  0x00000030,
	.grp_ctlds =  0x00000030,
	.grp_ddrmode =  0x00020000,
	.grp_b0ds =  0x00000030,
	.grp_b1ds =  0x00000030,
	.grp_b2ds =  0x00000030,
	.grp_b3ds =  0x00000030,
	.grp_b4ds =  0x00000030,
	.grp_b5ds =  0x00000030,
	.grp_b6ds =  0x00000030,
	.grp_b7ds =  0x00000030,
};

const struct mx6dq_iomux_ddr_regs mx6q_ddr_ioregs = {
	.dram_sdclk_0 =  0x00020030,
	.dram_sdclk_1 =  0x00020030,
	.dram_cas =  0x00020030,
	.dram_ras =  0x00020030,
	.dram_reset =  0x00020030,
	.dram_sdcke0 =  0x00003000,
	.dram_sdcke1 =  0x00003000,
	.dram_sdba2 =  0x00000000,
	.dram_sdodt0 =  0x00003030,
	.dram_sdodt1 =  0x00003030,
	.dram_sdqs0 =  0x00000030,
	.dram_sdqs1 =  0x00000030,
	.dram_sdqs2 =  0x00000030,
	.dram_sdqs3 =  0x00000030,
	.dram_sdqs4 =  0x00000030,
	.dram_sdqs5 =  0x00000030,
	.dram_sdqs6 =  0x00000030,
	.dram_sdqs7 =  0x00000030,
	.dram_dqm0 =  0x00020030,
	.dram_dqm1 =  0x00020030,
	.dram_dqm2 =  0x00020030,
	.dram_dqm3 =  0x00020030,
	.dram_dqm4 =  0x00020030,
	.dram_dqm5 =  0x00020030,
	.dram_dqm6 =  0x00020030,
	.dram_dqm7 =  0x00020030,
};

const struct mx6dq_iomux_grp_regs mx6q_grp_ioregs = {
	.grp_ddr_type =  0x000C0000,
	.grp_ddrmode_ctl =  0x00020000,
	.grp_ddrpke =  0x00000000,
	.grp_addds =  0x00000030,
	.grp_ctlds =  0x00000030,
	.grp_ddrmode =  0x00020000,
	.grp_b0ds =  0x00000030,
	.grp_b1ds =  0x00000030,
	.grp_b2ds =  0x00000030,
	.grp_b3ds =  0x00000030,
	.grp_b4ds =  0x00000030,
	.grp_b5ds =  0x00000030,
	.grp_b6ds =  0x00000030,
	.grp_b7ds =  0x00000030,
};

const struct mx6_mmdc_calibration mx6_mmcd_calib = {
	.p0_mpwldectrl0 =  0x001F001F,
	.p0_mpwldectrl1 =  0x001F001F,
	.p1_mpwldectrl0 =  0x001F001F,
	.p1_mpwldectrl1 =  0x001F001F,
	.p0_mpdgctrl0 =  0x40404040,
	.p0_mpdgctrl1 =  0x40404040,
	.p1_mpdgctrl0 =  0x40404040,
	.p1_mpdgctrl1 =  0x40404040,
	.p0_mprddlctl =  0x40404040,
	.p1_mprddlctl =  0x40404040,
	.p0_mpwrdlctl =  0x40404040,
	.p1_mpwrdlctl =  0x40404040,
};

const struct mx6_mmdc_calibration mx6q_mmcd_calib = {
	.p0_mpwldectrl0 =  0x001B0017,
	.p0_mpwldectrl1 =  0x0023001D,

	.p0_mpdgctrl0 =  0x03040318,
	.p0_mpdgctrl1 =  0x03040268,

	.p0_mprddlctl =  0x42323840,
	.p0_mpwrdlctl =  0x3A3A403A,
};

const struct mx6_mmdc_calibration mx6dl_mmcd_calib = {
	.p0_mpwldectrl0 =  0x0048004D,
	.p0_mpwldectrl1 =  0x003F0041,

	.p0_mpdgctrl0 =  0x423C023C,
	.p0_mpdgctrl1 =  0x02280224,

	.p0_mprddlctl =  0x40424A46,
	.p0_mpwrdlctl =  0x3638322E,
};

static struct mx6_ddr3_cfg mem_ddr_256x16 = {
	.mem_speed = 1600,
	.density = 4,
	.width = 16,
	.banks = 8,
	.rowaddr = 15,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
};

static struct mx6_ddr3_cfg mem_ddr_512x16 = {
	.mem_speed = 1600,
	.density = 4,
	.width = 16,
	.banks = 8,
	.rowaddr = 16,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
};

static struct mx6_ddr_sysinfo sysinfo = {
	/* width of data bus:0=16,1=32,2=64 */
	.dsize = 1,
	/* config for full 4GB range so that get_mem_size() works */
	.cs_density = 32, /* 32Gb per CS */
	/* single chip select */
	.ncs = 1,
	.cs1_mirror = 0,
	.rtt_wr = 1 /*DDR3_RTT_60_OHM*/,	/* RTT_Wr = RZQ/4 */
#ifdef RTT_NOM_120OHM
	.rtt_nom = 2 /*DDR3_RTT_120_OHM*/,	/* RTT_Nom = RZQ/2 */
#else
	.rtt_nom = 1 /*DDR3_RTT_60_OHM*/,	/* RTT_Nom = RZQ/4 */
#endif
	.walat = 1,	/* Write additional latency */
	.ralat = 5,	/* Read additional latency */
	.mif3_mode = 3,	/* Command prediction working mode */
	.bi_on = 1,	/* Bank interleaving enabled */
	.sde_to_rst = 0x10,	/* 14 cycles, 200us (JEDEC default) */
	.rst_to_cke = 0x23,	/* 33 cycles, 500us (JEDEC default) */
	.ddr_type = DDR_TYPE_DDR3,
	.refsel = 1,	/* Refresh cycles at 32KHz */
	.refr = 7,	/* 8 refresh commands per refresh cycle */
};

static void ccgr_init(void)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	writel(0x00C03F3F, &ccm->CCGR0);
	writel(0x0030FF03, &ccm->CCGR1);
	writel(0x0FFFC000, &ccm->CCGR2);
	writel(0x3FF00000, &ccm->CCGR3);
	writel(0x00FFF300, &ccm->CCGR4);
	writel(0x0F0000C3, &ccm->CCGR5);
	writel(0x000003FF, &ccm->CCGR6);
}

int pmic_setup(int bus)
{
	int ret;
	i2c_set_bus_num(bus);
	ret = i2c_probe(CONFIG_POWER_PFUZE100_I2C_ADDR);
	if (ret)
	{
		printf("%s: no pmic\n", __func__);
		return -ENODEV;
	}
	return 0;
}


static int pmic_set(int bus, pf100_regs reg, int mV)
{
	u8 values[2];
	i2c_set_bus_num(bus);
	switch (reg) {

	case SW1AB:
		if (mV > 1425) {
			printf("%s: SW1AB max is 1425 mV, reject %d mV\n", __func__, mV);
			return -EINVAL;
		}
		printf("Setting PMIC register SW1AB to %d mV\n", mV);
		values[0] = (mV - 300) / 25;
		i2c_write(0x08, PFUZE100_SW1ABVOL, 1, values, 1);
		break;

	case SW1C:
		if (mV > 1425) {
			printf("%s: SW1AB max is 1425 mV, reject %d mV\n", __func__, mV);
			return -EINVAL;
		}
		printf("Setting PMIC register SW1C to %d mV\n", mV);
		values[0] = (mV - 300) / 25;
		i2c_write(0x08, PFUZE100_SW1CVOL, 1, values, 1);
		break;

	case SW3AB:
		if (mV > 1500) {
			printf("%s: SW3AB max is 1500 mV, reject %d mV\n", __func__, mV);
			return -EINVAL;
		}
		printf("Setting PMIC register SW3AB to %d mV\n", mV);
		values[0] = (mV - 400) / 25;
		i2c_write(0x08, PFUZE100_SW3AVOL, 1, values, 1);
		i2c_write(0x08, PFUZE100_SW3BVOL, 1, values, 1);
		break;

	case VGEN4:
		printf("Setting PMIC register VGEN4 to %d mV\n", mV);
		values[0] = ((mV - 1800) * 15) / 1500;
		i2c_write(0x08, PFUZE100_VGEN4VOL, 1, values, 1);
		break;
	}
	return 0;
}



/*
 * This section requires the differentiation between iMX6 Sabre boards, but
 * for now, it will configure only for the mx6q variant.
 */
static void spl_dram_init(void)
{
#ifdef GPIO_DDR_SETTING
	int four_chip = gpio_get_value(GPIO_DDR_SETTING);
#endif

	if (is_mx6dl())
	{
		mx6sdl_dram_iocfg(32, &mx6dl_ddr_ioregs, &mx6dl_grp_ioregs);
		sysinfo.dsize = 1;
		mx6_dram_cfg(&sysinfo, &mx6dl_mmcd_calib, &mem_ddr_256x16);
	}
	else
	{
		sysinfo.dsize = 1;
		mx6dq_dram_iocfg(32, &mx6q_ddr_ioregs, &mx6q_grp_ioregs);
		mx6_dram_cfg(&sysinfo, &mx6q_mmcd_calib, &mem_ddr_256x16);
	}
}

static void do_hang_error(void)
{

	for(;;) {
		udelay(1000000);
	}

}

static struct mx6_mmdc_calibration calib;

void board_init_f(ulong dummy)
{
	int err;
	int pmic_bus = 3;
	/* setup AIPS and disable watchdog */
	arch_cpu_init();

	ccgr_init();
	gpr_init();
#ifdef CONFIG_SPL_WATCHDOG_SUPPORT
	void hw_watchdog_init(void);
	hw_watchdog_init();
#endif
	/* iomux and setup of i2c */
	board_early_init_f();

	/* setup GP timer */
	timer_init();

	/* UART clocks enabled and gd valid - init serial console */
	preloader_console_init();
	int version = get_version();
	printf("SPL started on version %d\n", version);
	if (version >= 1 || is_mx6dq())
		pmic_bus = 1;

	err = pmic_setup(pmic_bus);
	if (err == 0) {
		pmic_set(pmic_bus, SW1AB, 1425);
		pmic_set(pmic_bus, SW1C, 1425);
		pmic_set(pmic_bus, SW3AB, 1350);
		udelay(10000);
	}
	/* DDR initialization */
	printf("Memory width %d\n", is_mx6dl() ? 32 : 32);

	spl_dram_init();
#ifdef DYNAMIC_CALIB
	err = mmdc_do_write_level_calibration(&sysinfo);
	if (err & 0x03) {
		printf("DDR3 write level calibration error - hang\n");
		do_hang_error();

	}
	err = mmdc_do_dqs_calibration(&sysinfo);
	if (err) {
		printf("DDR3 DQS calibration error - hang\n");
		do_hang_error();
	}

	mmdc_read_calibration(&sysinfo, &calib);
	printf("Calibration results for PHY0\n");
	printf("mpwldectrl0 = %08x\n", calib.p0_mpwldectrl0);
	printf("mpwldectrl1 = %08x\n", calib.p0_mpwldectrl1);
	printf("mpdgctrl0   = %08x\n", calib.p0_mpdgctrl0);
	printf("mpdgctrl1   = %08x\n", calib.p0_mpdgctrl1);
	printf("mprddlctl   = %08x\n", calib.p0_mprddlctl);
	printf("mpwrdlctl   = %08x\n", calib.p0_mpwrdlctl);
	if (sysinfo.dsize == 2) {
		printf("\nCalibration results for PHY1\n");
		printf("mpwldectrl0 = %08x\n", calib.p1_mpwldectrl0);
		printf("mpwldectrl1 = %08x\n", calib.p1_mpwldectrl1);
		printf("mpdgctrl0   = %08x\n", calib.p1_mpdgctrl0);
		printf("mpdgctrl1   = %08x\n", calib.p1_mpdgctrl1);
		printf("mprddlctl   = %08x\n", calib.p1_mprddlctl);
		printf("mpwrdlctl   = %08x\n", calib.p1_mpwrdlctl);
	}
#endif

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	/* load/boot image from boot device */
	board_init_r(NULL, 0);
}


#endif
