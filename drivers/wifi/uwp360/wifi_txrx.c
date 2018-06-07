#include <zephyr.h>
#include <logging/sys_log.h>
#include <string.h>
#include <sipc.h>
#include <sblock.h>

#include "wifi_uwp360.h"

int wifi_rx_complete_handle(void *data,int len)
{
	return 0;
}

int wifi_tx_complete_handle(void * data,int len)
{
	return 0;
}

//event rx call back
int wifi_rx_event(int channel,void *data,int len)
{
	ipc_info("--->\n");

	if (channel  != SMSG_CH_WIFI_CTRL){
		ipc_error("invalid channel for event rx callback\n");
		return -1;
	}
	if (data == NULL || len == 0){
		ipc_error("invalid data(%p)/len(%d)\n",data,len);
		return -1;
	}

	return 0;
}

int wifi_tx_cmd(void *data, int len)
{
	int ret;

	ret = wifi_ipc_send(SMSG_CH_WIFI_CTRL,QUEUE_PRIO_HIGH,
			data,len,WIFI_CTRL_MSG_OFFSET);

	return ret;
}

int wifi_rx_data(int channel,void *data,int len)
{
	struct sprdwl_common_hdr *common_hdr = (struct sprdwl_common_hdr *)data;

	if (channel > SMSG_CH_WIFI_DATA_SPEC || data == NULL ||len ==0){
		ipc_error("invalid parameter,channel=%d data=%p,len=%d\n",channel,data,len);
		return -1;
	}
	if (channel == SMSG_CH_WIFI_DATA_SPEC)
		ipc_info("rx spicial data\n");
	else if (channel ==SMSG_CH_WIFI_DATA_NOR)
		ipc_info("rx data\n");
	else{
		ipc_error("should not be here[channel = %d]\n",channel);
		return -1;
	}

	switch (common_hdr->type){
		case SPRDWL_TYPE_DATA_PCIE_ADDR:
			if (common_hdr->direction_ind) /* direction_ind=1, rx data */
				wifi_rx_complete_handle(data,len);
			else
				wifi_tx_complete_handle(data,len); /* direction_ind=0, tx complete */
			break;
		case SPRDWL_TYPE_DATA:
		case SPRDWL_TYPE_DATA_SPECIAL:
			break;
		default :
			ipc_info("unknown type :%d\n",common_hdr->type);			
	}

	return 0;

}

#define TXRX_STACK_SIZE		(1024)
K_THREAD_STACK_MEMBER(txrx_stack, TXRX_STACK_SIZE);
static struct k_thread txrx_thread_data;

static struct k_sem	data_sem;
extern int wifi_ipc_recv(int ch, u8_t **data,int *len, int offset);
extern int wifi_cmd_handle_resp(char *data, int len);
static void txrx_thread(void)
{
	int ret;
	u8_t *addr;
	int len;

	while (1) {
		SYS_LOG_INF("wait for data.");
		k_sem_take(&data_sem, K_FOREVER);

		ret = wifi_ipc_recv(12, &addr, &len, 0);
		if(ret != 0) {
			SYS_LOG_WRN("Recieve none data.");
			continue;
		}

		SYS_LOG_INF("Recieve data %p len %i", addr, len);

		/* process data here */
		wifi_cmd_handle_resp(addr, len);

	}
}

void wifi_txrx_notify(void)
{
	k_sem_give(&data_sem);
}

int wifi_txrx_init(void)
{
	k_sem_init(&data_sem, 0, 1);

	k_thread_create(&txrx_thread_data, txrx_stack,
			TXRX_STACK_SIZE,
			(k_thread_entry_t)txrx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(7),
			0, K_NO_WAIT);

	return 0;
}
