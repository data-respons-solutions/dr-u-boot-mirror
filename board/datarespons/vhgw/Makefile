#
# Copyright 2018 NXP
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += imx8mn_evk.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
ifdef CONFIG_IMX8MN_LOW_DRIVE_MODE
obj-$(CONFIG_IMX8M_LPDDR4) += lpddr4_timing_v1.2.o
obj-$(CONFIG_IMX8M_DDR3L) += ddr3l_timing.o
else
obj-$(CONFIG_IMX8M_LPDDR4) += lpddr4_timing_v1.2.o
obj-$(CONFIG_IMX8M_DDR3L) += ddr3l_timing.o
endif
endif
