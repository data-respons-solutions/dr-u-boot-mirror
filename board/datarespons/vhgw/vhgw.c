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

DECLARE_GLOBAL_DATA_PTR;

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
