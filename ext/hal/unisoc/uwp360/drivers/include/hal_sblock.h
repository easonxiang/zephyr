#ifndef __SBLOCK_H
#define __SBLOCK_H

#include <nuttx/config.h>
#include <stdint.h>
#include <nuttx/spinlock.h>
#include <semaphore.h>
/* flag for CMD/DONE msg type */
#define SMSG_CMD_SBLOCK_INIT		0x0001
#define SMSG_DONE_SBLOCK_INIT		0x0002

/* flag for EVENT msg type */
#define SMSG_EVENT_SBLOCK_SEND		0x0001
#define SMSG_EVENT_SBLOCK_RECV		0x0003
#define SMSG_EVENT_SBLOCK_RELEASE	0x0002

#define SBLOCK_STATE_IDLE		0
#define SBLOCK_STATE_READY		1

#define SBLOCK_BLK_STATE_DONE 		0
#define SBLOCK_BLK_STATE_PENDING 	1

#define SBLOCK_SMEM_ADDR              0x00167000

#define BLOCK_HEADROOM_SIZE     16

#define CTRLPATH_TX_BLOCK_SIZE  (1600+BLOCK_HEADROOM_SIZE)
#define CTRLPATH_TX_BLOCK_NUM   3
#define CTRLPATH_RX_BLOCK_SIZE  (1536+BLOCK_HEADROOM_SIZE)
#define CTRLPATH_RX_BLOCK_NUM   5

#define DATAPATH_NOR_TX_BLOCK_SIZE  (188+BLOCK_HEADROOM_SIZE)
#define DATAPATH_NOR_TX_BLOCK_NUM   50
#define DATAPATH_NOR_RX_BLOCK_SIZE  (188+BLOCK_HEADROOM_SIZE)
#define DATAPATH_NOR_RX_BLOCK_NUM   50

#define DATAPATH_SPEC_TX_BLOCK_SIZE  (1664+BLOCK_HEADROOM_SIZE)
#define DATAPATH_SPEC_TX_BLOCK_NUM   2
#define DATAPATH_SPEC_RX_BLOCK_SIZE  (1664+BLOCK_HEADROOM_SIZE)
#define DATAPATH_SPEC_RX_BLOCK_NUM   5

#define BT_TX_BLOCK_SIZE  (4)
#define BT_TX_BLOCK_NUM   (1)
#define BT_RX_BLOCK_SIZE  (1300)
#define BT_RX_BLOCK_NUM   5

struct sblock_blks {
	uint32_t		addr; /*phy address*/
	uint32_t		length;
};

/* ring block header */
struct sblock_ring_header {
	/* get|send-block info */
	uint32_t		txblk_addr;
	uint32_t		txblk_count;
	uint32_t		txblk_size;
	uint32_t		txblk_blks;
	uint32_t		txblk_rdptr;
	uint32_t		txblk_wrptr;

	/* release|recv-block info */
	uint32_t		rxblk_addr;
	uint32_t		rxblk_count;
	uint32_t		rxblk_size;
	uint32_t		rxblk_blks;
	uint32_t		rxblk_rdptr;
	uint32_t		rxblk_wrptr;
};

struct sblock_header {
	struct sblock_ring_header ring;
	struct sblock_ring_header pool;
};


struct sblock_ring {
	struct sblock_header	*header;
	void			*txblk_virt; /* virt of header->txblk_addr */
	void			*rxblk_virt; /* virt of header->rxblk_addr */

	struct sblock_blks	*r_txblks;     /* virt of header->ring->txblk_blks */
	struct sblock_blks	*r_rxblks;     /* virt of header->ring->rxblk_blks */
	struct sblock_blks 	*p_txblks;     /* virt of header->pool->txblk_blks */
	struct sblock_blks 	*p_rxblks;     /* virt of header->pool->rxblk_blks */

	int 			*txrecord; /* record the state of every txblk */
	int 			*rxrecord; /* record the state of every rxblk */
    int             yell; /* need to notify cp */
//	spinlock_t		r_txlock; /* send *///
//	spinlock_t		r_rxlock; /* recv *//
//	spinlock_t 		p_txlock; /* get */
//	spinlock_t 		p_rxlock; /* release */

	sem_t	getwait;
	sem_t	recvwait;
};

struct sblock_mgr {
	uint8_t			dst;
	uint8_t			channel;
	uint32_t		state;

	void			*smem_virt;
	uint32_t		smem_addr;
	uint32_t		smem_size;

	uint32_t		txblksz;
	uint32_t		rxblksz;
	uint32_t		txblknum;
	uint32_t		rxblknum;

	struct sblock_ring	*ring;
	struct tcb_s	*thread;
    pid_t            thread_pid;

	void			(*handler)(int event, void *data);
	int			    (*callback)(int ch, void *data,int len);
	void			*data;
};

#define SBLOCKSZ_ALIGN(blksz,size) (((blksz)+((size)-1))&(~((size)-1)))
#define is_power_of_2(x)	((x) != 0 && (((x) & ((x) - 1)) == 0))
#define CONFIG_SBLOCK_THREAD_DEFPRIO 50
#define CONFIG_SBLOCK_THREAD_STACKSIZE 1024

#ifdef CONFIG_64BIT
#define SBLOCK_ALIGN_BYTES (8)
#else
#define SBLOCK_ALIGN_BYTES (4)
#endif

static inline uint32_t sblock_get_index(uint32_t x, uint32_t y)
{
	return (x / y);
}

static inline uint32_t sblock_get_ringpos(uint32_t x, uint32_t y)
{
	return is_power_of_2(y) ? (x & (y - 1)) : (x % y);
}

int sblock_create(uint8_t dst, uint8_t channel,
		uint32_t txblocknum, uint32_t txblocksize,
		uint32_t rxblocknum, uint32_t rxblocksize);
int sblock_release(uint8_t dst, uint8_t channel, struct sblock *blk);
int sblock_get_free_count(uint8_t dst, uint8_t channel);
int sblock_get_arrived_count(uint8_t dst, uint8_t channel);
int sblock_receive(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout);
int sblock_send_finish(uint8_t dst, uint8_t channel);
int sblock_send(uint8_t dst, uint8_t channel,uint8_t prio, struct sblock *blk);
int sblock_send_prepare(uint8_t dst, uint8_t channel,uint8_t prio, struct sblock *blk);
int sblock_get(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout);
void sblock_put(uint8_t dst, uint8_t channel, struct sblock *blk);
int sblock_register_notifier(uint8_t dst, uint8_t channel,
		void (*handler)(int event, void *data), void *data);
int sblock_register_callback(uint8_t channel,
		int (*callback)(int ch, void *data,int len));
#endif