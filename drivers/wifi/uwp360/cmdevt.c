#include <zephyr.h>
#include <logging/sys_log.h>
#include <string.h>
#include <sipc.h>
#include <sblock.h>

#include "wifi_uwp360.h"

#define RECV_BUF_SIZE 128 
static unsigned char recv_buf[RECV_BUF_SIZE];
static unsigned int recv_len;
static struct k_sem	cmd_sem;

int wifi_event_sta_connecting_handle(u8_t *pAd,struct event_sta_connecting_event *event)
{

	if (pAd == NULL || event == NULL){
		ipc_error("invalid parameter");
		return -1;
	}
	if (event->trans_header.type != EVENT_STA_CONNECTING_EVENT){
		ipc_error("event type is not EVENT_STA_CONNECTING_EVENT,we can do nothing");
		return -1;
	}

#if 0
	switch (event->type){
		case EVENT_STA_AP_CONNECTED_SUCC:
			ipc_error("sta connected to ap(%s) success,start dhcpc now",pAd->sta_config.ssid);
			netif_set_link_up(&pAd->sta_ifnet);
			dhcp_start(&pAd->sta_ifnet);
			break;
		case EVENT_STA_AP_CONNECTED_FAIL:
			if (event->errcode == EVENT_STA_CONNECT_ERRCODE_PW_ERR)
				ipc_error("sta connect to ap(%s)fail,password error",pAd->sta_config.ssid);
			else if (event->errcode == EVENT_STA_CONNECT_ERRCODE_AP_OF_OUT_RANGE)
				ipc_error("sta connect to ap(%s)fail,ap is out of range",pAd->sta_config.ssid);
			break;
		case EVENT_STA_AP_CONNECTED_LOST:
			ipc_error("sta lost connection with ap(%s)",pAd->sta_config.ssid);
			break;
		default:
			ipc_error("unknow event->type=%d",event->type);
	}
#endif

	return 0;
}

#if 0
int wifi_cmd_load_ini(u8_t *pAd)
{
	struct cmd_set_e2p data;
	int ret = 0;
	return 0;
	memset(&data,0,sizeof(data));
	//ret = wifi_read_ini(data.e2p);
	if (ret < 0){
		ipc_error("flash_read_e2p fail");
		return -1;
	}

	ret = wifi_cmd_send(CMD_SET_E2P,(char *)&data,sizeof(data),NULL,NULL);
	if (ret < 0 ){
		ipc_error("load e2p fail when call wifi_cmd_send");
		return -1;
	}

	return 0;
}
#endif

int wifi_cmd_set_sta_connect_info(u8_t *pAd,char *ssid,char *key)
{
	struct cmd_sta_set_rootap_info data;
	int ret = 0;
	if (pAd == NULL ||ssid == NULL)
	{
		ipc_error("Invalid parameter,pAd=%p,ssid=%p",pAd,ssid);
		return -1;
	}

	memset(&data,0,sizeof(data));
	memcpy(data.rootap.ssid,ssid,(strlen(ssid)< 32?strlen(ssid):32));
	if (strlen(key) <sizeof(data.rootap.key))
		memcpy(data.rootap.key,key,strlen(key));
	ret = wifi_cmd_send(CMD_STA_SET_ROOTAPINFO,
			(char *)&data,sizeof(data),NULL,NULL);
	if (ret < 0){
		ipc_error("something wrong when set connection info");
		return -1;
	}

	return 0;
}

#if LWIP_IGMP
int wifi_cmd_add_mmac(u8_t *pAd,char *mac)
{
	struct cmd_sta_set_igmp_mac data;
	int ret = 0;

	memset(&data,0,sizeof(data));
	data.igmp_mac_set.action = IGMP_MAC_ACTION_ADD;
	memcpy(data.igmp_mac_set.mac,mac,6);
	ret = wifi_cmd_send(CMD_STA_SET_IGMP_MAC,(char *)&data,sizeof(data), NULL, NULL);
	if (ret < 0){
		ipc_error("set igmp add filter fail");
		return -1;
	}else{
		ninfo("set igmp add filter success");
		return 0;
	}
}
int wifi_cmd_del_mmac(u8_t *pAd,char *mac)
{
	struct cmd_sta_set_igmp_mac data;
	int ret = 0;

	memset(&data,0,sizeof(data));
	data.igmp_mac_set.action = IGMP_MAC_ACTION_REMOVE;
	memcpy(data.igmp_mac_set.mac,mac,6);
	ret = wifi_cmd_send(CMD_STA_SET_IGMP_MAC,(char *)&data,sizeof(data), NULL, NULL);
	if (ret < 0){
		ipc_error("set igmp delete filter fail");
		return -1;
	}else {
		ninfo("set igmp delete filter success");
		return 0;
	}
}
#endif

int wifi_cmd_start_ap(u8_t *pAd)
{
	struct cmd_wifi_start data;
	int ret = 0;

	memset(&data,0,sizeof(data));
	data.mode.mode = WIFI_MODE_AP;
	wifi_get_mac((uint8_t *)data.mode.mac,1);
	ret = wifi_cmd_send(CMD_SET_WIFI_START,(char *)&data,sizeof(data),NULL,NULL);
	if (ret < 0){
		ipc_error("ap start fail");
		return -1;
	}

	return 0;
}

int wifi_cmd_start_sta(u8_t *pAd)
{
	struct cmd_wifi_start data;
	int ret = 0;
	ipc_info("--->");

	memset(&data,0,sizeof(data));
	data.mode.mode = WIFI_MODE_STA;
	wifi_get_mac((uint8_t *)data.mode.mac,0);
	ret = wifi_cmd_send(CMD_SET_WIFI_START,(char *)&data,sizeof(data),NULL,NULL);
	if (ret < 0){
		ipc_error("sta start fail");
		return -1;
	}

	return 0;
}

int wifi_cmd_start_apsta(u8_t *pAd)
{
	struct cmd_wifi_start data;
	int ret = 0;

	memset(&data,0,sizeof(data));
	data.mode.mode = WIFI_MODE_APSTA;
	wifi_get_mac((uint8_t *)data.mode.mac,1);
	ret = wifi_cmd_send(CMD_SET_WIFI_START,(char *)&data,sizeof(data),NULL,NULL);
	if (ret < 0){
		ipc_error("apsta start fail");
		return -1;
	}

	return 0;
}

int wifi_cmd_config_sta(u8_t *pAd,struct apinfo *config)
{
	int ret = 0;
	struct cmd_sta_set_rootap_info data;

	memset(&data,0,sizeof(data));
	memcpy(&data.rootap, config, sizeof(*config));
	ret = wifi_cmd_send(CMD_STA_SET_ROOTAPINFO,(char *)&data,sizeof(data),NULL,NULL);
	if (ret < 0){
		ipc_error("config sta fail");
		return -1;
	}

	return 0;
}

int wifi_cmd_config_ap(u8_t *pAd,struct apinfo *config)
{
	int ret = 0;
	struct cmd_ap_set_ap_info data;

	memset(&data,0,sizeof(data));
	memcpy(&data.apinfo, config, sizeof(*config));
	ret = wifi_cmd_send(CMD_AP_SET_APINFO,(char *)&data,sizeof(data),NULL,NULL);
	if (ret < 0){
		ipc_error("config ap fail");
		return -1;
	}

	return 0;
}

/*
 * return value is the real len sent to upper layer or -1 while error.
 */
static unsigned char npi_buf[128];
int wifi_cmd_npi_send(int ictx_id,char * t_buf,uint32_t t_len,char *r_buf,uint32_t *r_len)
{
	int ret = 0;

	/*FIXME add cmd header buffer.*/
	memcpy(npi_buf + sizeof(struct trans_hdr), t_buf, t_len);
	t_len += sizeof(struct trans_hdr);

	ret = wifi_cmd_send(CMD_SET_NPI, npi_buf, t_len,r_buf,r_len);
	if (ret < 0){
		ipc_error("npi_send_command fail");
		return -1;
	}

	if (r_buf != NULL && r_len != 0) {
		*r_len = *r_len - sizeof(struct trans_hdr);
		/* No need to copy trans_hdr */
		memcpy(r_buf, r_buf + sizeof(struct trans_hdr), *r_len);
	}

	return 0;
}

int wifi_cmd_npi_get_mac(int ictx_id,char * buf)
{
	return wifi_get_mac((uint8_t *)buf,ictx_id);
}

int wifi_cmd_handle_resp(char *data, int len)
{
	struct trans_hdr *hdr = (struct trans_hdr *)data;

	if(len > RECV_BUF_SIZE) {
		ipc_error("invalid data len %d.", len);
		return -1;
	}

	if(hdr->response == 0) {
		ipc_warn("This is invalid event?");
		return 0;
	}

	memcpy(recv_buf, data, len);
	recv_len = len;

	k_sem_give(&cmd_sem);
	return 0;
}

static unsigned int seq = 1;
int wifi_cmd_send(cmd_type type,char *data,int len,char * rbuf,int *rlen)
{
	int ret = 0;
	ipc_info("--->");

	struct trans_hdr *hdr = (struct trans_hdr *)data;

	if (type > CMD_AP_MAX || type < CMD_GET_CP_VERSION){
		ipc_error("Invalid command type %d ",type);
		return -1;
	}

	if (data != NULL && len ==0){
		ipc_error("data len Invalid,data=%p,len=%d",data,len);
		return -1;
	}

	hdr->len = len;
	hdr->type = type;
	hdr->seq = seq++;

	ret = wifi_tx_cmd(data, len);
	if (ret < 0){
		ipc_error("wifi_cmd_send fail");
		return -1;
	}

	ret = k_sem_take(&cmd_sem, 3000);
	if(ret) {
		ipc_error("wait cmd(%d) timeout.\n", type);
		return ret;
	}

	if(rbuf)
		memcpy(rbuf, recv_buf, recv_len);
	if(rlen)
		*rlen = recv_len;
	
	ipc_info("send msg success");
	return 0;
}


int wifi_cmdevt_init(void)
{
	k_sem_init(&cmd_sem, 0, 1);
	return 0;
}
