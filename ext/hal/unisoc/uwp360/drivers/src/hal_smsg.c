#include <nuttx/config.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <debug.h>
#include <semaphore.h>
#include <nuttx/kmalloc.h>

#include "ipc/sipc.h"
#include "ipc/sipc_priv.h"
#include "ipc/sipc_ipi.h"
static struct smsg_ipc *smsg_ipcs[SIPC_ID_NR];

void wakeup_smsg_task_all( sem_t *sem)
{
    int svalue;
    int ret;

    for(;;)
    {
        ret = sem_getvalue(sem,&svalue);
		if (ret == ERROR) {
			break;
		}
        if (svalue < 0)
        {
            sem_post(sem);
			ipc_info("wake task %d %x\n",svalue,sem);
        }
        else
        {
            break;
        }
    }
}

void smsg_clear_queue(struct smsg_ipc *ipc,int prio)
{
	switch (prio) {
		case IPC_IRQ_MSG:
			writel(0, ipc->deq_irq_dis.rxbuf_rdptr);
			writel(0, ipc->deq_irq_dis.rxbuf_wrptr);
			writel(0, ipc->deq_irq_dis.txbuf_rdptr);
			writel(0, ipc->deq_irq_dis.txbuf_wrptr);
		break;
		case IPC_PRIO_MSG:
			writel(0, ipc->deq_priority_msg.rxbuf_rdptr);
			writel(0, ipc->deq_priority_msg.rxbuf_wrptr);
			writel(0, ipc->deq_priority_msg.txbuf_rdptr);
			writel(0, ipc->deq_priority_msg.txbuf_wrptr);
		break;
		case IPC_NORMAL_MSG:
			writel(0, ipc->deq_normal_msg.rxbuf_rdptr);
			writel(0, ipc->deq_normal_msg.rxbuf_wrptr);
			writel(0, ipc->deq_normal_msg.txbuf_rdptr);
			writel(0, ipc->deq_normal_msg.txbuf_wrptr);
		break;
		case 0:
			writel(0, ipc->deq_irq_dis.rxbuf_rdptr);
			writel(0, ipc->deq_irq_dis.rxbuf_wrptr);
			writel(0, ipc->deq_irq_dis.txbuf_rdptr);
			writel(0, ipc->deq_irq_dis.txbuf_wrptr);

			writel(0, ipc->deq_priority_msg.rxbuf_rdptr);
			writel(0, ipc->deq_priority_msg.rxbuf_wrptr);
			writel(0, ipc->deq_priority_msg.txbuf_rdptr);
			writel(0, ipc->deq_priority_msg.txbuf_wrptr);

			writel(0, ipc->deq_normal_msg.rxbuf_rdptr);
			writel(0, ipc->deq_normal_msg.rxbuf_wrptr);
			writel(0, ipc->deq_normal_msg.txbuf_rdptr);
			writel(0, ipc->deq_normal_msg.txbuf_wrptr);
		break;
		default:
		break;
	}
}
int smsg_msg_dispatch_thread(int argc, FAR char *argv[])
{
	struct smsg_ipc *ipc = smsg_ipcs[0];
	struct smsg *msg;
	struct smsg_channel *ch;
	uintptr_t rxpos;
	uint32_t wr;
    struct smsg_queue *msg_queue;
	while(1){
		ipc_info("\n\nipi irq answer %d %d %d %d %d %d\n",readl(ipc->deq_irq_dis.rxbuf_wrptr) ,
		readl(ipc->deq_irq_dis.rxbuf_rdptr),readl(ipc->deq_priority_msg.rxbuf_wrptr),readl(ipc->deq_priority_msg.rxbuf_rdptr),
		readl(ipc->deq_normal_msg.rxbuf_wrptr),readl(ipc->deq_normal_msg.rxbuf_rdptr));
		if (readl(ipc->deq_priority_msg.rxbuf_wrptr) != readl(ipc->deq_priority_msg.rxbuf_rdptr)) {
			msg_queue = &(ipc->deq_priority_msg);
		} else if (readl(ipc->deq_normal_msg.rxbuf_wrptr) != readl(ipc->deq_normal_msg.rxbuf_rdptr)){
				msg_queue = &(ipc->deq_normal_msg);
		}else {
				sem_wait(ipc->dis_semwait);//do wait sleep
				if (readl(ipc->deq_priority_msg.rxbuf_wrptr) != readl(ipc->deq_priority_msg.rxbuf_rdptr)) {
					msg_queue = &(ipc->deq_priority_msg);
			} else {
					msg_queue = &(ipc->deq_normal_msg);
				}
		}

		while (readl(msg_queue->rxbuf_wrptr) != readl(msg_queue->rxbuf_rdptr)) {
			rxpos = (readl(msg_queue->rxbuf_rdptr) & (msg_queue->rxbuf_size - 1)) *
					sizeof (struct smsg) + msg_queue->rxbuf_addr;
			msg = (struct smsg *)rxpos;
			ipc_info("irq get smsg: wrptr=%u, rdptr=%u, rxpos=0x%lx\n",
					readl(msg_queue->rxbuf_wrptr), readl(msg_queue->rxbuf_rdptr), rxpos);
			ipc_info("irq read smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
					msg->channel, msg->type, msg->flag, msg->value);

			if(msg->type == SMSG_TYPE_DIE) {
				writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);
				continue;
			}
			if (msg->channel >= SMSG_CH_NR || msg->type >= SMSG_TYPE_NR) {
					/* invalid msg */
				ipc_info("invalid smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
					msg->channel, msg->type, msg->flag, msg->value);

					/* update smsg rdptr */
				writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);
				continue;
			}

			ch = ipc->channels[msg->channel];
			if (!ch) {
				if (ipc->states[msg->channel] == CHAN_STATE_UNUSED &&
								msg->type == SMSG_TYPE_OPEN &&
								msg->flag == SMSG_OPEN_MAGIC) {

						ipc->states[msg->channel] = CHAN_STATE_WAITING;
				} else {
						/* drop this bad msg since channel is not opened */
						ipc_info("smsg channel %d not opened! "
								"drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
								msg->channel, msg->type, msg->flag, msg->value);
				}
				/* update smsg rdptr */
				writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);

				continue;
			}

			ipc->busy[msg->channel]++;

			if ((int)(readl(ch->wrptr) - readl(ch->rdptr)) >= SMSG_CACHE_NR) {
				/* msg cache is full, drop this msg */
				ipc_info("smsg channel %d recv cache is full! "
						"drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
						msg->channel, msg->type, msg->flag, msg->value);
			} else {
				/* write smsg to cache */
				wr = readl(ch->wrptr) & (SMSG_CACHE_NR - 1);
				memcpy(&(ch->caches[wr]), msg, sizeof(struct smsg));
				writel(readl(ch->wrptr) + 1, ch->wrptr);
			}

							/* update smsg rdptr */
			writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);
			ipc->busy[msg->channel]--;
			sem_post((ch->rxsemwait));

		}
	}

}

static int smsg_irq_handler(int irq, void *context, void *arg)
{
	struct smsg_ipc *ipc = (struct smsg_ipc *)arg;
	struct smsg *msg;
	struct smsg_channel *ch;
	uintptr_t rxpos;
	uint32_t wr;
    struct smsg_queue *msg_queue;

	if (ipc->rxirq_status()) {
		ipc->rxirq_clear();
	}
	wakeup_smsg_task_all((ipc->dis_semwait));
#if 0
    ipc_info("\n %d %d %d %d %d %d\n",readl(ipc->deq_irq_dis.rxbuf_wrptr) ,
	readl(ipc->deq_irq_dis.rxbuf_rdptr),readl(ipc->deq_priority_msg.rxbuf_wrptr),readl(ipc->deq_priority_msg.rxbuf_rdptr),
	readl(ipc->deq_normal_msg.rxbuf_wrptr),readl(ipc->deq_normal_msg.rxbuf_rdptr)); 

    if (readl(ipc->deq_priority_msg.rxbuf_wrptr) != readl(ipc->deq_priority_msg.rxbuf_rdptr)) {
		msg_queue = &(ipc->deq_priority_msg);
	} else {
		msg_queue = &(ipc->deq_normal_msg);
	}

    while (readl(msg_queue->rxbuf_wrptr) != readl(msg_queue->rxbuf_rdptr)) {
		rxpos = (readl(msg_queue->rxbuf_rdptr) & (msg_queue->rxbuf_size - 1)) *
			sizeof (struct smsg) + msg_queue->rxbuf_addr;
		msg = (struct smsg *)rxpos;

		ipc_info("channel=%d, type=%d, flag=0x%04x, value=0x%08x wrptr=%u, rdptr=%u, rxpos=0x%lx\n",
			msg->channel, msg->type, msg->flag, msg->value,readl(msg_queue->rxbuf_wrptr), readl(msg_queue->rxbuf_rdptr), rxpos);

        if(msg->type == SMSG_TYPE_DIE) {
			writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);
			continue;
		}
        if (msg->channel >= SMSG_CH_NR || msg->type >= SMSG_TYPE_NR) {
			/* invalid msg */
			ipc_info("invalid smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type, msg->flag, msg->value);

			/* update smsg rdptr */
			writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);

			continue;
		}
		ch = ipc->channels[msg->channel];
		if (!ch) {
			if (ipc->states[msg->channel] == CHAN_STATE_UNUSED &&
					msg->type == SMSG_TYPE_OPEN &&
					msg->flag == SMSG_OPEN_MAGIC) {

				ipc->states[msg->channel] = CHAN_STATE_WAITING;
			} else {
				/* drop this bad msg since channel is not opened */
				ipc_info("smsg channel %d not opened! "
					"drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
					msg->channel, msg->type, msg->flag, msg->value);
			}
			/* update smsg rdptr */
			writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);

			continue;
		}
        ipc->busy[msg->channel]++;

        if ((int)(readl(ch->wrptr) - readl(ch->rdptr)) >= SMSG_CACHE_NR) {
			/* msg cache is full, drop this msg */
			ipc_info("smsg channel %d recv cache is full! "
				"drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type, msg->flag, msg->value);
		} else {
			/* write smsg to cache */
			wr = readl(ch->wrptr) & (SMSG_CACHE_NR - 1);
			memcpy(&(ch->caches[wr]), msg, sizeof(struct smsg));
			writel(readl(ch->wrptr) + 1, ch->wrptr);
		}

        		/* update smsg rdptr */
		writel(readl(msg_queue->rxbuf_rdptr) + 1, msg_queue->rxbuf_rdptr);

		wakeup_smsg_task_all((ch->rxsemwait));

		ipc->busy[msg->channel]--;
	}
#endif
    return 0;
}

int smsg_ipc_create(uint8_t dst, struct smsg_ipc *ipc)
{
	int rval;

	if (!ipc->irq_handler) {
		ipc->irq_handler = smsg_irq_handler;
	}

	//spin_lock_init(&(ipc->txpinlock));
    smsg_ipcs[dst] = ipc;
	smsg_clear_queue(ipc,0);

    	/* explicitly call irq handler in case of missing irq on boot */
	//ipc->irq_handler(ipc->irq, ipc,ipc);

    /*register IPI irq   arg1 irq number arg2 func arg3 smsg_ipc * */        
    rval = irq_attach(ipc->irq,ipc->irq_handler,ipc);
	if (rval != 0) {
		ipc_warn("Failed to request irq %s: %d\n",
				ipc->name, ipc->irq);
		return rval;
	}

	ipc->dis_semwait = sem_open("dis_irq_sem", O_CREAT, 0, 0);
	ipc->pid = task_create("smsg_dispatch_thread", CONFIG_SMSG_THREAD_DEFPRIO ,CONFIG_SMSG_THREAD_STACKSIZE,
			(main_t)smsg_msg_dispatch_thread, (FAR char * const *)NULL);
    if ((ipc->pid < 0)) {
		ipc_info("Failed to create kthread\n");
		return rval;
	}
    return 0;
}

int smsg_ipc_destroy(uint8_t dst)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];

	//kthread_stop(ipc->thread);
    task_delete(ipc->pid);
	//free_irq(ipc->irq, ipc);
    /*nuttx no equal free_irq interface*/
	smsg_ipcs[dst] = NULL;

	return 0;
}

int smsg_ch_open(uint8_t dst, uint8_t channel,int prio, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	struct smsg mopen, mrecv;
    char semname[SEM_NAMELEN];

	int rval = 0;

	if(!ipc) {
	    return -ENODEV;
	}

	ch = malloc(sizeof(struct smsg_channel));

	if (!ch) {
		return -ENOMEM;
	}
	memset(ch,0,sizeof(struct smsg_channel));
    ipc->busy[channel] = 1;
    /**/
    (void)snprintf(semname, SEM_NAMELEN, SEM_FORMAT, channel);
    ch->rxsemwait = sem_open(semname, O_CREAT, 0, 0);
    pthread_mutex_init(&ch->rxlock, NULL);
    ipc->channels[channel] = ch;

    smsg_set(&mopen, channel, SMSG_TYPE_OPEN, SMSG_OPEN_MAGIC, 0);
    rval = smsg_send(dst,prio, &mopen, timeout);
	if (rval != 0) {
		ipc_warn("smsg_ch_open smsg send error, errno %d!\n", rval);
		ipc->states[channel] = CHAN_STATE_UNUSED;
		ipc->channels[channel] = NULL;
		ipc->busy[channel]--;
		/* guarantee that channel resource isn't used in irq handler  */
		while(ipc->busy[channel]) {
			;
		}
		free(ch);

		return rval;
	}

    	/* open msg might be got before */
	if (ipc->states[channel] == CHAN_STATE_WAITING) {
		goto open_done;
	}

	smsg_set(&mrecv, channel, 0, 0, 0);
 	rval = smsg_recv(dst, &mrecv, timeout);
	if (rval != 0) {
		ipc_info("smsg_ch_open smsg receive error, errno %d!\n", rval);
		ipc->states[channel] = CHAN_STATE_UNUSED;
		ipc->channels[channel] = NULL;
		ipc->busy[channel]--;
		// guarantee that channel resource isn't used in irq handler  
		while(ipc->busy[channel]) {
			;
		}
		free(ch);

		return rval;
	}

	if (mrecv.type != SMSG_TYPE_OPEN || mrecv.flag != SMSG_OPEN_MAGIC) {
		ipc_info("Got bad open msg on channel %d-%d\n", dst, channel);
		ipc->states[channel] = CHAN_STATE_UNUSED;
		ipc->channels[channel] = NULL;
		ipc->busy[channel]--;
		// guarantee that channel resource isn't used in irq handler  
		while(ipc->busy[channel]) {
			;
		}
		free(ch);

		return -EIO;
	}

open_done:
	ipc->states[channel] = CHAN_STATE_OPENED;
	ipc->busy[channel]--;

	return 0;
}

int smsg_ch_close(uint8_t dst, uint8_t channel, int prio, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch = ipc->channels[channel];
	struct smsg mclose;

	if (!ch) {
		return 0;
	}

	smsg_set(&mclose, channel, SMSG_TYPE_CLOSE, SMSG_CLOSE_MAGIC, 0);
	smsg_send(dst,prio, &mclose, timeout);

	ipc->states[channel] = CHAN_STATE_FREE;
    wakeup_smsg_task_all(ch->rxsemwait);
	ipc_info("ch close %x\n",ch->rxsemwait);
    	/* wait for the channel beeing unused */
	while(ipc->busy[channel]) {
		;
	}

    	/* maybe channel has been free for smsg_ch_open failed */
	if (ipc->channels[channel]){
		ipc->channels[channel] = NULL;
		/* guarantee that channel resource isn't used in irq handler */
		while(ipc->busy[channel]) {
			;
		}
		free(ch);
	}

	/* finally, update the channel state*/
	ipc->states[channel] = CHAN_STATE_UNUSED;

	return 0;
}

int smsg_send_irq(uint8_t dst, struct smsg *msg)
{
    struct smsg_ipc *ipc = smsg_ipcs[dst];
    uintptr_t txpos;
    int rval = 0;

    if(!ipc) {
		return -ENODEV;
	}
	if ((int)(readl(ipc->deq_irq_dis.txbuf_wrptr) - readl(ipc->deq_irq_dis.txbuf_rdptr)) >= ipc->deq_irq_dis.txbuf_size) {
			ipc_warn("smsg irq txbuf is full! %d %d\n",readl(ipc->deq_irq_dis.txbuf_wrptr),readl(ipc->deq_irq_dis.txbuf_rdptr));
			rval = -EBUSY;
			goto send_failed;
	}
			/* calc txpos and write smsg */
	txpos = (readl(ipc->deq_irq_dis.txbuf_wrptr) & (ipc->deq_irq_dis.txbuf_size - 1)) *
			sizeof(struct smsg) + ipc->deq_irq_dis.txbuf_addr;
	memcpy((void *)txpos, msg, sizeof(struct smsg));
/* 	ipc_info("write smsg: wrptr=%u, rdptr=%u, txpos=0x%lx %d\n",
					readl(ipc->deq_irq_dis.txbuf_wrptr),
					readl(ipc->deq_irq_dis.txbuf_rdptr), txpos,msg->value); */

	/* update wrptr */
	writel(readl(ipc->deq_irq_dis.txbuf_wrptr) + 1, ipc->deq_irq_dis.txbuf_wrptr);
	ipi_phy_trig_int_irq();

send_failed:
        return rval;
}

int smsg_send(uint8_t dst, uint8_t prio,struct smsg *msg, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	uintptr_t txpos;
	int rval = 0;
	struct smsg_queue *msg_queue;

    if(!ipc) {
	    return -ENODEV;
	}

	if (!ipc->channels[msg->channel] && msg->channel != SMSG_CH_IRQ_DIS) {
		ipc_info("channel %d not inited!\n", msg->channel);
		return -ENODEV;
	}

	if (ipc->states[msg->channel] != CHAN_STATE_OPENED &&
		msg->type != SMSG_TYPE_OPEN &&
		msg->type != SMSG_TYPE_CLOSE &&
		msg->channel != SMSG_CH_IRQ_DIS) {
		ipc_info("channel %d not opened!\n", msg->channel);
		return -EINVAL;
	}

	ipc_info("send smsg:prio=%d channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			prio,msg->channel, msg->type, msg->flag, msg->value);

    switch (prio) {
		case IPC_IRQ_MSG:
		msg_queue = &(ipc->deq_irq_dis);
		break;
		case IPC_PRIO_MSG:
		msg_queue = &(ipc->deq_priority_msg);
		break;
		case IPC_NORMAL_MSG:
		msg_queue = &(ipc->deq_normal_msg);
		break;
	}
    //spin_lock_irqsave(&(ipc->txpinlock), flags);
    ipc_info("%d smsg txbuf wr %d rd %d!\n",prio,readl(msg_queue->txbuf_wrptr),readl(msg_queue->txbuf_rdptr));
    if ((int)(readl(msg_queue->txbuf_wrptr) - readl(msg_queue->txbuf_rdptr)) >= msg_queue->txbuf_size) {
		ipc_warn("smsg txbuf is full! %d %d\n",readl(msg_queue->txbuf_wrptr),readl(msg_queue->txbuf_rdptr));
		rval = -EBUSY;
		goto send_failed;
	}

    	/* calc txpos and write smsg */
	txpos = (readl(msg_queue->txbuf_wrptr) & (msg_queue->txbuf_size - 1)) *
		sizeof(struct smsg) + msg_queue->txbuf_addr;
	memcpy((void *)txpos, msg, sizeof(struct smsg));

	ipc_info("write smsg: wrptr=%u, rdptr=%u, txpos=0x%lx\n",
			readl(msg_queue->txbuf_wrptr),
			readl(msg_queue->txbuf_rdptr), txpos);

	/* update wrptr */
	writel(readl(msg_queue->txbuf_wrptr) + 1, msg_queue->txbuf_wrptr);
	ipc->txirq_trigger();

send_failed:
	//spin_unlock_irqrestore(&(ipc->txpinlock), flags);

	return rval;
}

int smsg_recv(uint8_t dst, struct smsg *msg, int timeout)
{
    struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	uint32_t rd;
	int rval = 0;
    struct timespec cur_time;
	if(!ipc) {
	    return -ENODEV;
	}

    ipc->busy[msg->channel]++;

    ch = ipc->channels[msg->channel];

    if (!ch) {
		ipc_warn("channel %d not opened!\n", msg->channel);
		ipc->busy[msg->channel]--;
		return -ENODEV;
	}
	ipc_info("smsg_recv: dst=%d, channel=%d, timeout=%d\n",
			dst, msg->channel, timeout);
	pthread_mutex_lock(&(ch->rxlock));
	if (readl(ch->wrptr) == readl(ch->rdptr)) {
		if (timeout == 0) {
			/*if (!pthread_mutex_trylock(&(ch->rxlock))) {
				ipc_warn("recv smsg busy!\n");
				ipc->busy[msg->channel]--;

				return -EBUSY;
			}*/

			/* no wait */
			if (readl(ch->wrptr) == readl(ch->rdptr)) {
				ipc_warn("smsg rx cache is empty!\n");
				rval = -ENODATA;

				goto recv_failed;
			}
		} else if (timeout < 0) {
			//pthread_mutex_lock(&(ch->rxlock));
				/* wait forever */
			//rval = wait_event_interruptible(ch->rxwait,(readl(ch->wrptr) != readl(ch->rdptr)) || (ipc->states[msg->channel] == CHAN_STATE_FREE));
			rval = sem_wait(ch->rxsemwait);
			if (rval < 0) {
				ipc_warn("smsg_recv wait interrupted!\n");

				goto recv_failed;
			}

			if (ipc->states[msg->channel] == CHAN_STATE_FREE) {
				ipc_warn("smsg_recv smsg channel is free!\n");
				rval = -EIO;

				goto recv_failed;
			}
		} else {
			//pthread_mutex_lock(&(ch->rxlock));
			/* wait timeout */
			//rval = wait_event_interruptible_timeout(ch->rxwait,
			//	(readl(ch->wrptr) != readl(ch->rdptr)) ||
			//	(ipc->states[msg->channel] == CHAN_STATE_FREE), timeout);
			(void)clock_gettime(CLOCK_REALTIME, &cur_time);
			ipc_info("sec=%d ns=%d %ld",cur_time.tv_sec,cur_time.tv_nsec,cur_time.tv_nsec);
			cur_time.tv_sec += timeout;
			rval = sem_timedwait(ch->rxsemwait,&cur_time);
			(void)clock_gettime(CLOCK_REALTIME, &cur_time);
			ipc_info("sec=%d ns=%d %ld",cur_time.tv_sec,cur_time.tv_nsec,cur_time.tv_nsec);

			if (rval < 0) {
				ipc_warn("smsg_recv wait interrupted!\n");

				goto recv_failed;
			} else if (rval == 0) {
				ipc_warn("smsg_recv wait timeout!\n");
				rval = -ETIME;

				goto recv_failed;
			}

			if (ipc->states[msg->channel] == CHAN_STATE_FREE) {
				ipc_warn("smsg_recv smsg channel is free!\n");
				rval = -EIO;

				goto recv_failed;
			}
		}
	}
	/* read smsg from cache */
	rd = readl(ch->rdptr) & (SMSG_CACHE_NR - 1);
	memcpy(msg, &(ch->caches[rd]), sizeof(struct smsg));
	writel(readl(ch->rdptr) + 1, ch->rdptr);

	ipc_info("recv smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x wrptr=%d, rdptr=%d, rd=%d\n",
			msg->channel, msg->type, msg->flag, msg->value,readl(ch->wrptr), readl(ch->rdptr), rd);

recv_failed:
	pthread_mutex_unlock(&(ch->rxlock));
	ipc->busy[msg->channel]--;
	return rval;
}