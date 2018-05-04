/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>

#include <gpio.h>
#include <watchdog.h>
#include <device.h>
#include <console.h>
#include <flash.h>
#include <shell/shell.h>
#include <string.h>
#include <misc/util.h>

#define RCC_BASE	0x40023800
#define GPIOF_BASE	0x40021400

#define RCC_AHB1ENR	RCC_BASE + 0x30

#define GPIO_PORT0		"GPIO_P0"
#define MARLIN_WDG		CONFIG_WDT_UWP360_DEVICE_NAME
struct device *porte;
struct device *portf;
#define PORTF		"GPIOF"
#define BEEP		8
#define LED1		9
#define LED2		10
#define ON			0
#define OFF			1

#define PORTE		"GPIOE"
#define BTN1		2
#define BTN2		3
#define BTN3		4

#define GPIO0		0
#define GPIO2		2

void led_beep_init(struct device *dev)
{
	gpio_pin_configure(dev, LED1, GPIO_DIR_OUT
				| GPIO_PUD_PULL_UP);
	gpio_pin_configure(dev, LED2, GPIO_DIR_OUT
				| GPIO_PUD_PULL_UP);
	gpio_pin_configure(dev, BEEP, GPIO_DIR_OUT
				| GPIO_PUD_PULL_UP);
	gpio_pin_write(dev, LED1, OFF);
	gpio_pin_write(dev, LED2, OFF);
	/*
	gpio_pin_write(dev, BEEP, 1);
	k_sleep(100);
	gpio_pin_write(dev, BEEP, 0);
	*/
}

void beep(struct device *dev)
{
	static int sw = OFF;

	gpio_pin_write(dev, BEEP, sw);
	sw = (sw == ON) ? OFF : ON;
}

void led1_switch(struct device *dev)
{
	static int sw = ON;

	sw = (sw == ON) ? OFF : ON;
	//gpio_pin_write(dev, LED1, sw);
	gpio_pin_write(dev, GPIO2, sw);
}

void led2_switch(struct device *dev)
{
	static int sw = ON;

	gpio_pin_write(dev, LED2, sw);
	sw = (sw == ON) ? OFF : ON;
}
struct gpio_callback cb;
static int cb_cnt;
static int system_init = 0;

static void callback_1(struct device *dev,
		       struct gpio_callback *gpio_cb, u32_t pins)
{
	if(system_init){
	printk("%s 0x%x triggered: %d\n", __func__, pins, ++cb_cnt);
	if(pins & BIT(BTN1)) led1_switch(portf);
	if(pins & BIT(BTN2)) led2_switch(portf);
	//if(pins & BIT(BTN3)) beep(portf);
	}
}

static void callback_2(struct device *dev,
		       struct gpio_callback *gpio_cb, u32_t pins)
{
	printk("gpio int.\n");
}

void btn_init(struct device *dev)
{
	gpio_pin_disable_callback(dev, BTN1);
	gpio_pin_disable_callback(dev, BTN2);
	gpio_pin_disable_callback(dev, BTN3);

//	gpio_pin_configure(dev, BTN1, GPIO_DIR_IN
//				| GPIO_PUD_PULL_UP);
	gpio_pin_configure(dev, BTN1,
			   GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE | \
			   GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE);
	gpio_pin_configure(dev, BTN2,
			   GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE | \
			   GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE);
	gpio_pin_configure(dev, BTN3,
			   GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE | \
			   GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE);

	gpio_init_callback(&cb, callback_1, BIT(BTN1)|BIT(BTN2)|BIT(BTN3));
	gpio_add_callback(dev, &cb);

	gpio_pin_enable_callback(dev, BTN1);
	gpio_pin_enable_callback(dev, BTN2);
	gpio_pin_enable_callback(dev, BTN3);
}

void timer_expiry(struct k_timer *timer)
{
	//struct device *portf;
	static int c = 0;
	//portf = (struct device*)k_timer_user_data_get(timer);
	//led2_switch(portf);
	printk("timer_expiry %d.\n", c++);
}

void timer_stop(struct k_timer *timer)
{
	static int c = 0;
	printk("timer_stop %d.\n", c++);
}

void print_devices(void)
{
	int i;
	int count = 0;

	static struct device *device_list;

	//device_list_get(&device_list, &count);

	printk("device list(%d):\n", count);
	for(i = 0; i < count; i++) {
		printk(" %s\n ", device_list[i].config->name);
	}
}

/* WDT Requires a callback, there is no interrupt enable / disable. */
void wdt_example_cb(struct device *dev)
{
	struct device *gpio;
	u32_t val;

	gpio = device_get_binding(GPIO_PORT0);
	if(gpio == NULL) {
		printk("Can not find device %s.\n", GPIO_PORT0);
		return;
	}

	gpio_pin_read(gpio, GPIO0, &val);

	//printk("gpio0 val: %d.\n", val);

	led1_switch(gpio);

	wdt_reload(dev);
}

void wdg_init(void)
{
	struct wdt_config wr_cfg;
	struct wdt_config cfg;
	struct device *wdt_dev;

	wr_cfg.timeout = 4000;
	wr_cfg.mode = WDT_MODE_INTERRUPT_RESET;
	wr_cfg.interrupt_fn = wdt_example_cb;

	wdt_dev = device_get_binding(MARLIN_WDG);
	if(wdt_dev == NULL) {
		printk("Can not find device %s.\n", MARLIN_WDG);
		return;
	}

	wdt_set_config(wdt_dev, &wr_cfg);
	wdt_enable(wdt_dev);

	wdt_get_config(wdt_dev, &cfg);
}

#if 0
//#define FLASH_TEST_REGION_OFFSET 0xff000
#define FLASH_TEST_REGION_OFFSET 0x0
#define FLASH_SECTOR_SIZE        4096
#define TEST_DATA_BYTE_0         0x55
#define TEST_DATA_BYTE_1         0xaa
#define TEST_DATA_LEN            0x100
void spi_flash_test(void)
{
	struct device *flash_dev;
	u8_t buf[TEST_DATA_LEN];
	u8_t *p = 0x100000;

	printk("\nW25QXXDV SPI flash testing\n");
	printk("==========================\n");
	printk("flash name:%s.\n",CONFIG_FLASH_MARLIN_NAME);

	flash_dev = device_get_binding(CONFIG_FLASH_MARLIN_NAME);

	if (!flash_dev) {
		printk("SPI flash driver was not found!\n");
		return;
	}

	/* Write protection needs to be disabled in w25qxxdv flash before
	 * each write or erase. This is because the flash component turns
	 * on write protection automatically after completion of write and
	 * erase operations.
	 */
#if 1
	printk("\nTest 1: Flash erase\n");
	flash_write_protection_set(flash_dev, false);
	if (flash_erase(flash_dev,
			FLASH_TEST_REGION_OFFSET,
			FLASH_SECTOR_SIZE) != 0) {
		printk("   Flash erase failed!\n");
	} else {
		printk("   Flash erase succeeded!\n");
	}

	printk("\nTest 2: Flash write\n");
	flash_write_protection_set(flash_dev, false);

	buf[0] = TEST_DATA_BYTE_0;
	buf[1] = TEST_DATA_BYTE_1;
	if (flash_write(flash_dev, FLASH_TEST_REGION_OFFSET, p,
	    TEST_DATA_LEN) != 0) {
		printk("   Flash write failed!\n");
		return;
	}
#endif

	if (flash_read(flash_dev, FLASH_TEST_REGION_OFFSET, buf,
	    TEST_DATA_LEN) != 0) {
		printk("   Flash read failed!\n");
		return;
	}

	if(memcmp(p, buf, TEST_DATA_LEN) == 0) {
		printk("   Data read matches with data written. Good!!\n");
	} else {
		printk("   Data read does not match with data written!!\n");
	}

}

#endif

void uart_test(void)
{
	int i = 100;
	while(i--) {
		printk("%iwfsadklsdffffffasdffffffwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwweeeertterd\n\n", i);
	}
}

static int test_cmd(int argc, char *argv[])
{
	struct device *gpio;
	u32_t val;

	if(argc < 2) return -EINVAL;

	if(!strcmp(argv[1], "gpio")) {

		gpio = device_get_binding(GPIO_PORT0);
		if(gpio == NULL) {
			printk("Can not find device %s.\n", GPIO_PORT0);
			return -EINVAL;
		}

		gpio_pin_read(gpio, GPIO0, &val);
		printk("gpio 0 val: %d.\n", val);

		return 0;
	}else if(!strcmp(argv[1], "flash")) {
		//spi_flash_test();
	}else if(!strcmp(argv[1], "uart")) {
		uart_test();
	}else {
		return -EINVAL;
	}

	return 0;
}

static int info_cmd(int argc, char *argv[])
{
	if(argc != 1) return -EINVAL;

	printk("System Info: \n\n");

	print_devices();

	return 0;
}

static const struct shell_cmd zephyr_cmds[] = {
	{ "info", info_cmd, "" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ "test", test_cmd, "<gpio|flash|uart>" },
	{ NULL, NULL }
};

void gpio_init(void)
{
	struct device *gpio;

	gpio = device_get_binding(GPIO_PORT0);
	if(gpio == NULL) {
		printk("Can not find device %s.\n", GPIO_PORT0);
		return;
	}

	gpio_pin_disable_callback(gpio, GPIO0);

	gpio_pin_configure(gpio, GPIO0,
			   GPIO_DIR_IN | GPIO_INT | GPIO_INT_EDGE | \
			   GPIO_INT_ACTIVE_HIGH | GPIO_INT_DEBOUNCE);

	gpio_pin_configure(gpio, GPIO2, GPIO_DIR_OUT
				| GPIO_PUD_PULL_DOWN);

	gpio_init_callback(&cb, callback_2, BIT(GPIO0));
	gpio_add_callback(gpio, &cb);

	gpio_pin_enable_callback(gpio, GPIO0);
}

void main(void)
{
	struct k_timer timer;

	wdg_init();

	gpio_init();

	SHELL_REGISTER("zephyr", zephyr_cmds);

	shell_register_default_module("zephyr");

	while(1){}

	k_timer_init(&timer, timer_expiry, timer_stop);
	k_timer_user_data_set(&timer, (void *)portf);
	printk("start timer..\n");
	k_timer_start(&timer, K_SECONDS(1), K_SECONDS(5));

	while(1) {
	}
}
