/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HAL_SYS_H
#define __HAL_SYS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uwp360_hal.h"

#define CTL_APB_BASE			BASE_GLB

#define REG_APB_RST				(CTL_APB_BASE + 0x0)
#define REG_MCU_SOFT_RST		(CTL_APB_BASE + 0x4)
#define REG_PWR_ON_RSTN_INDEX	(CTL_APB_BASE + 0x8)
#define REG_SYS_SOFT_RST		(CTL_APB_BASE + 0xC)

#define REG_APB_EB				(CTL_APB_BASE + 0x18)
#define REG_APB_EB1				(CTL_APB_BASE + 0x1C)
#define REG_WDG_INT_EN			(CTL_APB_BASE + 0x20)
#define REG_TRIM_LDO_DIGCORE	(CTL_APB_BASE + 0x24)
#define REG_WFRX_MODE_SEL		(CTL_APB_BASE + 0x28)
#define REG_FM_RF_CTRL			(CTL_APB_BASE + 0x2C)
#define REG_APB_SLP_CTL0		(CTL_APB_BASE + 0x30)
#define REG_APB_INT_STS0		(CTL_APB_BASE + 0x34)
#define REG_WIFI_SYS0			(CTL_APB_BASE + 0x70)
#define REG_WIFI_SYS1			(CTL_APB_BASE + 0x74)

#define APB_MCU_SOFT_RST	0
#define APB_PWR_ON_RST		0
#define APB_SYS_SOFT_RST	0

	enum {
		APB_EB_TMR3_RTC = 0,
		APB_EB_TMR4_RTC,
		APB_EB_UART0,
		APB_EB_UART1,
		APB_EB_SYST,
		APB_EB_TMR0,
		APB_EB_TMR1,
		APB_EB_WDG,
		APB_EB_DJTAG,
		APB_EB_WDG_RTC,
		APB_EB_SYST_RTC,
		APB_EB_TMR0_RTC,
		APB_EB_TMR1_RTC,
		APB_EB_BT_RTC,
		APB_EB_EIC,
		APB_EB_EIC_RTC,
		
		APB_EB_IIS = 16,
		APB_EB_INTC,
		APB_EB_PIN,
		APB_EB_FM,
		APB_EB_EFUSE,
		APB_EB_TMR2,
		APB_EB_TMR3,
		APB_EB_TMR4,
		APB_EB_GPIO,
		APB_EB_IPI,
		APB_EB_COM_TMR_ = 30,
		APB_EB_WCI2,
	};

	struct uwp360_sys {
		u32_t rst;
		u32_t mcu_soft_rst;
		u32_t rstn_index;
		u32_t sys_soft_rst;
		u32_t eb;
		u32_t eb1;
	};

	static inline void uwp360_sys_enable(u32_t bits) {
		sci_glb_set(REG_APB_EB, bits);
	}

	static inline void uwp360_sys_disable(u32_t bits) {
		sci_glb_clr(REG_APB_EB, bits);
	}

	static inline void uwp360_sys_reset(u32_t bits) {
		u32_t wait = 50;

		sci_glb_set(REG_APB_RST, bits);
		while(wait--){}
		sci_glb_clr(REG_APB_RST, bits);
	}

#ifdef __cplusplus
}
#endif

#endif
