/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __UWP360_HAL_H
#define __UWP360_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_irq.h"
#include "hal_base.h"
#include "hal_sys.h"
#include "hal_uart.h"
#include "hal_pin.h"
#include "hal_wdg.h"
#include "hal_sfc.h"
#include "hal_sfc_cfg.h"
#include "hal_sfc_phy.h"
#include "hal_sfc_hal.h"
#include "hal_gpio.h"

#define TRUE   (1)
#define FALSE  (0)

#define LOGI printk
#define SCI_ASSERT
#define mdelay

#ifdef __cplusplus
}
#endif

#endif
