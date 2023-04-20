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
#include <mtd.h>
#include <image.h>
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

struct mtd_info* get_mtd_by_partname(const char* partname)
{
	struct mtd_info* mtd = NULL;
	mtd_for_each_device(mtd) {
		if (mtd_is_partition(mtd) && (strcmp(mtd->name, partname) == 0))
			return mtd;
	}
	return NULL;
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
static ulong spl_mtd_fit_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf)
{
	struct mtd_info *mtd = load->dev;
	size_t retlen = 0;
	ulong r = 0;
	r = mtd_read(mtd, sector, count, &retlen, buf);
	if (r != 0)
		return 0;
	return (ulong) retlen;
}
static int spl_mtd_load_image(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev)
{
	struct mtd_info *mtd = NULL;
	struct image_header *header = NULL;
	size_t retlen = 0;
	int r = 0;
	char* partnames[] = {"u-boot", "u-boot-second"};

	volatile const struct src *src = (volatile const struct src*) SRC_BASE_ADDR;
	if ((src->gpr10 & SRC_GPR10_PERSIST_SECONDARY_BOOT) == SRC_GPR10_PERSIST_SECONDARY_BOOT) {
		/* Secondary boot, swap partition search order */
		char* tmp = partnames[0];
		partnames[0] = partnames[1];
		partnames[1] = tmp;
	}

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

	for (size_t i = 0; i < ARRAY_SIZE(partnames); ++i) {
		printf("MTD load: %s\n", partnames [i]);
		mtd = get_mtd_by_partname(partnames [i]);
		if (mtd == NULL) {
			printf("MTD probe failed\n");
			r = -ENODEV;
			continue;
		}

		/* Load u-boot, mkimage header is 64 bytes. */
		r = mtd_read(mtd, 0, sizeof(*header), &retlen, (void*) header);
		if (r != 0 || retlen != sizeof(*header)) {
			if (retlen != sizeof(*header))
				r = -EIO;
			printf("MTD Failed reading from device: %d\n", r);
			continue;
		}
		if (image_get_magic(header) != FDT_MAGIC) {
			printf("%s: Image not of type FDT\n", __func__);
			r = -EINVAL;
			continue;
		}
		struct spl_load_info load;
		load.dev = mtd;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spl_mtd_fit_read;
		r = spl_load_simple_fit(spl_image, &load, 0, header);
		if (r == 0)
			break;
	}

	return r;
}
SPL_LOAD_IMAGE_METHOD("MTD", 0, BOOT_DEVICE_SPI, spl_mtd_load_image);

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
	int r = 0;
	size_t retlen = 0;
	struct mtd_info* platform = get_mtd_by_partname("platform");
	if (platform == NULL)
		return -ENODEV;

	uint8_t* header_buf = malloc(PLATFORM_HEADER_SIZE);
	if (header_buf == NULL)
		return -ENOMEM;

	r = mtd_read(platform, 0, PLATFORM_HEADER_SIZE, &retlen, header_buf);
	if (r != 0 || retlen != PLATFORM_HEADER_SIZE)
		goto exit;

	r = parse_header(platform_header, header_buf, PLATFORM_HEADER_SIZE);
	if (r != 0) {
		printf("platform_header corrupt: %d\n", r);
		goto exit;
	}

	uint8_t* dram_buf = malloc(platform_header->ddrc_blob_size);
	if (dram_buf == NULL) {
		r = -ENOMEM;
		goto exit;
	}

	r = mtd_read(platform, platform_header->ddrc_blob_offset,
					platform_header->ddrc_blob_size, &retlen, dram_buf);
	if (r != 0 || retlen != platform_header->ddrc_blob_size)
		goto exit;

	const uint32_t crc32_init = crc32(0L, Z_NULL, 0);
	const uint32_t crc32_calc = crc32(crc32_init, dram_buf, platform_header->ddrc_blob_size);
	if (platform_header->ddrc_blob_crc32 != crc32_calc) {
		printf("dram_timing_info corrupt: crc32 mismatch\n");
		r = -EINVAL;
		goto exit;
	}

	r = parse_dram_timing_info(dram_timing_info, dram_buf, platform_header->ddrc_blob_size);
	if (r != 0) {
		printf("dram_timing_info corrupt: %d\n", r);
		goto exit;
	}

	r = 0;
exit:
	if (header_buf != NULL)
		free(header_buf);
	if (dram_buf != NULL)
		free(dram_buf);
	return r;
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

	/* Ensure all devices (and their partitions) are probed */
	mtd_probe_devices();

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
