/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HAL_PIN_H
#define __HAL_PIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uwp360_hal.h"

#define CTL_PIN_BASE	CTL_AON_PIN_BASE

#define REG_PIN_ESMD3	(CTL_PIN_BASE + 0x20)
#define REG_PIN_ESMD2	(CTL_PIN_BASE + 0x24)

#ifdef __cplusplus
}
#endif

#endif
