/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HAL_BASE_H
#define __HAL_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#define BASE_SFC_CFG			0x400
#define BASE_WDG				0x40010000
#define CTL_GLBREG_BASE			0x40088000

#define CTL_AON_PIN_BASE		0x40840000

#define __REG_SET_ADDR(reg)		(reg + 0x1000)
#define __REG_CLR_ADDR(reg)		(reg + 0x2000)

#define sci_write32(reg, val) do { \
	sys_write32(val, reg); \
}while(0)

#define sci_read32 sys_read32

#define sci_reg_and(reg, val) do { \
	sci_write32(reg, (sci_read32(reg) & val)); \
}while(0)

#define sci_reg_or(reg, val) do { \
	sci_write32(reg, (sci_read32(reg) | val)); \
}while(0)

#define sci_glb_set(reg, val) do { \
	sys_write32(val, __REG_SET_ADDR(reg)); \
}while(0)

#define sci_glb_clr(reg, val) do { \
	sys_write32(val, __REG_CLR_ADDR(reg)); \
}while(0)

#ifdef __cplusplus
}
#endif

#endif
