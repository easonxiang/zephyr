#include <sched.h>
#include <errno.h>
#include "ifnet.h"
#include "txrx.h"
#include "intf.h"

pid_t	thread_pid;

int wifi_get_mac(uint8_t *mac,int idx)
{
	//1 get mac address from flash.
	//2 copy to mac
	// if idx == 0,get sta mac,
	// if idx == 1,get ap mac
	mac[0]=0x00;
	mac[1]=0x0c;
	mac[2]=0x43;
	mac[3]=0x34;
	mac[4]=0x76;
	mac[5]=0x28;

	return 0;
}

int wifi_module_init(struct adapter *pAd)
{
	int ret = 0;

	_info(" --->\n");
	if (pAd == NULL){
		_err("invalid parmater pAd = NULL\n");
		return -1;
	}

	//init wifi driver part ipc
	ret =  wifi_rx_mbox_init();
	if (ret < 0){
		_err("wifi_rx_mbox_init fail\n");
		return -1;
	}

	pAd->wifi_running = 1;

	//init event handler thread
	ret =  wifi_msg_init(pAd);
	if (ret < 0){
		_err("msg_thread_init fail\n");
		return -1;
	}
	
	ret = cp_check_wifi_ready();
	if(ret) {
		_err("cp wifi is not ready.\n");
		return -1;
	}
	cp_check_sblock_running();
#if 0

	ret = wifi_cmd_load_ini(pAd);
	if (ret < 0){
		_err("load ini fail\n");
		return -1;
	}

	ret = wifi_rx_buffer_init(96);
	if (ret < 0){
		_err("rx buffer init fail\n");
		return -1;
	}
#endif
	if (pAd->opmode == WIFI_MODE_AP)
	{
		ret = wifi_cmd_start_ap(pAd);
	}else if (pAd->opmode == WIFI_MODE_STA){
		ret = wifi_cmd_start_sta(pAd);
	}else if (pAd->opmode == WIFI_MODE_APSTA){
		ret = wifi_cmd_start_apsta(pAd);
	}else{
		assert("wifi opmode is unknow\n");
		return -1;
	}
	_info("success <----\n");

	return ret;
}
//get rootap config information from flash
int wifi_get_config_from_flash(struct adapter *pAd)
{
	struct apinfo *sta_config = &pAd->sta_config;

	//add some fake code for test before flash config ready.
	memset(sta_config,0,sizeof(struct apinfo));
	strncat(sta_config->ssid,"sprd_module_ap",strlen("sprd_module_ap"));
	strncat(sta_config->key,"12345678",strlen("12345678"));
	sta_config->auth_mode = AUTH_MODE_WPA2PSK;
	sta_config->encrytype = ENCRYT_TYPE_AES;

	return 0;
}

//after up layer app set config,call this api to save to flash
int wifi_config_to_flash(struct adapter *pAd)
{

	return 0;
}

//read usr config from flash,and parse to struct adapter.
int  wifi_usr_config_init(struct adapter *pAd)
{
	int ret = 0;
	memset(pAd, 0, sizeof(struct adapter));
	pAd->opmode = WIFI_MODE_STA;// for now ,default to sta mode;
	ret = wifi_get_config_from_flash(pAd);
	if (ret < 0){
		_err("get_config_from_flash fail\n");
		return -1;
	}

	return ret ;
}

int wifi_init_thread(int argc, FAR char *argv[])
{
	struct adapter *pAd = &wifi_pAd;
	int ret = 0;
	struct tcb_s *tcb;
	struct sched_param param = {.sched_priority = 100};
	_info("---->\n");

	sched_setscheduler(getpid(), SCHED_FIFO, &param);

	sched_lock();
	tcb = sched_gettcb(getpid());
	if (!tcb)
	{
		/* This pid does not correspond to any known task */

		ret = ERROR;
	}
	else
	{
		/* Return the priority of the task */
		_err("tcb->timeslice = %d\n", (int)tcb->timeslice);
	}

	sched_unlock();
	
	ret = wifi_intf_init(pAd);
	if (ret < 0){
		_err("wifi_intf_init fail\n");
		//return -1;
	}

	/*wifi config init*/
	ret = wifi_usr_config_init(pAd);
	if (ret < 0){
		_err("wifi_usr_config_init fail\n");
		//return -1;
	}
	if (pAd->opmode == WIFI_MODE_STA){
		ret = wifi_ifnet_sta_init(pAd);
	}
	else if (pAd->opmode == WIFI_MODE_AP)
		ret = wifi_ifnet_ap_init(pAd);
	else if (pAd->opmode == WIFI_MODE_APSTA){
		ret = wifi_ifnet_ap_init(pAd);
		ret += wifi_ifnet_sta_init(pAd);
	}
	if(ret < 0){
		_err("ifnet_init fail\n");
		//return -2;
	}
	ret = wifi_module_init(pAd);
	if(ret < 0){
		_err("sc2355a module init fail\n");
		//return -1;
	}
	if (pAd->opmode == WIFI_MODE_AP||pAd->opmode == WIFI_MODE_APSTA)
		ret = wifi_cmd_config_ap(pAd,&pAd->ap_config);

	if (pAd->opmode == WIFI_MODE_APSTA || pAd->opmode == WIFI_MODE_STA)
		//ret += wifi_cmd_config_sta(pAd,&pAd->sta_config);
	if (ret < 0){
		_err("config ap or sta fail\n");
		//return -1;
	}
	//from now ,wifi should be runing,and well configed. we just need to wait connected event.
    _info("success <-----\n");
	//return ret;
	while(1) {
		sleep(10000);
	}

}

//up layer call this function to init wifi part.
int wifi_module_up(void)
{
	thread_pid = task_create("wifi_module_init", 100 ,1024,
			(main_t)wifi_init_thread, NULL);

	if (thread_pid == ERROR)
		_err("create wifi init thread fail\n");

	return 0;
}


