/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/sys_log.h>
#include <uwp360_hal.h>
#include "hal_sipc.h"
#include "hal_sipc_priv.h"

#define SMSG_IRQ_TXBUF_ADDR         (0)
#define SMSG_IRQ_TXBUF_SIZE		    (0x100)
#define SMSG_IRQ_RXBUF_ADDR		    (SMSG_IRQ_TXBUF_ADDR + SMSG_IRQ_TXBUF_SIZE)
#define SMSG_IRQ_RXBUF_SIZE		    (0x100)

#define SMSG_PRIO_TXBUF_ADDR         (SMSG_IRQ_RXBUF_ADDR + SMSG_IRQ_RXBUF_SIZE)
#define SMSG_PRIO_TXBUF_SIZE		    (0x200)
#define SMSG_PRIO_RXBUF_ADDR		    (SMSG_PRIO_TXBUF_ADDR + SMSG_PRIO_TXBUF_SIZE)
#define SMSG_PRIO_RXBUF_SIZE		    (0x200)

#define SMSG_TXBUF_ADDR		(SMSG_PRIO_RXBUF_ADDR + SMSG_PRIO_RXBUF_SIZE)
#define SMSG_TXBUF_SIZE		(SZ_1K)
#define SMSG_RXBUF_ADDR		(SMSG_TXBUF_ADDR + SMSG_TXBUF_SIZE)
#define SMSG_RXBUF_SIZE		(SZ_1K)

#define SMSG_RINGHDR		(SMSG_TXBUF_ADDR + SMSG_TXBUF_SIZE + SMSG_RXBUF_SIZE)

#define SMSG_IRQ_TXBUF_RDPTR	(SMSG_RINGHDR + 0)
#define SMSG_IRQ_TXBUF_WRPTR	(SMSG_RINGHDR + 4)
#define SMSG_IRQ_RXBUF_RDPTR	(SMSG_RINGHDR + 8)
#define SMSG_IRQ_RXBUF_WRPTR	(SMSG_RINGHDR + 12)

#define SMSG_PRIO_TXBUF_RDPTR	(SMSG_RINGHDR + 16)
#define SMSG_PRIO_TXBUF_WRPTR	(SMSG_RINGHDR + 20)
#define SMSG_PRIO_RXBUF_RDPTR	(SMSG_RINGHDR + 24)
#define SMSG_PRIO_RXBUF_WRPTR	(SMSG_RINGHDR + 28)

#define SMSG_TXBUF_RDPTR	(SMSG_RINGHDR + 32)
#define SMSG_TXBUF_WRPTR	(SMSG_RINGHDR + 36)
#define SMSG_RXBUF_RDPTR	(SMSG_RINGHDR + 40)
#define SMSG_RXBUF_WRPTR	(SMSG_RINGHDR + 44)

static int sipc_create(struct smsg_ipc  *sipc)
{
    uintptr_t base;
    int ret;
    SYS_LOG_INF("0x%x %x",sipc,IPC_RING_ADDR);
/*irq dispatch deq init*/
    base = IPC_RING_ADDR;
    sipc->deq_irq_dis.txbuf_size = SMSG_IRQ_TXBUF_SIZE / sizeof(struct smsg);
    sipc->deq_irq_dis.txbuf_addr = base + SMSG_IRQ_TXBUF_ADDR;
    sipc->deq_irq_dis.txbuf_rdptr = base + SMSG_IRQ_TXBUF_RDPTR;
    sipc->deq_irq_dis.txbuf_wrptr = base + SMSG_IRQ_TXBUF_WRPTR;

    sipc->deq_irq_dis.rxbuf_size = SMSG_IRQ_RXBUF_SIZE / sizeof(struct smsg);
    sipc->deq_irq_dis.rxbuf_addr = base + SMSG_IRQ_RXBUF_ADDR;
    sipc->deq_irq_dis.rxbuf_rdptr = base + SMSG_IRQ_RXBUF_RDPTR;
    sipc->deq_irq_dis.rxbuf_wrptr = base + SMSG_IRQ_RXBUF_WRPTR;
/*prio msg deq init*/
    sipc->deq_priority_msg.txbuf_size = SMSG_PRIO_TXBUF_SIZE / sizeof(struct smsg);
    sipc->deq_priority_msg.txbuf_addr = base + SMSG_PRIO_TXBUF_ADDR;
    sipc->deq_priority_msg.txbuf_rdptr = base + SMSG_PRIO_TXBUF_RDPTR;
    sipc->deq_priority_msg.txbuf_wrptr = base + SMSG_PRIO_TXBUF_WRPTR;

    sipc->deq_priority_msg.rxbuf_size = SMSG_PRIO_RXBUF_SIZE / sizeof(struct smsg);
    sipc->deq_priority_msg.rxbuf_addr = base + SMSG_PRIO_RXBUF_ADDR;
    sipc->deq_priority_msg.rxbuf_rdptr = base + SMSG_PRIO_RXBUF_RDPTR;
    sipc->deq_priority_msg.rxbuf_wrptr = base + SMSG_PRIO_RXBUF_WRPTR;

/*normal msg deq init*/
    sipc->deq_normal_msg.txbuf_size = SMSG_TXBUF_SIZE / sizeof(struct smsg);
    sipc->deq_normal_msg.txbuf_addr = base + SMSG_TXBUF_ADDR;
    sipc->deq_normal_msg.txbuf_rdptr = base + SMSG_TXBUF_RDPTR;
    sipc->deq_normal_msg.txbuf_wrptr = base + SMSG_TXBUF_WRPTR;

    sipc->deq_normal_msg.rxbuf_size = SMSG_RXBUF_SIZE / sizeof(struct smsg);
    sipc->deq_normal_msg.rxbuf_addr = base + SMSG_RXBUF_ADDR;
    sipc->deq_normal_msg.rxbuf_rdptr = base + SMSG_RXBUF_RDPTR;
    sipc->deq_normal_msg.rxbuf_wrptr = base + SMSG_RXBUF_WRPTR;

    ret = smsg_ipc_create(sipc->dst, sipc);

    return ret;
}
int sipc_probe(void)
{
	struct smsg_ipc *sipc = NULL;

    /*将IPC共享内存的地址直接转换为ipc的结构体进行管理*/
    memset((void*)IPC_RING_ADDR,0,(SMSG_RXBUF_WRPTR+4 - SMSG_IRQ_TXBUF_ADDR));
	//sipc = (struct smsg_ipc *)IPC_RING_ADDR;
    sipc = (struct smsg_ipc *)kmm_malloc(sizeof(struct smsg_ipc));
    memset((void*)sipc,0,sizeof(struct smsg_ipc));
    if (sipc == NULL) {
        ipc_warn("warning!! 0x%x %d",sipc,sizeof(struct smsg_ipc));
    }

    sipc->name = IPC_NAME;
    sipc->dst  = IPC_DST;
//    sipc->irq  = IPC_IRQ_NUM;
//    sipc->rxirq_status = get_ipi_handler_finish_flag;
//    sipc->rxirq_clear  = ipi_phy_clr_int_from;
//    sipc->txirq_trigger = ipi_phy_trig_int;

    sipc_create(sipc);

	return 0;
}
