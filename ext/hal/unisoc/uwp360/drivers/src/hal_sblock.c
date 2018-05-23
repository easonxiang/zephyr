#include <nuttx/config.h>
#include <sched.h>
#include <errno.h>
#include <stdio.h>
#include <nuttx/kmalloc.h>
#include <time.h>

#include "ipc/sipc.h"
#include "ipc/sipc_priv.h"
#include "ipc/sblock.h"
#include "ipc/sc2355a_ipc_intf.h"

static struct sblock_mgr *sblocks[SIPC_ID_NR][SMSG_CH_NR];
static uint32_t record_share_addr = SBLOCK_SMEM_ADDR;
uint8_t cmd_hdr1[] = 
{0x10,0x1,0xc,0x0,0x55,0x49,0x0,0x0,0x0,0x0,0x0,0x0};

int sblock_get(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout);
int sblock_send(uint8_t dst, uint8_t channel,uint8_t prio, struct sblock *blk);

static int sblock_recover(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	//unsigned long pflags, qflags;
	int i, j;

	if (!sblock) {
		return -ENODEV;
	}
	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	sblock->state = SBLOCK_STATE_IDLE;
	wakeup_smsg_task_all(&ring->getwait);
	wakeup_smsg_task_all(&ring->recvwait);

    //spin_lock_irqsave(&ring->r_txlock, pflags);
	/* clean txblks ring */
	ringhd->txblk_wrptr = ringhd->txblk_rdptr;

	//spin_lock_irqsave(&ring->p_txlock, qflags);

    	/* recover txblks pool */
	poolhd->txblk_rdptr = poolhd->txblk_wrptr;
	for (i = 0, j = 0; i < poolhd->txblk_count; i++) {
		if (ring->txrecord[i] == SBLOCK_BLK_STATE_DONE) {
			ring->p_txblks[j].addr = i * sblock->txblksz + poolhd->txblk_addr;
			ring->p_txblks[j].length = sblock->txblksz;
			poolhd->txblk_wrptr = poolhd->txblk_wrptr + 1;
			j++;
		}
	}
    //spin_unlock_irqrestore(&ring->p_txlock, qflags);
	//spin_unlock_irqrestore(&ring->r_txlock, pflags);

//	spin_lock_irqsave(&ring->r_rxlock, pflags);
	/* clean rxblks ring */
	ringhd->rxblk_rdptr = ringhd->rxblk_wrptr;

//	spin_lock_irqsave(&ring->p_rxlock, qflags);
	/* recover rxblks pool */
	poolhd->rxblk_wrptr = poolhd->rxblk_rdptr;
	for (i = 0, j = 0; i < poolhd->rxblk_count; i++) {
		if (ring->rxrecord[i] == SBLOCK_BLK_STATE_DONE) {
			ring->p_rxblks[j].addr = i * sblock->rxblksz + poolhd->rxblk_addr;
			ring->p_rxblks[j].length = sblock->rxblksz;
			poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
			j++;
		}
	}
//	spin_unlock_irqrestore(&ring->p_rxlock, qflags);
//	spin_unlock_irqrestore(&ring->r_rxlock, pflags);

	return 0;
}

static int sblock_thread(int argc, FAR char *argv[])
{
	struct sblock_mgr *sblock = (struct sblock_mgr*)strtoul(argv[1], NULL, 16);
	struct smsg mcmd, mrecv;
	int rval;
	int recovery = 0;
	struct sched_param param = {.sched_priority = 90};
	struct sblock blk;
	int prio;
	//int i,val=15;
	uint8_t *recvdata;
	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(getpid(), SCHED_RR, &param);
    ipc_info("sblock thread %d %d 0x%x 0x%x 0x%x %d %d\n",getpid(),argc,strtoul(argv[0], NULL, 16),
	strtoul(argv[1], NULL, 16),sblock,sblock->dst,sblock->channel);

	/* since the channel open may hang, we call it in the sblock thread */
	switch(sblock->channel) {
		case SMSG_CH_BT:
            prio = IPC_PRIO_MSG;
            break;
        case SMSG_CH_WIFI_CTRL:
             prio = IPC_PRIO_MSG;
             break;
        case SMSG_CH_WIFI_DATA_NOR:
             prio = IPC_NORMAL_MSG;
            break;
        case SMSG_CH_WIFI_DATA_SPEC:
            prio = IPC_NORMAL_MSG;
            break;
        default :
        prio = IPC_NORMAL_MSG;
        break;
    }

	rval = smsg_ch_open(sblock->dst, sblock->channel,prio, -1);
	if (rval != 0) {
		ipc_info("Failed to open channel %d\n", sblock->channel);
		/* assign NULL to thread poniter as failed to open channel */
		sblock->thread = NULL;
		return rval;
	}

/* 	for (i=0;i<val;i++) {
		rval=sblock_get(0,sblock->channel,&blk,0);
		if (rval < 0)
			continue;
		sblock_send(0,sblock->channel,&blk);
	}
ipc_info("sblock open\n"); */
    while(1)
    {
		/* monitor sblock recv smsg */
		smsg_set(&mrecv, sblock->channel, 0, 0, 0);
		rval = smsg_recv(sblock->dst, &mrecv, -1);
		if (rval == -EIO || rval == -ENODEV) {
		/* channel state is FREE */
			sleep(1);
			continue;
		}
		ipc_info("sblock thread recv msg: dst=%d, channel=%d, "
				"type=%d, flag=0x%04x, value=0x%08x\n",
				sblock->dst, sblock->channel,
				mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* handle channel recovery */
			if (recovery) {
				if (sblock->handler) {
/*这个中断处理是在main.c里面注册的block的中断处理 是通知应用处理sblock的消息的*/
					sblock->handler(SBLOCK_NOTIFY_CLOSE, sblock->data);
				}
				sblock_recover(sblock->dst, sblock->channel);
			}
			smsg_open_ack(sblock->dst, sblock->channel);
			break;
        case SMSG_TYPE_CLOSE:
			/* handle channel recovery */
			smsg_close_ack(sblock->dst, sblock->channel);
			if (sblock->handler) {
				sblock->handler(SBLOCK_NOTIFY_CLOSE, sblock->data);
			}
			sblock->state = SBLOCK_STATE_IDLE;
			break;
        case SMSG_TYPE_CMD:
			/* respond cmd done for sblock init */
			smsg_set(&mcmd, sblock->channel, SMSG_TYPE_DONE,
					SMSG_DONE_SBLOCK_INIT, sblock->smem_addr);
			smsg_send(sblock->dst,prio, &mcmd, -1);
			sblock->state = SBLOCK_STATE_READY;
			recovery = 1;
			ipc_info("ap cp create %d channel success!\n",sblock->channel);
			if (sblock->channel == SMSG_CH_BT) {
                sprd_bt_irq_init();
            }

/* 			sprd_wifi_send(sblock->channel,prio,cmd_hdr1,sizeof(cmd_hdr1));
			sleep(1);
			sprd_wifi_send(sblock->channel,prio,cmd_hdr1,sizeof(cmd_hdr1));
			sleep(1);
			sprd_wifi_send(sblock->channel,prio,cmd_hdr1,sizeof(cmd_hdr1));
			sleep(1);
			sprd_wifi_send(sblock->channel,prio,cmd_hdr1,sizeof(cmd_hdr1)); */
			break;
        case SMSG_TYPE_EVENT:
            /* handle sblock send/release events */
            switch (mrecv.flag) {
                case SMSG_EVENT_SBLOCK_SEND:
					rval = sblock_receive(0, sblock->channel, &blk, 0);
					recvdata = (uint8_t *)blk.addr;
					ipc_info("sblock recv 0x%x %x %x %x %x %x %x",recvdata[0],recvdata[1],recvdata[2],
					recvdata[3],recvdata[4],recvdata[5],recvdata[6]);
					if (rval != 0) {
						ipc_info("sblock recv error\n");
					}
                    if (sblock->callback) {
						sblock->callback(sblock->channel, blk.addr,blk.length);
                    }
					sblock_release(0,sblock->channel, &blk);
                    break;
				case SMSG_EVENT_SBLOCK_RECV:
					break;
                case SMSG_EVENT_SBLOCK_RELEASE:
				    sblock_release(0,sblock->channel, &blk);
				  // wakeup_smsg_task_all(&(sblock->ring->getwait));
                    //if (sblock->handler) {
                      //  sblock->handler(SBLOCK_NOTIFY_GET, sblock->data);
                    //}
                    break;
                default:
                    rval = 1;
                    break;
            }
            break;
		default:
			rval = 1;
			break;
		};
		if (rval) {
			ipc_info("non-handled sblock msg: %d-%d, %d, %d, %d\n",
					sblock->dst, sblock->channel,
					mrecv.type, mrecv.flag, mrecv.value);
			rval = 0;
		}
	}
    
}

int sblock_create(uint8_t dst, uint8_t channel,
		uint32_t txblocknum, uint32_t txblocksize,
		uint32_t rxblocknum, uint32_t rxblocksize)
{
	struct sblock_mgr *sblock = NULL;
	volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	uint32_t hsize;
	int i, result;
	char arg[16];
	char *argv[2];
    char thread_name[20];
	sblock = malloc(sizeof(struct sblock_mgr));
	if (!sblock) { 
		ipc_error("malloc failed\n");
		return -ENOMEM;
	}

	sblock->state = SBLOCK_STATE_IDLE;
	sblock->dst = dst;
	sblock->channel = channel;
	txblocksize = SBLOCKSZ_ALIGN(txblocksize,SBLOCK_ALIGN_BYTES);
	rxblocksize = SBLOCKSZ_ALIGN(rxblocksize,SBLOCK_ALIGN_BYTES);
	sblock->txblksz = txblocksize;
	sblock->rxblksz = rxblocksize;
	sblock->txblknum = txblocknum;
	sblock->rxblknum = rxblocknum;
    sblock->handler = NULL;
	sblock->callback = NULL;
	/* allocate smem */
	hsize = sizeof(struct sblock_header);
	sblock->smem_size = hsize +						/* for header*/
		txblocknum * txblocksize + rxblocknum * rxblocksize + 		/* for blks */
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks) + 	/* for ring*/
		(txblocknum + rxblocknum) * sizeof(struct sblock_blks); 	/* for pool*/
//这里分配共享内存 在NuttX里面sblock的共享内存怎么来确定
	sblock->smem_addr = record_share_addr;//smem_alloc(sblock->smem_size);
	record_share_addr += sblock->smem_size;
	ipc_info( "smem_addr 0x%x record_share_addr 0x%x\n",sblock->smem_addr,record_share_addr);
	if (!sblock->smem_addr) {
		ipc_info( "Failed to allocate smem for sblock\n");
		free(sblock);
		return -ENOMEM;
	}
#if 0    
    /*共享内存映射到虚拟地址 在这里不需要了 用的是同一个地址*/
	sblock->smem_virt = SBLOCK_SMEM_ADDR;//ioremap_nocache(sblock->smem_addr, sblock->smem_size);
	if (!sblock->smem_virt) {
		ipc_info("Failed to map smem for sblock\n");
		//smem_free(sblock->smem_addr, sblock->smem_size);
		kfree(sblock);
		return -EFAULT;
	}
#endif   

	/* initialize ring and header */
	sblock->ring = malloc(sizeof(struct sblock_ring));
	if (!sblock->ring) {
		ipc_info( "Failed to allocate ring for sblock\n");
		//smem_free(sblock->smem_addr, sblock->smem_size);
		free(sblock);
		return -ENOMEM;
	}

    ringhd = (volatile struct sblock_ring_header *)(sblock->smem_addr);
	ringhd->txblk_addr = sblock->smem_addr + hsize;
	ringhd->txblk_count = txblocknum;
	ringhd->txblk_size = txblocksize;
	ringhd->txblk_rdptr = 0;
	ringhd->txblk_wrptr = 0;
	ringhd->txblk_blks = sblock->smem_addr + hsize +
		txblocknum * txblocksize + rxblocknum * rxblocksize;  //这个地址是指向后面的 blk的管理block ring的地址
	ringhd->rxblk_addr = ringhd->txblk_addr + txblocknum * txblocksize;
	ringhd->rxblk_count = rxblocknum;
	ringhd->rxblk_size = rxblocksize;
	ringhd->rxblk_rdptr = 0;
	ringhd->rxblk_wrptr = 0;
	ringhd->rxblk_blks = ringhd->txblk_blks + txblocknum * sizeof(struct sblock_blks);

	poolhd = (volatile struct sblock_ring_header *)(sblock->smem_addr + sizeof(struct sblock_ring_header));
	poolhd->txblk_addr = sblock->smem_addr + hsize;
	poolhd->txblk_count = txblocknum;
	poolhd->txblk_size = txblocksize;
	poolhd->txblk_rdptr = 0;
	poolhd->txblk_wrptr = 0;
	poolhd->txblk_blks = ringhd->rxblk_blks + rxblocknum * sizeof(struct sblock_blks);
	poolhd->rxblk_addr = ringhd->txblk_addr + txblocknum * txblocksize;
	poolhd->rxblk_count = rxblocknum;
	poolhd->rxblk_size = rxblocksize;
	poolhd->rxblk_rdptr = 0;
	poolhd->rxblk_wrptr = 0;
	poolhd->rxblk_blks = poolhd->txblk_blks + txblocknum * sizeof(struct sblock_blks);
    ipc_info("0x%x ringhd %x poolhd %x\n",sblock,ringhd,poolhd);
	sblock->ring->txrecord = malloc(sizeof(int) * txblocknum);
	if (!sblock->ring->txrecord) {
		ipc_info( "Failed to allocate memory for txrecord\n");
		//smem_free(sblock->smem_addr, sblock->smem_size);
		free(sblock->ring);
		free(sblock);
		return -ENOMEM;
	}

	sblock->ring->rxrecord = malloc(sizeof(int) * rxblocknum);
	if (!sblock->ring->rxrecord) {
		ipc_info( "Failed to allocate memory for rxrecord\n");
		//smem_free(sblock->smem_addr, sblock->smem_size);
		free(sblock->ring->txrecord);
		free(sblock->ring);
		free(sblock);
		return -ENOMEM;
	}

	sblock->ring->header =(struct sblock_header	*)sblock->smem_addr;
   //这个地址实际上就是虚拟地址上的 ringhd->txblk_addr地址的对应值 完全对应了一套
	sblock->ring->r_txblks = (struct sblock_blks *)ringhd->txblk_blks;

	sblock->ring->r_rxblks = (struct sblock_blks *)ringhd->rxblk_blks;

	sblock->ring->p_txblks = (struct sblock_blks *)poolhd->txblk_blks;
	sblock->ring->p_rxblks = (struct sblock_blks *)poolhd->rxblk_blks;
	sblock->ring->txblk_virt = (void *)ringhd->txblk_addr;
	sblock->ring->rxblk_virt = (void *)ringhd->rxblk_addr;
/*这里是将每个blk的地址放到pool里面管理*/
	for (i = 0; i < txblocknum; i++) {
		sblock->ring->p_txblks[i].addr = poolhd->txblk_addr + i * txblocksize;
		sblock->ring->p_txblks[i].length = txblocksize;
		sblock->ring->txrecord[i] = SBLOCK_BLK_STATE_DONE;
		poolhd->txblk_wrptr++;
	}
	for (i = 0; i < rxblocknum; i++) {
		sblock->ring->p_rxblks[i].addr = poolhd->rxblk_addr + i * rxblocksize;
		sblock->ring->p_rxblks[i].length = rxblocksize;
		sblock->ring->rxrecord[i] = SBLOCK_BLK_STATE_DONE;
		poolhd->rxblk_wrptr++;
	}

    sblock->ring->yell = 1;
    sem_init(&sblock->ring->getwait, 0, 0);   
    sem_init(&sblock->ring->recvwait, 0, 0); 
    //spin_lock_init(&sblock->ring->r_txlock);
	//spin_lock_init(&sblock->ring->r_rxlock);
	//spin_lock_init(&sblock->ring->p_txlock);
	//spin_lock_init(&sblock->ring->p_rxlock);   
	sblocks[dst][channel]=sblock;
	sblock->state = SBLOCK_STATE_READY;
	snprintf(arg, 16, "%p", sblock); /* task_create doesn't handle binary arguments. */
	argv[0] = arg;

    ipc_info("r_txblks 0x%x 0x%x 0x%x 0x%x\n",sblock->ring->r_txblks,sblock->ring->r_rxblks,sblock->ring->p_txblks,sblock->ring->p_rxblks,poolhd->rxblk_blks);
    snprintf(thread_name,20,"block_task_%d",channel);

    sblock->thread_pid = task_create(thread_name, CONFIG_SBLOCK_THREAD_DEFPRIO ,CONFIG_SBLOCK_THREAD_STACKSIZE,
			(main_t)sblock_thread, (FAR char * const *)argv);
    if ((sblock->thread < 0)) {
		ipc_info("Failed to create kthread: sblock-%d-%d\n", dst, channel);
		//smem_free(sblock->smem_addr, sblock->smem_size);
		free(sblock->ring->txrecord);
		free(sblock->ring->rxrecord);
		free(sblock->ring);
		free(sblock);
		return result;
	}

	sblocks[dst][channel]=sblock;

	return 0;
}

void sblock_destroy(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];
    int prio;
	if (sblock == NULL) {
		return;
	}
	switch(sblock->channel) {
        case SMSG_CH_WIFI_CTRL:
             prio = IPC_PRIO_MSG;
             break;
        case SMSG_CH_WIFI_DATA_NOR:
             prio = IPC_NORMAL_MSG;
            break;
        case SMSG_CH_WIFI_DATA_SPEC:
            prio = IPC_NORMAL_MSG;
            break;
        default :
        prio = IPC_NORMAL_MSG;
        break;
    }
    sblock->state = SBLOCK_STATE_IDLE;
	smsg_ch_close(dst, channel,prio, -1);

    	/* stop sblock thread if it's created successfully and still alive */
	if ((sblock->thread_pid > 0)) {
		task_delete(sblock->thread_pid);
	}

	if (sblock->ring) {
		wakeup_smsg_task_all(&sblock->ring->recvwait);
		wakeup_smsg_task_all(&sblock->ring->getwait);
		if (sblock->ring->txrecord) {
			free(sblock->ring->txrecord);
		}
		if (sblock->ring->rxrecord) {
			free(sblock->ring->rxrecord);
		}
		free(sblock->ring);
	}

    //smem_free(sblock->smem_addr, sblock->smem_size);
	free(sblock);

	sblocks[dst][channel] = NULL;
}

int sblock_register_callback(uint8_t channel,
		int (*callback)(int ch, void *data,int len))
{
	int dst=0;
	struct sblock_mgr *sblock = sblocks[dst][channel];
    ipc_info("%d channel=%d 0x%x\n", dst, channel,callback);
	if (!sblock) {
		ipc_info("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	sblock->callback = callback;

	return 0;
}

int sblock_register_notifier(uint8_t dst, uint8_t channel,
		void (*handler)(int event, void *data), void *data)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];

	if (!sblock) {
		ipc_info("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}
#ifndef CONFIG_SIPC_WCN
	if (sblock->handler) {
		ipc_info( "sblock handler already registered\n");
		return -EBUSY;
	}
#endif
	sblock->handler = handler;
	sblock->data = data;

	return 0;
}


/*这个地方是发送失败了 再重新放入pool*/
void sblock_put(uint8_t dst, uint8_t channel, struct sblock *blk)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	//unsigned long flags;
	int txpos;
	int index;

	if (!sblock) {
		return;
	}

    ring = sblock->ring;
    poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

    // spin_lock_irqsave(&ring->p_txlock, flags);
    txpos = sblock_get_ringpos(poolhd->txblk_rdptr-1,poolhd->txblk_count);
    ring->r_txblks[txpos].addr = (uint32_t)blk->addr;
    ring->r_txblks[txpos].length = poolhd->txblk_size;
    poolhd->txblk_rdptr = poolhd->txblk_rdptr - 1;
	ipc_info("%d %d \n",poolhd->txblk_rdptr,txpos);
    /*  在接收的那里需要从pool中获取blk的等待 这里进行了释放所以需要唤醒所有在这个信号量上等待的task*/
    if ((int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr) == 1) {
		wakeup_smsg_task_all(&(ring->getwait));
	}

	index = sblock_get_index((blk->addr - ring->txblk_virt), sblock->txblksz);
	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	//spin_unlock_irqrestore(&ring->p_txlock, flags);
}

int sblock_get(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	//volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	int txpos, index;
	int rval = 0;
	//unsigned long flags;
    struct timespec time;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		ipc_info( "sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}
    time.tv_sec = timeout;
	ring = sblock->ring;
	//ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);
    ipc_info("%d %d\n",poolhd->txblk_rdptr,poolhd->txblk_wrptr);

    if (poolhd->txblk_rdptr == poolhd->txblk_wrptr) {
		if (timeout == 0) {
			/* no wait */
			ipc_info("sblock_get %d-%d is empty!\n",dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = sem_wait(&(ring->getwait));
			if (rval < 0) {
				ipc_info( "sblock_get wait interrupted!\n");
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				ipc_info( "sblock_get sblock state is idle!\n");
				rval = -EIO;
			}
		} else {
			(void)clock_gettime(CLOCK_REALTIME, &time);
			ipc_info("sec=%d ns=%d %ld",time.tv_sec,time.tv_nsec,time.tv_nsec);
			time.tv_sec += timeout;
			/* wait timeout */
			rval = sem_timedwait(&(ring->getwait),&time);
			if (rval < 0) {
				ipc_info( "sblock_get wait interrupted!\n");
			} else if (rval == 0) {
				ipc_info( "sblock_get wait timeout!\n");
				rval = -ETIME;
			}

			if(sblock->state == SBLOCK_STATE_IDLE) {
				ipc_info( "sblock_get sblock state is idle!\n");
				rval = -EIO;
			}
		}
	}

	if (rval < 0) {
		return rval;
	}
	/* multi-gotter may cause got failure */
	//spin_lock_irqsave(&ring->p_txlock, flags);
	if (poolhd->txblk_rdptr != poolhd->txblk_wrptr &&
			sblock->state == SBLOCK_STATE_READY) {
		txpos = sblock_get_ringpos(poolhd->txblk_rdptr, poolhd->txblk_count);
		ipc_info("txpos %d poolhd->txblk_rdptr %d\n",txpos,poolhd->txblk_rdptr);
		blk->addr = (void *)ring->p_txblks[txpos].addr;
		blk->length = poolhd->txblk_size;
		poolhd->txblk_rdptr = poolhd->txblk_rdptr + 1;
		index = sblock_get_index((blk->addr - ring->txblk_virt), sblock->txblksz);
		ring->txrecord[index] = SBLOCK_BLK_STATE_PENDING;
	} else {
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	//spin_unlock_irqrestore(&ring->p_txlock, flags);

	return rval;
}

static int sblock_send_ex(uint8_t dst, uint8_t channel,uint8_t prio, struct sblock *blk, bool yell)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	struct smsg mevt;
	int txpos, index;
	int rval = 0;
	//unsigned long flags;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		ipc_info("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}


	ipc_info("sblock_send: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

    ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	//spin_lock_irqsave(&ring->r_txlock, flags);
	txpos = sblock_get_ringpos(ringhd->txblk_wrptr, ringhd->txblk_count);
	ring->r_txblks[txpos].addr = (uint32_t)blk->addr;
	ring->r_txblks[txpos].length = blk->length;
    ringhd->txblk_wrptr = ringhd->txblk_wrptr + 1;
    ipc_info("channel=%d, wrptr=%d, txpos=%d,%x,addr=%x\n",
			channel, ringhd->txblk_wrptr, txpos,&ring->r_txblks[txpos],ring->r_txblks[txpos].addr);

    if (sblock->state == SBLOCK_STATE_READY) {
            if(yell) {
		        smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBLOCK_SEND, 0);
		        rval = smsg_send(dst,prio, &mevt, 0);
                }
                else if(!ring->yell) {
                        if(((int)(ringhd->txblk_wrptr - ringhd->txblk_rdptr) == 1) /*&&
                                ((int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr) == (sblock->txblknum - 1))*/) {
                                ring->yell = 1;
                        }
                }
	}

    index = sblock_get_index((blk->addr - ring->txblk_virt), sblock->txblksz);
 	ring->txrecord[index] = SBLOCK_BLK_STATE_DONE;

	//spin_unlock_irqrestore(&ring->r_txlock, flags);

	return rval ;
}

int sblock_send(uint8_t dst, uint8_t channel,uint8_t prio, struct sblock *blk)
{
	return sblock_send_ex(dst, channel, prio, blk, true);
}

int sblock_send_prepare(uint8_t dst, uint8_t channel,uint8_t prio, struct sblock *blk)
{
	return sblock_send_ex(dst, channel,prio, blk, false);
}

int sblock_send_finish(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring;
	//volatile struct sblock_ring_header *ringhd;
	struct smsg mevt;
	int rval=0;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		ipc_info("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}

	ring = sblock->ring;
	//ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	if (ring->yell) {
                ring->yell = 0;
		smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBLOCK_SEND, 0);
		rval = smsg_send(dst,IPC_NORMAL_MSG, &mevt, 0);
	}

	return rval;
}

int sblock_receive(uint8_t dst, uint8_t channel, struct sblock *blk, int timeout)
{
	struct sblock_mgr *sblock = sblocks[dst][channel];
	struct sblock_ring *ring;
	volatile struct sblock_ring_header *ringhd;
	int rxpos, index, rval = 0;
	//unsigned long flags;
	struct timespec time;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		ipc_info("sblock-%d-%d not ready!\n", dst, channel);
		return sblock ? -EIO : -ENODEV;
	}
	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	ipc_info("sblock_receive: dst=%d, channel=%d, timeout=%d\n",dst, channel, timeout);
	ipc_info("sblock_receive: channel=%d, wrptr=%d, rdptr=%d",channel, ringhd->rxblk_wrptr, ringhd->rxblk_rdptr);

	if (ringhd->rxblk_wrptr == ringhd->rxblk_rdptr) {
		if (timeout == 0) {
			/* no wait */
			ipc_info("sblock_receive %d-%d is empty!\n",dst, channel);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = sem_wait(&(ring->recvwait));
			if (rval < 0) {
				ipc_info( "sblock_receive wait interrupted!\n");
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				ipc_info( "sblock_receive sblock state is idle!\n");
				rval = -EIO;
			}

		} else {
			(void)clock_gettime(CLOCK_REALTIME, &time);
			ipc_info("sec=%d ns=%d %ld",time.tv_sec,time.tv_nsec,time.tv_nsec);
			time.tv_sec += timeout;
			/* wait timeout */
			rval = sem_timedwait(&(ring->recvwait),&time);
			if (rval < 0) {
				ipc_info( "sblock_receive wait interrupted!\n");
			} else if (rval == 0) {
				ipc_info( "sblock_receive wait timeout!\n");
				rval = -ETIME;
			}

			if (sblock->state == SBLOCK_STATE_IDLE) {
				ipc_info( "sblock_receive sblock state is idle!\n");
				rval = -EIO;
			}
		}
	}

	if (rval < 0) {
		return rval;
	}

	/* multi-receiver may cause recv failure */
	//spin_lock_irqsave(&ring->r_rxlock, flags);

	if (ringhd->rxblk_wrptr != ringhd->rxblk_rdptr &&
			sblock->state == SBLOCK_STATE_READY) {
		rxpos = sblock_get_ringpos(ringhd->rxblk_rdptr, ringhd->rxblk_count);
		blk->addr = (void *)ring->r_rxblks[rxpos].addr;
		blk->length = ring->r_rxblks[rxpos].length;
		ringhd->rxblk_rdptr = ringhd->rxblk_rdptr + 1;
		ipc_info("sblock_receive: channel=%d, rxpos=%d, addr=%p, len=%d\n",
			channel, rxpos, blk->addr, blk->length);
		index = sblock_get_index((blk->addr - ring->rxblk_virt), sblock->rxblksz);
		ring->rxrecord[index] = SBLOCK_BLK_STATE_PENDING;
	} else {
		rval = sblock->state == SBLOCK_STATE_READY ? -EAGAIN : -EIO;
	}
	//spin_unlock_irqrestore(&ring->r_rxlock, flags);
    ipc_info("%d\n",index);
	return rval;
}


int sblock_get_arrived_count(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *ringhd = NULL;
	int blk_count = 0;
	//unsigned long flags;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		ipc_info("sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);

	//spin_lock_irqsave(&ring->r_rxlock, flags);
	blk_count = (int)(ringhd->rxblk_wrptr - ringhd->rxblk_rdptr);
	//spin_unlock_irqrestore(&ring->r_rxlock, flags);

	return blk_count;

}

int sblock_get_free_count(uint8_t dst, uint8_t channel)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	int blk_count = 0;


	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		ipc_info( "sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ring = sblock->ring;
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	//spin_lock_irqsave(&ring->p_txlock, flags);
	blk_count = (int)(poolhd->txblk_wrptr - poolhd->txblk_rdptr);
	//spin_unlock_irqrestore(&ring->p_txlock, flags);

	return blk_count;
}


int sblock_release(uint8_t dst, uint8_t channel, struct sblock *blk)
{
	struct sblock_mgr *sblock = (struct sblock_mgr *)sblocks[dst][channel];
	struct sblock_ring *ring = NULL;
	//volatile struct sblock_ring_header *ringhd = NULL;
	volatile struct sblock_ring_header *poolhd = NULL;
	struct smsg mevt;
	int rxpos;
	int index;

	if (!sblock || sblock->state != SBLOCK_STATE_READY) {
		ipc_info( "sblock-%d-%d not ready!\n", dst, channel);
		return -ENODEV;
	}

	ipc_info("sblock_release: dst=%d, channel=%d, addr=%p, len=%d\n",
			dst, channel, blk->addr, blk->length);

	ring = sblock->ring;
	//ringhd = (volatile struct sblock_ring_header *)(&ring->header->ring);
	poolhd = (volatile struct sblock_ring_header *)(&ring->header->pool);

	//spin_lock_irqsave(&ring->p_rxlock, flags);
	rxpos = sblock_get_ringpos(poolhd->rxblk_wrptr, poolhd->rxblk_count);
	ring->p_rxblks[rxpos].addr = (uint32_t)blk->addr;
	ring->p_rxblks[rxpos].length = poolhd->rxblk_size;
	poolhd->rxblk_wrptr = poolhd->rxblk_wrptr + 1;
	ipc_info("sblock_release: addr=%x\n", ring->p_rxblks[rxpos].addr);

	if((int)(poolhd->rxblk_wrptr - poolhd->rxblk_rdptr) == 1 &&
			sblock->state == SBLOCK_STATE_READY) {
		/* send smsg to notify the peer side */
		smsg_set(&mevt, channel, SMSG_TYPE_EVENT, SMSG_EVENT_SBLOCK_RELEASE, 0);
		smsg_send(dst,IPC_NORMAL_MSG, &mevt, -1);
	}

	index = sblock_get_index((blk->addr - ring->rxblk_virt), sblock->rxblksz);
	ring->rxrecord[index] = SBLOCK_BLK_STATE_DONE;

	//spin_unlock_irqrestore(&ring->p_rxlock, flags);

	return 0;
}