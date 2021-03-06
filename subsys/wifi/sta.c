/*
 * @file
 * @brief Station mode handling
 */

/*
 * Copyright (c) 2018, Unisoc Communications Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wifimgr.h"
#include "led.h"

int wifi_manager_get_sta_config(void *handle)
{
	struct wifi_manager *mgr = (struct wifi_manager *)handle;
	struct wifimgr_config *conf = &mgr->sta_conf;
	struct wifimgr_ctrl_cbs *cbs = wifimgr_get_ctrl_cbs();

	syslog(LOG_INFO, "STA Config\n");
	if (strlen(conf->ssid))
		syslog(LOG_INFO, "SSID:\t\t%s\n", conf->ssid);

	if (!is_zero_ether_addr(conf->bssid))
		syslog(LOG_INFO, "BSSID:\t\t%02x:%02x:%02x:%02x:%02x:%02x\n",
		       conf->bssid[0], conf->bssid[1], conf->bssid[2],
		       conf->bssid[3], conf->bssid[4], conf->bssid[5]);

	if (conf->channel)
		syslog(LOG_INFO, "Channel:\t%d\n", conf->channel);

	if (strlen(conf->passphrase))
		syslog(LOG_INFO, "Passphrase:\t%s\n", conf->passphrase);
	fflush(stdout);

	/* Notify the external caller */
	if (cbs && cbs->get_conf_cb)
		cbs->get_conf_cb(WIFIMGR_IFACE_NAME_STA,
				 strlen(conf->ssid) ? conf->ssid : NULL,
				 is_zero_ether_addr(conf->bssid) ? NULL :
				 conf->bssid,
				 strlen(conf->passphrase) ? conf->
				 passphrase : NULL, conf->band, conf->channel);

	return 0;
}

int wifi_manager_set_sta_config(void *handle)
{
	struct wifimgr_config *conf = (struct wifimgr_config *)handle;

	syslog(LOG_INFO, "Setting STA ...\n");
	syslog(LOG_INFO, "SSID:\t\t%s\n", conf->ssid);

	if (strlen(conf->passphrase))
		syslog(LOG_INFO, "Passphrase:\t%s\n", conf->passphrase);
	fflush(stdout);

	conf->found = false;

	return 0;
}

int wifi_manager_get_sta_status(void *handle)
{
	struct wifi_manager *mgr = (struct wifi_manager *)handle;
	struct wifimgr_state_machine *sm = &mgr->sta_sm;
	struct wifimgr_status *sts = &mgr->sta_sts;
	struct wifimgr_ctrl_cbs *cbs = wifimgr_get_ctrl_cbs();
	int ret = 0;

	syslog(LOG_INFO, "STA Status:\t%s\n",
	       sta_sts2str(sm_sta_query(sm)));

	if (is_zero_ether_addr(sts->own_mac))
		ret = wifi_drv_iface_get_mac(mgr->sta_iface, sts->own_mac);
	syslog(LOG_INFO, "Own MAC:\t%02x:%02x:%02x:%02x:%02x:%02x\n",
	       sts->own_mac[0], sts->own_mac[1], sts->own_mac[2],
	       sts->own_mac[3], sts->own_mac[4], sts->own_mac[5]);

	if (sm_sta_connected(sm) == true) {
		ret = wifi_drv_iface_get_station(mgr->sta_iface, &sts->rssi);
		syslog(LOG_INFO, "Signal:\t\t%d\n", sts->rssi);
	}
	fflush(stdout);

	/* Notify the external caller */
	if (cbs && cbs->get_status_cb)
		cbs->get_status_cb(WIFIMGR_IFACE_NAME_STA, sm_sta_query(sm),
				   sts->own_mac, sts->rssi);

	return ret;
}

static int wifi_manager_disconnect_event(void *arg)
{
	struct wifimgr_evt_disconnect *disc =
	    (struct wifimgr_evt_disconnect *)arg;
	struct wifi_manager *mgr =
	    container_of(disc, struct wifi_manager, evt_disc);
	struct net_if *iface = (struct net_if *)mgr->sta_iface;
	struct wifimgr_ctrl_cbs *cbs = wifimgr_get_ctrl_cbs();
	int ret = 0;

	syslog(LOG_INFO, "disconnect, reason_code %d!\n", disc->reason_code);
	fflush(stdout);

	command_processor_unregister_sender(&mgr->prcs, WIFIMGR_CMD_DISCONNECT);

	if (iface)
		wifimgr_dhcp_stop(iface);

	/* Notify the external caller */
	if (cbs && cbs->notify_disconnect)
		cbs->notify_disconnect(disc->reason_code);

	return ret;
}

int wifi_manager_disconnect(void *handle)
{
	struct wifi_manager *mgr = (struct wifi_manager *)handle;
	int ret;

	event_listener_add_receiver(&mgr->lsnr, WIFIMGR_EVT_DISCONNECT,
				    true, wifi_manager_disconnect_event,
				    &mgr->evt_disc);

	ret = wifi_drv_iface_disconnect(mgr->sta_iface);
	if (ret)
		event_listener_remove_receiver(&mgr->lsnr,
					       WIFIMGR_EVT_DISCONNECT);

	return ret;
}

static int wifi_manager_connect_event(void *arg)
{
	struct wifimgr_evt_connect *conn = (struct wifimgr_evt_connect *)arg;
	struct wifi_manager *mgr =
	    container_of(conn, struct wifi_manager, evt_conn);
	struct net_if *iface = (struct net_if *)mgr->sta_iface;
	struct wifimgr_ctrl_cbs *cbs = wifimgr_get_ctrl_cbs();
	int ret = conn->status;

	if (!ret) {
		syslog(LOG_INFO, "connect successfully!\n");

		/* Register disconnect event here due to AP deauth */
		event_listener_add_receiver(&mgr->lsnr, WIFIMGR_EVT_DISCONNECT,
					    true, wifi_manager_disconnect_event,
					    &mgr->evt_disc);

		command_processor_register_sender(&mgr->prcs,
						  WIFIMGR_CMD_DISCONNECT,
						  wifi_manager_disconnect, mgr);

		if (iface)
			wifimgr_dhcp_start(iface);

	} else {
		syslog(LOG_ERR, "failed to connect!\n");
	}
	fflush(stdout);

	/* Notify the external caller */
	if (cbs && cbs->notify_connect)
		cbs->notify_connect(conn->status);

	return ret;
}

int wifi_manager_connect(void *handle)
{
	struct wifi_manager *mgr = (struct wifi_manager *)handle;
	struct wifimgr_config *conf = &mgr->sta_conf;
	int ret;

	if (!strlen(conf->ssid)) {
		syslog(LOG_ERR, "no AP specified!\n");
		return -EINVAL;
	}

	syslog(LOG_INFO, "Connecting to %s\n", conf->ssid);

	ret = event_listener_add_receiver(&mgr->lsnr, WIFIMGR_EVT_CONNECT,
					  true, wifi_manager_connect_event,
					  &mgr->evt_conn);
	if (ret)
		return ret;

	ret = wifi_drv_iface_connect(mgr->sta_iface, conf->ssid,
				     conf->passphrase);

	if (ret)
		event_listener_remove_receiver(&mgr->lsnr, WIFIMGR_EVT_CONNECT);

	return ret;
}

static int wifi_manager_scan_result(void *arg)
{
	struct wifimgr_evt_scan_result *scan_res =
	    (struct wifimgr_evt_scan_result *)arg;
	struct wifi_manager *mgr =
	    container_of(scan_res, struct wifi_manager, evt_scan_res);
	struct wifimgr_ctrl_cbs *cbs = wifimgr_get_ctrl_cbs();

	syslog(LOG_INFO, "\t%-32s", scan_res->ssid);
	syslog(LOG_INFO, "\t%02x:%02x:%02x:%02x:%02x:%02x\t%u\t%d\n",
	       scan_res->bssid[0], scan_res->bssid[1], scan_res->bssid[2],
	       scan_res->bssid[3], scan_res->bssid[4], scan_res->bssid[5],
	       scan_res->channel, scan_res->rssi);
	fflush(stdout);

	/* Found specified AP */
	if (!strcmp(scan_res->ssid, mgr->sta_conf.ssid))
		if (!strncmp
		    (scan_res->bssid, mgr->sta_conf.bssid, WIFIMGR_ETH_ALEN)
		    || is_zero_ether_addr(mgr->sta_conf.bssid))
			mgr->sta_conf.found = true;

	/* Notify the external caller */
	if (cbs && cbs->notify_scan_res)
		cbs->notify_scan_res(strlen(scan_res->ssid) ? scan_res->
				     ssid : NULL,
				     is_zero_ether_addr(scan_res->
							bssid) ? NULL :
				     scan_res->bssid, scan_res->band,
				     scan_res->channel, scan_res->rssi);

	return 0;
}

static int wifi_manager_scan_done(void *arg)
{
	struct wifimgr_evt_scan_done *scan_done =
	    (struct wifimgr_evt_scan_done *)arg;
	struct wifi_manager *mgr =
	    container_of(scan_done, struct wifi_manager, evt_scan_done);
	struct wifimgr_ctrl_cbs *cbs = wifimgr_get_ctrl_cbs();
	int ret = scan_done->result;

	event_listener_remove_receiver(&mgr->lsnr, WIFIMGR_EVT_SCAN_RESULT);

	if (!ret)
		syslog(LOG_INFO, "scan done!\n");
	else
		syslog(LOG_ERR, "scan abort!\n");
	fflush(stdout);

	/* Notify the external caller */
	if (cbs && cbs->notify_scan_done)
		cbs->notify_scan_done(mgr->sta_conf.found);

	return ret;
}

int wifi_manager_scan(void *handle)
{
	struct wifi_manager *mgr = (struct wifi_manager *)handle;
	struct wifimgr_config *conf = &mgr->sta_conf;
	int ret;

	ret = event_listener_add_receiver(&mgr->lsnr, WIFIMGR_EVT_SCAN_RESULT,
					  false, wifi_manager_scan_result,
					  &mgr->evt_scan_res);
	if (ret)
		return ret;

	ret = event_listener_add_receiver(&mgr->lsnr, WIFIMGR_EVT_SCAN_DONE,
					  true, wifi_manager_scan_done,
					  &mgr->evt_scan_done);
	if (ret) {
		event_listener_remove_receiver(&mgr->lsnr,
					       WIFIMGR_EVT_SCAN_RESULT);
		return ret;
	}

	ret = wifi_drv_iface_scan(mgr->sta_iface, conf->band, conf->channel);
	if (ret) {
		event_listener_remove_receiver(&mgr->lsnr,
					       WIFIMGR_EVT_SCAN_RESULT);
		event_listener_remove_receiver(&mgr->lsnr,
					       WIFIMGR_EVT_SCAN_DONE);
	}

	return ret;
}

int wifi_manager_open_station(void *handle)
{
	struct wifi_manager *mgr = (struct wifi_manager *)handle;
	int ret;

	ret = wifi_drv_iface_open_station(mgr->sta_iface);
	if (ret) {
		syslog(LOG_ERR, "failed to open STA!\n");
		return ret;
	}

	command_processor_unregister_sender(&mgr->prcs, WIFIMGR_CMD_OPEN_STA);

	command_processor_register_sender(&mgr->prcs, WIFIMGR_CMD_CLOSE_STA,
					  wifi_manager_close_station, mgr);
	command_processor_register_sender(&mgr->prcs, WIFIMGR_CMD_SCAN,
					  wifi_manager_scan, mgr);
	command_processor_register_sender(&mgr->prcs, WIFIMGR_CMD_CONNECT,
					  wifi_manager_connect, mgr);

	return ret;
}

int wifi_manager_close_station(void *handle)
{
	struct wifi_manager *mgr = (struct wifi_manager *)handle;
	int ret;

	ret = wifi_drv_iface_close_station(mgr->sta_iface);
	if (ret) {
		syslog(LOG_ERR, "failed to close STA!\n");
		return ret;
	}

	command_processor_unregister_sender(&mgr->prcs, WIFIMGR_CMD_CLOSE_STA);
	command_processor_unregister_sender(&mgr->prcs, WIFIMGR_CMD_DISCONNECT);
	command_processor_unregister_sender(&mgr->prcs, WIFIMGR_CMD_CONNECT);
	command_processor_unregister_sender(&mgr->prcs, WIFIMGR_CMD_SCAN);

	command_processor_register_sender(&mgr->prcs, WIFIMGR_CMD_OPEN_STA,
					  wifi_manager_open_station, mgr);

	return ret;
}
