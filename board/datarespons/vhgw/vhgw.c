// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2022, Data Respons Solutions AB
 *
 */

#include <common.h>
#include <stdlib.h>
#include <env.h>
#include <init.h>
#include <asm/global_data.h>
#include <power/regulator.h>
#include <bloblist.h>
#include <fdt_support.h>
#include <mapmem.h>
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

/*
 * OPTEE will place dt overlay at 0x43200000.
 * Apply the overlay before booting linux.
 */
int ft_board_setup(void *blob, struct bd_info *bd)
{
	(void) bd;
	struct fdt_header* overlay = map_sysmem(0x43200000, 0);
	int r = 0;

	if (!fdt_valid(&overlay)) {
		printf("WARNING: optee overlay not found\n");
		return -ENOENT;
	}

	r = fdt_shrink_to_minimum((struct fdt_header*) blob, fdt_totalsize(overlay));
	if (r < 8096) {
		printf("ERROR: optee fdt resize failed: %d\n", r);
		return -ENOMEM;
	}

	/*
	 * overlay is damaged by apply operation, use a copy.
	 */
	struct fdt_header* overlay_copy = malloc(fdt_totalsize(overlay));
	if (overlay_copy == NULL) {
		printf("ERROR: optee fdt overlay copy failed\n");
		return -ENOMEM;
	}
	memcpy(overlay_copy, overlay, fdt_totalsize(overlay));
	r = fdt_overlay_apply_verbose((struct fdt_header *) blob, overlay_copy);
	free(overlay_copy);
	return r;
}
