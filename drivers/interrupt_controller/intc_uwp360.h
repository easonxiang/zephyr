/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INTC_UWP360_H_
#define _INTC_UWP360_H_

typedef void (*uwp360_intc_callback_t) (int channel, void *user);

extern void uwp360_intc_set_irq_callback(int channel,
		uwp360_intc_callback_t cb, void *arg);
extern void uwp360_intc_unset_irq_callback(int channel);
extern void uwp360_intc_set_fiq_callback(int channel,
		uwp360_intc_callback_t cb, void *arg);
extern void uwp360_intc_unset_fiq_callback(int channel);
extern void uwp360_aon_intc_set_irq_callback(int channel,
		uwp360_intc_callback_t cb, void *arg);
extern void uwp360_aon_intc_unset_irq_callback(int channel);

#endif /* _INTC_UWP360_H_ */
