/*
 * Copyright (c) 2018, UNISOC Incorporated
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HAL_SIPC_H
#define __HAL_SIPC_H

#ifdef __cplusplus
extern "C" {
#endif


/* ****************************************************************** */
/* SMSG interfaces */

/* sipc processor ID definition */
enum {
	SIPC_ID_AP = 0,		/* Application Processor */
	SIPC_ID_WCN,		/* Wireless Connectivity */
	SIPC_ID_NR,		/* total processor number */
};

/* share-mem ring buffer short message */
struct smsg {
	uint8_t			channel;	/* channel index */
	uint8_t			type;		/* msg type */
	uint16_t		flag;		/* msg flag */
	uint32_t		value;		/* msg value */
};

/* smsg channel definition */
enum {
	SMSG_CH_CTRL = 0,	/* some emergency control */
	SMSG_CH_COMM,		/* general communication channel */
	SMSG_CH_BT,
	SMSG_CH_WIFI_CTRL=12,
	SMSG_CH_WIFI_DATA_NOR,
	SMSG_CH_WIFI_DATA_SPEC,
	SMSG_CH_TTY,		/* virtual serial for telephony */
	SMSG_CH_PBUF_EVENT,
	SMSG_CH_PBUF_DATA,
	SMSG_CH_IRQ_DIS,
	SMSG_CH_NR,		/* total channel number */
};

#define WIFI_CTRL_MSG_OFFSET		0
#define WIFI_DATA_NOR_MSG_OFFSET	9

/* smsg type definition */
enum {
	SMSG_TYPE_NONE = 0,
	SMSG_TYPE_OPEN,		/* first msg to open a channel */
	SMSG_TYPE_CLOSE,	/* last msg to close a channel */
	SMSG_TYPE_DATA,		/* data, value=addr, no ack */
	SMSG_TYPE_EVENT,	/* event with value, no ack */
	SMSG_TYPE_CMD,		/* command, value=cmd */
	SMSG_TYPE_DONE,		/* return of command */
	SMSG_TYPE_SMEM_ALLOC,	/* allocate smem, flag=order */
	SMSG_TYPE_SMEM_FREE,	/* free smem, flag=order, value=addr */
	SMSG_TYPE_SMEM_DONE,	/* return of alloc/free smem */
	SMSG_TYPE_FUNC_CALL,	/* RPC func, value=addr */
	SMSG_TYPE_FUNC_RETURN,	/* return of RPC func */
	SMSG_TYPE_DIE,
	SMSG_TYPE_DFS,
	SMSG_TYPE_DFS_RSP,
	SMSG_TYPE_ASS_TRG,
	SMSG_TYPE_NR,		/* total type number */
};

/* flag for OPEN/CLOSE msg type */
#define	SMSG_OPEN_MAGIC		0xBEEE
#define	SMSG_CLOSE_MAGIC	0xEDDD

#ifndef SZ_1K
#define SZ_1K                               0x400
#endif

#define IPC_RING_ADDR       0x0017E000
#define IPC_RING_SMSG_NUM   50
#define IPC_IRQ_NUM         12
#define IPC_IRQ_DISPATCH_FRQ       TB_GNSS2BTWF_IPI_INT_FRQ
#define IPC_IRQ_DISPATCH_FRQ_IN    TB_GNSS2BTWF_IPI_INT_FRQ_INVALID  
#define IPC_NAME            "nuttx-threadx-ipc"
#define IPC_DST             0

#define IPC_IRQ_MSG         1
#define IPC_PRIO_MSG        2
#define IPC_NORMAL_MSG      3

#define SEM_NAMELEN         8
#define SEM_FORMAT          "sem%d" 
#define readl(a)  (*((uint32_t*)(a)))

#define writel(value,addr) \
do \
{ \
    *(volatile uint32_t *)addr=value; \
}while(0)

#ifdef CONFIG_DEBUG_CORE_IPC_ERROR
	#define ipc_error(format, ...)     _err(format,  ##__VA_ARGS__)
#else
	#define ipc_error(format, ...)
#endif

#ifdef CONFIG_DEBUG_CORE_IPC_WARN
	#define ipc_warn(format, ...)     _warn(format,  ##__VA_ARGS__)
#else
	#define ipc_warn(format, ...)
#endif

#ifdef CONFIG_DEBUG_CORE_IPC_INFO
	#define ipc_info(format, ...)     _info(format,  ##__VA_ARGS__)
#else
	#define ipc_info(format, ...)
#endif

/**
 * smsg_ch_open -- open a channel for smsg
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_ch_open(uint8_t dst, uint8_t channel, int prio, int timeout);

/**
 * smsg_ch_close -- close a channel for smsg
 *
 * @dst: dest processor ID
 * @channel: channel ID
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_ch_close(uint8_t dst, uint8_t channel, int prio, int timeout);

/**
 * smsg_send -- send smsg
 *
 * @dst: dest processor ID
 * @msg: smsg body to be sent
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_send(uint8_t dst,uint8_t prio, struct smsg *msg, int timeout);
int smsg_send_irq(uint8_t dst,struct smsg *msg);
/**
 * smsg_recv -- poll and recv smsg
 *
 * @dst: dest processor ID
 * @msg: smsg body to be received, channel should be filled as input
 * @timeout: milliseconds, 0 means no wait, -1 means unlimited
 * @return: 0 on success, <0 on failue
 */
int smsg_recv(uint8_t dst, struct smsg *msg, int timeout);

void wakeup_smsg_task_all(struct k_sem *sem);

/* quickly fill a smsg body */
static inline void smsg_set(struct smsg *msg, uint8_t channel,
		uint8_t type, uint16_t flag, uint32_t value)
{
	msg->channel = channel;
	msg->type = type;
	msg->flag = flag;
	msg->value = value;
}

/* ack an open msg for modem recovery */
static inline void smsg_open_ack(uint8_t dst, uint16_t channel)
{
	struct smsg mopen;
	smsg_set(&mopen, channel, SMSG_TYPE_OPEN, SMSG_OPEN_MAGIC, 0);
	smsg_send(dst,IPC_PRIO_MSG, &mopen, -1);
}

/* ack an close msg for modem recovery */
static inline void smsg_close_ack(uint8_t dst, uint16_t channel)
{
	struct smsg mclose;
	smsg_set(&mclose, channel, SMSG_TYPE_CLOSE, SMSG_CLOSE_MAGIC, 0);
	smsg_send(dst,IPC_PRIO_MSG, &mclose, -1);
}

/* sblock structure: addr is the uncached virtual address */
struct sblock {
	void		*addr;
	uint32_t	length;
#ifdef CONFIG_ZERO_COPY_SIPX
        uint16_t        index;
        uint16_t        offset;
#endif
};

#define	SBLOCK_NOTIFY_GET	0x01
#define	SBLOCK_NOTIFY_RECV	0x02
#define	SBLOCK_NOTIFY_STATUS	0x04
#define	SBLOCK_NOTIFY_OPEN	0x08
#define	SBLOCK_NOTIFY_CLOSE	0x10

int sipc_probe(void);

#ifdef __cplusplus
}
#endif

#endif
