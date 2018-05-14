/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <soc.h>
#include <errno.h>
#include <assert.h>

#include "intc_uwp360.h"
#include "eic_uwp360.h"
#include "uwp360_hal.h"

/* convenience defines */
#define DEV_DATA(dev) \
	((struct eic_uwp360_data * const)(dev)->driver_data)

static struct device DEVICE_NAME_GET(eic_uwp360);

struct __intc_cb {
	uwp360_eic_callback_t cb;
	void *data;
};

struct eic_uwp360_data {
	struct __intc_cb eic[EIC_MAX_CHANNEL];
};

static struct eic_uwp360_data eic_uwp360_dev_data;

void uwp360_eic_set_callback(int channel,
		uwp360_eic_callback_t cb, void *arg)
{
	struct device *dev = DEVICE_GET(eic_uwp360);
	struct eic_uwp360_data *data = DEV_DATA(dev);

	if(data->eic[channel].cb != NULL) {
		printk("EIC %d callback has been registered.\n", channel);
		return;
	}

	data->eic[channel].cb = cb;
	data->eic[channel].data = arg;

}

void uwp360_eic_unset_callback(int channel)
{
	struct device *dev = DEVICE_GET(eic_uwp360);
	struct eic_uwp360_data *data = DEV_DATA(dev);

	data->eic[channel].cb = NULL;
	data->eic[channel].data = NULL;
}

void uwp360_eic_enable(int channel)
{
	int eic = channel > 3;
	int ch = channel & 0x7;

	uwp360_hal_eic_enable(eic, ch);
	uwp360_hal_eic_disable_sleep(eic, ch);
}

void uwp360_eic_set_trigger(int channel, int type)
{
	int eic = channel > 3;
	int ch = channel & 0x7;

	uwp360_hal_eic_set_trigger(eic, ch, type);
}

void uwp360_eic_disable(int channel)
{
	int eic = channel > 3;
	int ch = channel & 0x7;

	uwp360_hal_eic_disable(eic, ch);
}

static void eic_uwp360_isr(int channel, void *arg)
{
	int eic;
	int ch;
	int isr_num;
	struct device *dev = (struct device *)arg;
	struct eic_uwp360_data *data = DEV_DATA(dev);

	for(eic = 0; eic < EIC_MAX_NUM; eic++) {
		for(ch = 0; ch < EIC0_MAX_CHANNEL; ch++) {
			isr_num = eic * EIC0_MAX_CHANNEL + ch;
			if(uwp360_hal_eic_status(eic, ch) && data->eic[isr_num].cb) {
				uwp360_hal_eic_disable(eic, ch);
				uwp360_hal_eic_clear(eic, ch);

				data->eic[isr_num].cb(ch, data->eic[isr_num].data);

				uwp360_hal_eic_enable(eic, ch);
			}
		}
	}
}

static int eic_uwp360_init(struct device *dev)
{
	/* enable EB 26,27 is according to RTL code */
	//uwp360_sys_enable(BIT(APB_EB_EIC) | BIT(APB_EB_EIC_RTC));
	//uwp360_sys_reset(BIT(APB_EB_EIC) | BIT(APB_EB_EIC_RTC));
	uwp360_sys_enable(BIT(26) | BIT(27));
	uwp360_sys_reset(BIT(23) | BIT(24));

	uwp360_aon_enable(BIT(AON_EB_EIC0));
	uwp360_aon_reset(BIT(AON_RST_EIC0));

	uwp360_intc_set_irq_callback(INT_EIC, eic_uwp360_isr, dev);
	uwp360_intc_set_fiq_callback(INT_EIC, eic_uwp360_isr, dev);
	uwp360_aon_intc_set_irq_callback(INT_EIC, eic_uwp360_isr, dev);

	uwp360_irq_enable(INT_EIC);
	uwp360_fiq_enable(INT_EIC);
	uwp360_aon_irq_enable(INT_EIC);

	return 0;
}

DEVICE_INIT(eic_uwp360, CONFIG_EIC_UWP360_DEVICE_NAME,
		    eic_uwp360_init, &eic_uwp360_dev_data, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
