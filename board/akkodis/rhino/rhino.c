// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 Akkodis Edge
 */

#include <init.h>
#include <dm/uclass.h>
#include <asm/global_data.h>
#include <scmi_agent.h>
#include "../dts/upstream/src/arm64/freescale/imx95-power.h"

DECLARE_GLOBAL_DATA_PTR;

static int imx9_scmi_power_domain_enable(u32 domain, bool enable)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_CLK, "protocol@14", &dev);
	if (ret)
		return ret;

	return scmi_pwd_state_set(dev, 0, domain, enable ? 0 : BIT(30));
}

int board_init(void)
{
	struct udevice *dev = NULL;
	int r = 0;

	/* On reboot from linux the usb PD is disabled and needs to be
	 * enabled from here. PD control is not reliably supported
	 * by DT and needs to be explicitly enabled here.  */
	r = imx9_scmi_power_domain_enable(IMX95_PD_HSIO_TOP, true);
	if (r != 0)
		printf("failed enabling power-domain HSIO_TOP [%d]\n", r);

	/* Instantiate usb hub */
	r = uclass_get_device_by_name(UCLASS_MISC, "usb2512bi@2c", &dev);
	if (r < 0)
		printf("Failed enabling USB hub [%d]\n", r);

	return 0;
}

int board_late_init(void)
{
	return 0;
}
