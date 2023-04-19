// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022, Data Respons Solutions AB
 *
 */

#include <common.h>
#include <env.h>
#include <init.h>
#include <asm/global_data.h>
#include <power/regulator.h>
#include <bloblist.h>
#include <../common/platform_header.h>

DECLARE_GLOBAL_DATA_PTR;

#define BLOBLIST_DATARESPONS_PLATFORM 0xc001

int board_phys_sdram_size(phys_size_t *size)
{
	struct platform_header* platform_header = bloblist_find(BLOBLIST_DATARESPONS_PLATFORM,
												sizeof(struct platform_header));
	if (size == NULL)
		return -EINVAL;
	if (platform_header == NULL) {
		printf("No platform_header -- set ram size to default: 0x%llx\n", (phys_size_t) PHYS_SDRAM_SIZE);
		*size = PHYS_SDRAM_SIZE;
	}
	else {
		*size = platform_header->ddrc_size;
	}
	return 0;
}

int power_init_board(void)
{
	int r = 0;

	const int verbose = 1;
	r = regulators_enable_boot_on(verbose);
	if (r)
		printf("Failed enabling boot regulators: %d\n", r);

	return 0;
}

int board_init(void)
{
	return 0;
}

int board_late_init(void)
{
	return 0;
}
