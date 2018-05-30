/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IPI_UWP360_H_
#define _IPI_UWP360_H_

typedef void (*uwp360_ipi_callback_t) (void *data);
extern void uwp360_ipi_set_callback(uwp360_ipi_callback_t cb, void *arg);
extern void uwp360_ipi_unset_callback(void);

static inline void uwp360_ipi_irq_trigger(void) {
	uwp360_hal_ipi_trigger(IPI_CORE_BTWF, IPI_TYPE_IRQ0);
}

#endif /* _INTC_UWP360_H_ */
