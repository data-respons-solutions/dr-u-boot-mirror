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

#include <spi_flash.h>
#include <u-boot/zlib.h>
#include <bloblist.h>

#include "../common/platform_header.h"
#include "../common/imx8m_ddrc_parse.h"

DECLARE_GLOBAL_DATA_PTR;

#define CONFIG_DR_NVRAM_PLATFORM_OFFSET 0x3E0000
#define BLOBLIST_DATARESPONS_PLATFORM 0xc001
#define SRC_GPR10_PERSIST_SECONDARY_BOOT BIT(30)

struct platform_header platform_header;
struct dram_timing_info dram_timing_info;

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
 * Primary SPL boots primary u-boot
 * Secondary SPL boots secondary u-boot
 *
 * If boot fails, try the other one (i.e. Primary SPL -> secondary u-boot)
 * This state should be avoided in flashing stage by:
 *  - always erase SPL before u-boot
 *  - always write u-boot before SPL
 */
void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = spl_boot_device();
	spl_boot_list[1] = spl_boot_device();
}
unsigned int spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	volatile struct src *src = (volatile struct src*) SRC_BASE_ADDR;
	static int i = 0;
	const int is_primary = (src->gpr10 & SRC_GPR10_PERSIST_SECONDARY_BOOT) == 0;
	unsigned int addr = 0;

	if (i == 0)
		addr = is_primary ? CONFIG_SYS_SPI_U_BOOT_OFFS : CONFIG_SYS_SPI_U_BOOT2_OFFS;
	else
		addr = is_primary ? CONFIG_SYS_SPI_U_BOOT2_OFFS : CONFIG_SYS_SPI_U_BOOT_OFFS;
	i++;

	if (addr == CONFIG_SYS_SPI_U_BOOT_OFFS)
		printf("Primary boot\n");
	else
		printf("Secondary boot\n");
	return addr;
}

void spl_dram_init(struct dram_timing_info* dram_timing_info)
{
	ddr_init(dram_timing_info);
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

	/* Store platform header in dram */
	void* pheader = bloblist_add(BLOBLIST_DATARESPONS_PLATFORM, sizeof(struct platform_header), 8);
	if (pheader == NULL) {
		printf("platform header blob registration failed\n");
		hang();
	}
	memcpy(pheader, &platform_header, sizeof(struct platform_header));
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

static int read_platform_header(struct platform_header* platform_header, struct dram_timing_info* dram_timing_info)
{
	struct spi_flash *flash = NULL;
	int r = 0;
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
			CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE);
	if (!flash)
		return -ENODEV;

	uint8_t* header_buf = malloc_simple(PLATFORM_HEADER_SIZE);
	if (header_buf == NULL)
		return -ENOMEM;

	r = spi_flash_read(flash, CONFIG_DR_NVRAM_PLATFORM_OFFSET, PLATFORM_HEADER_SIZE, header_buf);
	if (r != 0)
		return r;

	r = parse_header(platform_header, header_buf, PLATFORM_HEADER_SIZE);
	if (r != 0) {
		printf("platform_header corrupt: %d\n", r);
		return r;
	}

	uint8_t* dram_buf = malloc_simple(platform_header->ddrc_blob_size);
	if (dram_buf == NULL)
		return -ENOMEM;

	r = spi_flash_read(flash, CONFIG_DR_NVRAM_PLATFORM_OFFSET + platform_header->ddrc_blob_offset,
							platform_header->ddrc_blob_size, dram_buf);
	if (r != 0)
		return r;

	const uint32_t crc32_init = crc32(0L, Z_NULL, 0);
	const uint32_t crc32_calc = crc32(crc32_init, dram_buf, platform_header->ddrc_blob_size);
	if (platform_header->ddrc_blob_crc32 != crc32_calc) {
		printf("dram_timing_info corrupt: crc32 mismatch\n");
		return -EINVAL;
	}

	r = parse_dram_timing_info(dram_timing_info, dram_buf, platform_header->ddrc_blob_size);
	if (r != 0) {
		printf("dram_timing_info corrupt: %d\n", r);
		return r;
	}

	free(header_buf);
	free(dram_buf);

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

	ret = read_platform_header(&platform_header, &dram_timing_info);
	if (ret != 0) {
		printf("platform header failed: %d\n", ret);
		hang();
	}

	printf("Platform: %s\n", platform_header.name);

	/* DDR initialization */
	spl_dram_init(&dram_timing_info);

	/* init */
	board_init_r(NULL, 0);
}
