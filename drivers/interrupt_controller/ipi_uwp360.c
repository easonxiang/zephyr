/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <soc.h>
#include <errno.h>
#include <assert.h>

#include "uwp360_hal.h"

static struct device DEVICE_NAME_GET(ipi_uwp360);

struct ipi_uwp360_data {
};

static struct ipi_uwp360_data ipi_uwp360_dev_data;

static void ipi_uwp360_irq(void *arg)
{
	printk("ipi irq.\n");
}

static int ipi_uwp360_init(struct device *dev)
{
	uwp360_sys_enable(BIT(APB_EB_IPI));
	uwp360_sys_reset(BIT(APB_EB_IPI));

	IRQ_CONNECT(NVIC_INT_GNSS2BTWF_IPI, 5,
				ipi_uwp360_irq,
				DEVICE_GET(ipi_uwp360), 0);
	irq_enable(NVIC_INT_GNSS2BTWF_IPI);

	return 0;
}

DEVICE_INIT(ipi_uwp360, CONFIG_IPI_UWP360_DEVICE_NAME,
		    ipi_uwp360_init, &ipi_uwp360_dev_data, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
