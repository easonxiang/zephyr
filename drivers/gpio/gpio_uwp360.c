/*
 * Copyright (c) 2016, Spreadtrum Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>

#include <device.h>
#include <gpio.h>
#include <init.h>
#include <kernel.h>
#include <sys_io.h>

#include "uwp360_hal.h"
#include "gpio_utils.h"
#include "../interrupt_controller/intc_uwp360.h"

struct gpio_uwp360_config {
	/* base address of GPIO port */
	u32_t port_base;
	/* GPIO IRQ number */
	u32_t irq_num;
};

struct gpio_uwp360_data {
	/* list of registered callbacks */
	sys_slist_t callbacks;
	/* callback enable pin bitmask */
	u32_t pin_callback_enables;
};
static const struct gpio_uwp360_config gpio_uwp360_p0_config = {
	.port_base = BASE_AON_GPIOP0,
};

static struct device DEVICE_NAME_GET(gpio_uwp360_p0);
static struct gpio_uwp360_data gpio_uwp360_p0_data;

#define DEV_CFG(dev) \
	((const struct gpio_uwp360_config *)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct gpio_uwp360_data *)(dev)->driver_data)

static inline int gpio_uwp360_config(struct device *port,
				   int access_op, u32_t pin, int flags)
{
	const struct gpio_uwp360_config *gpio_config = DEV_CFG(port);
	u32_t base = gpio_config->port_base;
	u32_t int_type;

	if (access_op != GPIO_ACCESS_BY_PIN) {
		return -ENOTSUP;
	}

	if (flags & GPIO_DIR_OUT)
		uwp360_gpio_set_dir(base, BIT(pin), GPIO_DIR_OUT);
	else
		uwp360_gpio_set_dir(base, BIT(pin), GPIO_DIR_IN);

	if (flags & GPIO_INT) {
		if (flags & GPIO_INT_EDGE) {
			int_type = GPIO_TRIGGER_BOTH_EDGE;
		} else { /* GPIO_INT_LEVEL */
			if (flags & GPIO_INT_ACTIVE_HIGH) {
				int_type = GPIO_TRIGGER_LEVEL_HIGH;
			} else {
				int_type = GPIO_TRIGGER_LEVEL_LOW;
			}
		}

		uwp360_gpio_int_set_type(base, BIT(pin), int_type);
		uwp360_gpio_int_clear(base, BIT(pin));
		uwp360_gpio_int_enable(base, BIT(pin));
	}

	uwp360_gpio_enable(base, BIT(pin));

	return 0;
}

static inline int gpio_uwp360_write(struct device *port,
				    int access_op, u32_t pin, u32_t value)
{
	const struct gpio_uwp360_config *gpio_config = DEV_CFG(port);
	u32_t base = gpio_config->port_base;

	if (access_op != GPIO_ACCESS_BY_PIN) {
		return -ENOTSUP;
	}

	uwp360_gpio_write(base, BIT(pin), value);

	return 0;
}

static inline int gpio_uwp360_read(struct device *port,
				   int access_op, u32_t pin, u32_t *value)
{
	const struct gpio_uwp360_config *gpio_config = DEV_CFG(port);
	u32_t base = gpio_config->port_base;
	u32_t status;

	if (access_op != GPIO_ACCESS_BY_PIN) {
		return -ENOTSUP;
	}

	status = uwp360_gpio_read(base, BIT(pin));
	*value = status >> pin;

	return 0;
}

static int gpio_uwp360_manage_callback(struct device *dev,
				    struct gpio_callback *callback, bool set)
{
	struct gpio_uwp360_data *data = DEV_DATA(dev);

	_gpio_manage_callback(&data->callbacks, callback, set);

	return 0;
}

static int gpio_uwp360_enable_callback(struct device *dev,
				    int access_op, u32_t pin)
{
	struct gpio_uwp360_data *data = DEV_DATA(dev);
	const struct gpio_uwp360_config *gpio_config = DEV_CFG(dev);
	u32_t base = gpio_config->port_base;
	
	if (access_op == GPIO_ACCESS_BY_PIN) {
		data->pin_callback_enables |= (1 << pin);
		uwp360_gpio_input_enable(base, BIT(pin));
	} else {
		data->pin_callback_enables = 0xFFFFFFFF;
		uwp360_gpio_input_enable(base, 0xFFFF);
	}

	return 0;
}


static int gpio_uwp360_disable_callback(struct device *dev,
				     int access_op, u32_t pin)
{
	struct gpio_uwp360_data *data = DEV_DATA(dev);
	const struct gpio_uwp360_config *gpio_config = DEV_CFG(dev);
	u32_t base = gpio_config->port_base;

	if (access_op == GPIO_ACCESS_BY_PIN) {
		data->pin_callback_enables &= ~(1 << pin);
		uwp360_gpio_input_disable(base, BIT(pin));
	} else {
		data->pin_callback_enables = 0;
		uwp360_gpio_input_disable(base, 0xFFFF);
	}

	return 0;
}

static void gpio_uwp360_isr(int ch, void *arg)
{
	struct device *dev = arg;
	const struct gpio_uwp360_config *gpio_config = DEV_CFG(dev);
	struct gpio_uwp360_data *data = DEV_DATA(dev);
	u32_t base = gpio_config->port_base;
	u32_t enabled_int, int_status;

	int_status  = uwp360_gpio_int_status(base, 1);

	enabled_int = int_status & data->pin_callback_enables;

	uwp360_gpio_int_disable(base, int_status);
	uwp360_gpio_int_clear(base, int_status);

	_gpio_fire_callbacks(&data->callbacks, (struct device *)dev,
			     enabled_int);

	uwp360_gpio_int_enable(base, int_status);
}

static const struct gpio_driver_api gpio_uwp360_api = {
	.config = gpio_uwp360_config,
	.write = gpio_uwp360_write,
	.read = gpio_uwp360_read,
	.manage_callback = gpio_uwp360_manage_callback,
	.enable_callback = gpio_uwp360_enable_callback,
	.disable_callback = gpio_uwp360_disable_callback,
};

static int gpio_uwp360_p0_init(struct device *dev)
{
	const struct gpio_uwp360_config *gpio_config = DEV_CFG(dev);
	u32_t base = gpio_config->port_base;

	uwp360_aon_enable(BIT(AON_EB_GPIO0));
	uwp360_aon_reset(BIT(AON_RST_GPIO0));
	/* enable all gpio read/write by default */
	uwp360_gpio_enable(base, 0xFFFF);

#if 0
	uwp360_eic_set_callback(EIC_CH_GPIO0, gpio_uwp360_isr, dev);
	uwp360_eic_set_trigger(EIC_CH_GPIO0, EIC_LOW_LEVEL_TRIGGER);
	uwp360_eic_enable(EIC_CH_GPIO0);
#endif
	uwp360_gpio_int_disable(base, 0xFFFF);
	uwp360_gpio_disable(base, 0xFFFF);
	uwp360_aon_intc_set_irq_callback(AON_INT_GPIO0, gpio_uwp360_isr, dev);
	//uwp360_aon_irq_enable(AON_INT_GPIO0);

	return 0;
}

DEVICE_AND_API_INIT(gpio_uwp360_p0, CONFIG_GPIO_UWP360_P0_NAME,
		    &gpio_uwp360_p0_init, &gpio_uwp360_p0_data,
		    &gpio_uwp360_p0_config,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &gpio_uwp360_api);

