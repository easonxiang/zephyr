/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HAL_SIPC_PRIV_H
#define __HAL_SIPC_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif

#define SMSG_CACHE_NR		32
#define CONFIG_SMSG_THREAD_DEFPRIO 50
#define CONFIG_SMSG_THREAD_STACKSIZE 1024

struct smsg_channel {
	/* wait queue for recv-buffer */
	//wait_queue_head_t	rxwait;
     struct k_sem	    *rxsemwait;
	 struct k_mutex		rxlock;

	/* cached msgs for recv */
	uintptr_t 		wrptr[1];
	uintptr_t 		rdptr[1];
	struct smsg		caches[SMSG_CACHE_NR];
};

struct smsg_queue {
	/* send-buffer info */
	uintptr_t			txbuf_addr;
	u32_t			txbuf_size;	/* must be 2^n */
	uintptr_t			txbuf_rdptr;
	uintptr_t			txbuf_wrptr;

	/* recv-buffer info */
	uintptr_t			rxbuf_addr;
	u32_t			rxbuf_size;	/* must be 2^n */
	uintptr_t			rxbuf_rdptr;
	uintptr_t			rxbuf_wrptr;
};
/* smsg ring-buffer between AP/CP ipc */
struct smsg_ipc {
	char			*name;
	uint8_t			dst;
	uint8_t			padding[3];

    struct smsg_queue   deq_irq_dis;
    struct smsg_queue   deq_priority_msg;
    struct smsg_queue   deq_normal_msg;

#ifdef 	CONFIG_SPRD_MAILBOX
	/* target core_id over mailbox */
	int 			core_id;
#endif

	/* sipc irq related */
	int			irq;
	//xcpt_t		irq_handler;
	//xcpt_t		irq_threadfn;

	u32_t 		(*rxirq_status)(void);
	void			(*rxirq_clear)(void);
	void			(*txirq_trigger)(void);

	/* sipc ctrl thread */
	struct tcb_s	*thread;
    u32_t			pid;
    struct k_sem    *dis_semwait;
	/* lock for send-buffer */
	//spinlock_t		txpinlock;
    u32_t            txpinlock;
	/* all fixed channels receivers */
	struct smsg_channel	*channels[SMSG_CH_NR];

	/* record the runtime status of smsg channel */
	u32_t 		busy[SMSG_CH_NR];

	/* all channel states: 0 unused, 1 opened */
	uint8_t			states[SMSG_CH_NR];
};

#define CHAN_STATE_UNUSED	0
#define CHAN_STATE_WAITING 	1
#define CHAN_STATE_OPENED	2
#define CHAN_STATE_FREE 	3

/* create/destroy smsg ipc between AP/CP */
int smsg_ipc_create(uint8_t dst, struct smsg_ipc *ipc);
int smsg_ipc_destroy(uint8_t dst);
int  smsg_suspend_init(void);

/* initialize smem pool for AP/CP */
int smem_init(u32_t addr, u32_t size);

#ifdef __cplusplus
}
#endif

#endif
