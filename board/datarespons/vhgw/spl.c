// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2018-2019, 2021 NXP
 *
 */

#include <common.h>
#include <cpu_func.h>
#include <hang.h>
#include <init.h>
#include <spl.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx8mn_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/arch/ddr.h>
#include <spi_flash.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <power/pmic.h>
#include <power/pca9450.h>
#include <power/bd71837.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>

DECLARE_GLOBAL_DATA_PTR;

int spl_board_boot_device(enum boot_device boot_dev_spl)
{
	switch (boot_dev_spl) {
	case SD1_BOOT:
		return BOOT_DEVICE_SPI;
	case USB_BOOT:
		return BOOT_DEVICE_BOOTROM;
	default:
		return BOOT_DEVICE_NONE;
	}
}

/*
 * u-boot image is stored in two flash sections for power fail safe updates. We avoid having to re-write spi
 * loader by calling existing functions twice (board_boot_order()) and adjusting offset from here (spl_spi_get_uboot_offs()).
 */
void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = spl_boot_device();
	spl_boot_list[1] = spl_boot_device();
}
unsigned int spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	static int i = 0;
	const unsigned int offs = i == 0 ? CONFIG_SYS_SPI_U_BOOT_OFFS : CONFIG_SYS_SPI_U_BOOT2_OFFS;
	i++;
	printf("SPI offs: 0x%x\n", offs);
	return offs;
}

void spl_dram_init(void)
{
	ddr_init(&dram_timing);
}

#if CONFIG_IS_ENABLED(DM_PMIC_BD71837)
int power_init_board(void)
{
	struct udevice *dev;
	int ret;

	ret = pmic_get("pmic@4b", &dev);
	if (ret == -ENODEV) {
		puts("No pmic@4b\n");
		return 0;
	}
	if (ret != 0)
		return ret;

	/* decrease RESET key long push time from the default 10s to 10ms */
	pmic_reg_write(dev, BD718XX_PWRONCONFIG1, 0x0);

	/* unlock the PMIC regs */
	pmic_reg_write(dev, BD718XX_REGLOCK, 0x1);

	/* Set VDD_ARM to typical value 0.85v for 1.2Ghz */
	pmic_reg_write(dev, BD718XX_BUCK2_VOLT_RUN, 0xf);

#ifdef CONFIG_IMX8MN_LOW_DRIVE_MODE
	/* Set VDD_SOC/VDD_DRAM to typical value 0.8v for low drive mode */
	pmic_reg_write(dev, BD718XX_BUCK1_VOLT_RUN, 0xa);
#else
	/* Set VDD_SOC/VDD_DRAM to typical value 0.85v for nominal mode */
	pmic_reg_write(dev, BD718XX_BUCK1_VOLT_RUN, 0xf);
#endif /* CONFIG_IMX8MN_LOW_DRIVE_MODE */

	/* Set VDD_SOC 0.75v for low-v suspend */
	pmic_reg_write(dev, BD718XX_BUCK1_VOLT_SUSP, 0x5);

	/* increase NVCC_DRAM_1V2 to 1.2v for DDR4 */
	pmic_reg_write(dev, BD718XX_4TH_NODVS_BUCK_VOLT, 0x28);

	/* lock the PMIC regs */
	pmic_reg_write(dev, BD718XX_REGLOCK, 0x11);

	return 0;
}
#endif

#if CONFIG_IS_ENABLED(DM_PMIC_PCA9450)
int power_init_board(void)
{
	struct udevice *dev;
	int ret;

	ret = pmic_get("pca9450@25", &dev);
	if (ret == -ENODEV) {
		puts("No pca9450@25\n");
		return 0;
	}
	if (ret != 0)
		return ret;

	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	pmic_reg_write(dev, PCA9450_BUCK123_DVS, 0x29);

#ifdef CONFIG_IMX8MN_LOW_DRIVE_MODE
	/* Set VDD_SOC/VDD_DRAM to 0.8v for low drive mode */
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS0, 0x10);
#elif defined(CONFIG_TARGET_IMX8MN_DDR3_EVK)
	/* Set VDD_SOC to 0.85v for DDR3L at 1600MTS */
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS0, 0x14);

	/* Disable the BUCK2 */
	pmic_reg_write(dev, PCA9450_BUCK2CTRL, 0x48);

	/* Set NVCC_DRAM to 1.35v */
	pmic_reg_write(dev, PCA9450_BUCK6OUT, 0x1E);
#else
	/* increase VDD_SOC/VDD_DRAM to typical value 0.95V before first DRAM access */
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS0, 0x1C);
#endif
	/* Set DVS1 to 0.75v for low-v suspend */
	/* Enable DVS control through PMIC_STBY_REQ and set B1_ENMODE=1 (ON by PMIC_ON_REQ=H) */
	pmic_reg_write(dev, PCA9450_BUCK1OUT_DVS1, 0xC);
	pmic_reg_write(dev, PCA9450_BUCK1CTRL, 0x59);

	/* set VDD_SNVS_0V8 from default 0.85V */
	pmic_reg_write(dev, PCA9450_LDO2CTRL, 0xC0);

	/* enable LDO4 to 1.2v */
	pmic_reg_write(dev, PCA9450_LDO4CTRL, 0x44);

	/* set WDOG_B_CFG to cold reset */
	pmic_reg_write(dev, PCA9450_RESET_CTRL, 0xA1);

	return 0;
}
#endif

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)
#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MN_PAD_UART2_RXD__UART2_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MN_PAD_UART2_TXD__UART2_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MN_PAD_GPIO1_IO02__WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	init_uart_clk(1);

	return 0;
}

void spl_board_init(void)
{
	struct udevice *dev;
	int ret;

	if (IS_ENABLED(CONFIG_FSL_CAAM)) {
		ret = uclass_get_device_by_driver(UCLASS_MISC, DM_DRIVER_GET(caam_jr), &dev);
		if (ret)
			printf("Failed to initialize caam_jr: %d\n", ret);
	}
	puts("Normal Boot\n");
}

int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}

void board_init_f(ulong dummy)
{
	struct udevice *dev;
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	board_early_init_f();

	timer_init();

	preloader_console_init();

	ret = spl_early_init();
	if (ret) {
		debug("spl_early_init() failed: %d\n", ret);
		hang();
	}

	ret = uclass_get_device_by_name(UCLASS_CLK,
					"clock-controller@30380000",
					&dev);
	if (ret < 0) {
		printf("Failed to find clock node. Check device tree\n");
		hang();
	}

	enable_tzc380();

	power_init_board();

	/* DDR initialization */
	spl_dram_init();

	board_init_r(NULL, 0);
}
