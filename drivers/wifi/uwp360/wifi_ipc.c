#include <zephyr.h>
#include <logging/sys_log.h>
#include <string.h>
#include <sipc.h>
#include <sblock.h>

extern int wifi_rx_event_callback(int channel,void *data,int len);
extern int wifi_rx_data_callback(int channel,void *data,int len);

int create_wifi_channel(int ch)
{
    int ret=0;
    switch (ch) {
        case SMSG_CH_WIFI_CTRL:
            ret=sblock_create(0, ch,CTRLPATH_TX_BLOCK_NUM, CTRLPATH_TX_BLOCK_SIZE,
            CTRLPATH_RX_BLOCK_NUM, CTRLPATH_RX_BLOCK_SIZE);
        break;
        case SMSG_CH_WIFI_DATA_NOR:
            ret=sblock_create(0, ch,DATAPATH_NOR_TX_BLOCK_NUM, DATAPATH_NOR_TX_BLOCK_SIZE,
		    DATAPATH_NOR_RX_BLOCK_NUM, DATAPATH_NOR_RX_BLOCK_SIZE);
        break;
        case SMSG_CH_WIFI_DATA_SPEC:
            ret=sblock_create(0, ch,DATAPATH_SPEC_TX_BLOCK_NUM, DATAPATH_SPEC_TX_BLOCK_SIZE,
		    DATAPATH_SPEC_RX_BLOCK_NUM, DATAPATH_SPEC_RX_BLOCK_SIZE);
        break;
        default:
            ipc_info("channel is error: %d\n",ch);
            ret = -1;
        break;
    }

    return ret;
}

int wifi_ipc_send(int ch,int prio,void *data,int len, int offset)
{
    int ret;
    struct sblock blk;
    ret = sblock_get(0, ch, &blk,0);
    if (ret != 0) {
        ipc_info("get block error: %d\n",ch);
        return -1;
    }
    ipc_info("wifi_send ch=%d prio=%d len=%d\n",ch,prio,len);
    memcpy(blk.addr+BLOCK_HEADROOM_SIZE+offset,data,len);

    blk.length = len + BLOCK_HEADROOM_SIZE;
    ret = sblock_send(0, ch,prio,&blk);

    return ret;
}

struct sblock blk;
int wifi_ipc_recv(int ch, u8_t **data,int *len, int offset)
{
	int ret;
	u8_t *recvdata;

	ret = sblock_receive(0, ch, &blk, 0);
	if (ret != 0) {
		ipc_info("sblock recv error\n");
	}
	*data = (u8_t *)blk.addr;
	*len = blk.length;
	recvdata = (u8_t *)blk.addr;
	ipc_info("sblock recv 0x%x %x %x %x %x %x %x",recvdata[0],recvdata[1],recvdata[2],
			recvdata[3],recvdata[4],recvdata[5],recvdata[6]);
	sblock_release(0, ch, &blk);
	return ret;
}

extern void wifi_txrx_notify(void);
extern int wifi_rx_event(int channel,void *data,int len);
static int wifi_ipc_recv_callback(int channel,void *data,int len)
{
	wifi_txrx_notify();

	return 0;
}

int wifi_ipc_init(void)
{
	int ret = 0;

	ret = create_wifi_channel(SMSG_CH_WIFI_CTRL);
	if (ret < 0){
		ipc_error("creater wifi event channel fail\n");
		return ret;
	}
	ret = sblock_register_callback(SMSG_CH_WIFI_CTRL,wifi_ipc_recv_callback);
	if (ret < 0){
		ipc_error("register event rx callback fail\n");
		return ret;
	}
	
#if 0
	ret = create_wifi_channel(SMSG_CH_WIFI_DATA_NOR);
	if (ret < 0){
		ipc_error("creater wifi event channel fail\n");
		return -1;
	}
	/*
	ret = sblock_register_callback(SMSG_CH_WIFI_DATA_NOR,wifi_rx_data_callback);
	if (ret < 0){
		ipc_error("register data rx callback fail\n");
		return -1;
	}
	*/
	
	ret = create_wifi_channel(SMSG_CH_WIFI_DATA_SPEC);
	if (ret < 0){
		ipc_error("creater wifi event channel fail\n");
		return -1;
	}
	/* SMSG_CH_WIFI_DATA_SPEC channel is not used currently. */
		/*
	ret = sblock_register_callback(SMSG_CH_WIFI_DATA_SPEC,wifi_rx_data_callback);
	if (ret < 0){
		ipc_error("register special data rx callback fail\n");
		return -1;
	}
	*/
#endif

	return 0;	
}
