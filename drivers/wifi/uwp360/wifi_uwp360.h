/*
 * (C) Copyright 2017
 * Dong Xiang, <dong.xiang@spreadtrum.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __WIFI_UWP360_H_
#define __WIFI_UWP360_H_

#include <zephyr.h>
#include "wifi_txrx.h"
#include "msg.h"
#include "cmdevt.h"

#define WIFI_MODE_STA       1
#define WIFI_MODE_AP        2
#define WIFI_MODE_APSTA     3
#define WIFI_MODE_MONITOR   4
struct adapter {
//	struct netif sta_ifnet;
//	struct netif ap_ifnet;
	struct apinfo sta_config;
	struct apinfo ap_config;
	int sta_status;
	int opmode;//ap ,sta
	int wifi_running; //if wifi is ready to work.
	unsigned long tx_byte_cnt;
	unsigned long rx_byte_cnt;
	//struct pbuf tx_queue; do we need tx queue to buffer packet when ipc cann't handle
	struct msg_queue wait_ack_queue;
};

extern struct adapter wifi_pAd;

int wifi_ifnet_sta_init(struct adapter *pAd);
int wifi_ifnet_ap_init(struct adapter *pAd);
struct netif *wifi_ifnet_get_interface(struct adapter *pAd,int ctx_id);
extern int wifi_ipc_send(int ch,int prio,void *data,int len, int offset);
extern int wifi_get_mac(u8_t *mac,int idx);
extern int wifi_ipc_init(void);

#endif /* __WIFI_UWP360_H_ */
