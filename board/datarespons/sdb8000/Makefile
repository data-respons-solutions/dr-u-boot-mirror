#
# Copyright 2021 Data Respons Solutions AB
#
# SPDX-License-Identifier:      GPL-2.0+
#

ifdef CONFIG_SPL_BUILD
obj-y += spl.o lpddr4_timing.o dram_timing_spi.o
else
obj-y += sdb8000.o
obj-$(CONFIG_ANDROID_SUPPORT) += android_boot.o
endif
