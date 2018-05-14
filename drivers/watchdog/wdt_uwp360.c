/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <watchdog.h>
#include <soc.h>
#include <errno.h>
#include <assert.h>

#include "uwp360_hal.h"

struct wdg_uwp360_data {
	void (*cb)(struct device *dev);
	u32_t timeout;
	u32_t reload;
	u32_t mode;
};

static struct wdg_uwp360_data wdg_uwp360_dev_data = {
	.cb = NULL,
};

#define DEV_DATA(dev) \
	((struct wdg_uwp360_data *)(dev)->driver_data)
static struct device DEVICE_NAME_GET(wdg_uwp360);

static void wdg_uwp360_enable(struct device *dev)
{
	uwp360_wdg_enable();
}

static void wdg_uwp360_disable(struct device *dev)
{
	uwp360_wdg_disable();
}

static void wdg_uwp360_reload(struct device *dev)
{
	struct wdg_uwp360_data * const dev_data = DEV_DATA(dev);

	uwp360_wdg_load(dev_data->timeout);
	uwp360_wdg_load_irq(dev_data->timeout >> 1);
}

static int wdg_uwp360_set_config(struct device *dev,
				 struct wdt_config *config)
{
	struct wdg_uwp360_data * const dev_data = DEV_DATA(dev);
	u32_t mode;

	if (config->mode == WDT_MODE_RESET)
		mode = WDG_MODE_RESET;
	else
		mode = WDG_MODE_MIX;

	dev_data->mode = config->mode;
	dev_data->timeout = config->timeout;
	dev_data->cb = config->interrupt_fn;

	uwp360_wdg_set_mode(mode);
	wdg_uwp360_reload(dev);

	return 0;
}

static void wdg_uwp360_get_config(struct device *dev,
				  struct wdt_config *config)
{
	struct wdg_uwp360_data * const dev_data = DEV_DATA(dev);

	config->timeout = dev_data->timeout;
	config->mode = dev_data->mode;
	config->interrupt_fn = dev_data->cb;
}

static void wdg_uwp360_isr(void *arg)
{
	struct device *dev = arg;
	struct wdg_uwp360_data * const dev_data = DEV_DATA(dev);

	if (dev_data->cb) {
		dev_data->cb(dev);
	}

	/* Clear interrupts here */
	uwp360_wdg_int_clear();
}

static const struct wdt_driver_api wdg_uwp360_api = {
	.enable = wdg_uwp360_enable,
	.disable = wdg_uwp360_disable,
	.get_config = wdg_uwp360_get_config,
	.set_config = wdg_uwp360_set_config,
	.reload = wdg_uwp360_reload,
};

static int wdg_uwp360_init(struct device *dev)
{
	uwp360_sys_enable(BIT(APB_EB_WDG) | BIT(APB_EB_WDG_RTC) | BIT(APB_EB_SYST_RTC));

	uwp360_sys_reset(BIT(APB_EB_WDG));

	IRQ_CONNECT(NVIC_INT_WDG, 5,
			wdg_uwp360_isr,
			DEVICE_GET(wdg_uwp360), 0);
	irq_enable(NVIC_INT_WDG);
	return 0;
}

DEVICE_AND_API_INIT(wdg_uwp360, CONFIG_WDT_UWP360_DEVICE_NAME,
		    wdg_uwp360_init, &wdg_uwp360_dev_data, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &wdg_uwp360_api);

