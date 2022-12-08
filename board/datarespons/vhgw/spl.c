// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2022, Data Respons Solutions AB
 *
 */

#include <common.h>
#include <hang.h>
#include <init.h>
#include <spl.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/arch/ddr.h>

#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <power/pmic.h>
#include <power/bd71837.h>

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
#if 0
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
#endif

void spl_dram_init(void)
{
	ddr_init(&dram_timing);
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

	ret = uclass_get_device_by_name(UCLASS_CLK,
					"clock-controller@30380000",
					&dev);
	if (ret < 0)
		printf("Failed to find clock node. Check device tree\n");
}

int power_init_board(void)
{
	struct udevice *dev;
	int ret;

	ret = pmic_get("bd71850@4b", &dev);
	if (ret != 0) {
		puts("No bd71850@4b\n");
		return ret;
	}

	/* decrease RESET key long push time from the default 10s to 10ms */
	pmic_reg_write(dev, BD718XX_PWRONCONFIG1, 0x0);

	/* unlock the PMIC regs */
	pmic_reg_write(dev, BD718XX_REGLOCK, 0x1);

	/* Set VDD_SOC to typical 0.95v for 1,4GHz ARM and 1,6GHz LPDDR4 */
	pmic_reg_write(dev, BD718XX_BUCK1_VOLT_RUN, 0x19);

	/* Disable unused BUCK2 */
	pmic_reg_write(dev, BD718XX_BUCK2_CTRL, 0x42);

	/* Disable unused LDO6 */
	pmic_reg_write(dev, BD718XX_LDO6_VOLT, 0x83);

	/* lock the PMIC regs */
	pmic_reg_write(dev, BD718XX_REGLOCK, 0x11);

	return 0;
}

int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}

void board_init_f(ulong dummy)
{
	int ret;

	arch_cpu_init();

	init_uart_clk(1);

	timer_init();

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	preloader_console_init();

	enable_tzc380();

	power_init_board();

	/* DDR initialization */
	spl_dram_init();

	board_init_r(NULL, 0);
}
