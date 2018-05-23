/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include "pinmux.h"
#include "uwp360_hal.h"

int pinmux_initialize(struct device *port)
{
	ARG_UNUSED(port);

	//uwp360_aon_glb_init_clock();
	//sys_write32(0x4011, REG_PIN_ESMD3);
	//sys_write32(0x411a, REG_PIN_ESMD2);

	return 0;
}

SYS_INIT(pinmux_initialize, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
