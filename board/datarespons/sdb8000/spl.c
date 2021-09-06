// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Data Respons Solutions AB
 */

#include <common.h>
#include <cpu_func.h>
#include <hang.h>
#include <spl.h>
#include <usb.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/arch/ddr.h>

#include <power/pmic.h>
#include <power/bd71837.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc_imx.h>
#include <mmc.h>

DECLARE_GLOBAL_DATA_PTR;

int spl_board_boot_device(enum boot_device boot_dev_spl)
{
	switch (boot_dev_spl) {
	case SPI_NOR_BOOT:
		return BOOT_DEVICE_SPI;
	case USB_BOOT:
		return BOOT_DEVICE_BOARD;
	default:
		return BOOT_DEVICE_NONE;
	}
}

int board_spi_cs_gpio(unsigned bus, unsigned cs)
{
	if (bus == 0 && cs == 0) {
		return IMX_GPIO_NR(5, 9);
	}

	return -1;
}

int power_init_board(void)
{
	struct pmic *p;
	int r;

	r = power_bd71837_init(0);
	if (r) {
		printf("power: init failed: %d\n", r);
		return r;
	}

	p = pmic_get("BD71837");
	r = pmic_probe(p);
	if (r) {
		pr_err("power: probe failed: %d\n", r);
		return r;
	}

	/* decrease RESET key long push time from the default 10s to 10ms */
	r = pmic_reg_write(p, BD71837_PWRONCONFIG1, 0x0);
	if (r) {
		pr_err("power: write failed: %d\n", r);
		return r;
	}

	/* unlock the PMIC regs */
	r = pmic_reg_write(p, BD71837_REGLOCK, 0x1);
	if (r) {
		pr_err("power: write failed: %d\n", r);
		return r;
	}

	/* increase VDD_SOC to typical value 0.85v before first DRAM access */
	r = pmic_reg_write(p, BD71837_BUCK1_VOLT_RUN, 0x0f);
	if (r) {
		pr_err("power: write failed: %d\n", r);
		return r;
	}

	/* increase VDD_DRAM to 0.975v for 3Ghz DDR */
	r = pmic_reg_write(p, BD71837_BUCK5_VOLT, 0x83);
	if (r) {
		pr_err("power: write failed: %d\n", r);
		return r;
	}

	/* lock the PMIC regs */
	r = pmic_reg_write(p, BD71837_REGLOCK, 0x11);
	if (r) {
		pr_err("power: write failed: %d\n", r);
		return r;
	}

	return 0;
}

void spl_board_init(void)
{

}

int board_fit_config_name_match(const char *name)
{
	return 0;
}

/*
 * We have USB_OTG_ID pin tied to ground, force DEVICE.
 */
int board_usb_phy_mode(struct udevice *dev)
{
	return USB_INIT_DEVICE;
}

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MM_PAD_UART4_RXD_UART4_RX | MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_FSEL1),
	IMX8MM_PAD_UART4_TXD_UART4_TX | MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_FSEL1),
};

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MM_PAD_GPIO1_IO02_WDOG1_WDOG_B  | MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE),
};

struct i2c_pads_info i2c_pad_info1 = {
	.scl = {
		.i2c_mode = IMX8MM_PAD_I2C1_SCL_I2C1_SCL | MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_HYS),
		.gpio_mode = IMX8MM_PAD_I2C1_SCL_GPIO5_IO14 | MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_HYS),
		.gp = IMX_GPIO_NR(5, 14),
	},
	.sda = {
		.i2c_mode = IMX8MM_PAD_I2C1_SDA_I2C1_SDA | MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_HYS),
		.gpio_mode = IMX8MM_PAD_I2C1_SDA_GPIO5_IO15 | MUX_PAD_CTRL(PAD_CTL_DSE6 | PAD_CTL_HYS),
		.gp = IMX_GPIO_NR(5, 15),
	},
};

static iomux_v3_cfg_t const spi_nor_pads[] = {
	IMX8MM_PAD_ECSPI1_SCLK_ECSPI1_SCLK	| MUX_PAD_CTRL(0x94),
	IMX8MM_PAD_ECSPI1_MISO_ECSPI1_MISO	| MUX_PAD_CTRL(0x94),
	IMX8MM_PAD_ECSPI1_MOSI_ECSPI1_MOSI	| MUX_PAD_CTRL(0x94),
	IMX8MM_PAD_ECSPI1_SS0_GPIO5_IO9		| MUX_PAD_CTRL(0x41),
};

int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
	imx_iomux_v3_setup_multiple_pads(spi_nor_pads, ARRAY_SIZE(spi_nor_pads));

	init_uart_clk(3);
	init_clk_ecspi(0);

	return 0;
}

void board_init_f(ulong dummy)
{
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	board_early_init_f();

	timer_init();

	preloader_console_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	enable_tzc380();

	/* Adjust pmic voltage to 1.0V for 800M */
	setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info1);

	power_init_board();
	/* DDR initialization */
	ddr_init(&dram_timing);
	printf("Ram size: 0x%lx\n", get_ram_size((void*)CONFIG_SYS_INIT_RAM_ADDR, CONFIG_SYS_INIT_RAM_SIZE));

	board_init_r(NULL, 0);
}

int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	puts ("resetting ...\n");

	reset_cpu(WDOG1_BASE_ADDR);

	return 0;
}
