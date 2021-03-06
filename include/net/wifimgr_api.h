/*
 * @file
 * @brief WiFi manager APIs for the external caller
 */

/*
 * Copyright (c) 2018, Unisoc Communications Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WIFIMGR_API_H_
#define _WIFIMGR_API_H_

#define WIFIMGR_IFACE_NAME_STA	"sta"
#define WIFIMGR_IFACE_NAME_AP	"ap"

struct wifimgr_ctrl_ops {
	int (*set_conf) (char *iface_name, char *ssid, char *bssid,
			 char *passphrase, unsigned char band,
			 unsigned char channel);
	int (*get_conf) (char *iface_name);
	int (*get_status) (char *iface_name);
	int (*open) (char *iface_name);
	int (*close) (char *iface_name);
	int (*scan) (void);
	int (*connect) (void);
	int (*disconnect) (void);
	int (*start_ap) (void);
	int (*stop_ap) (void);
	int (*del_station) (char *mac);
};

struct wifimgr_ctrl_cbs {
	void (*get_conf_cb) (char *iface_name, char *ssid, char *bssid,
			     char *passphrase, unsigned char band,
			     unsigned char channel);
	void (*get_status_cb) (char *iface_name, unsigned char status,
			       char *own_mac, signed char signal);
	void (*notify_scan_res) (char *ssid, char *bssid, unsigned char band,
				 unsigned char channel, signed char signal);
	void (*notify_scan_done) (unsigned char result);
	void (*notify_connect) (unsigned char result);
	void (*notify_disconnect) (unsigned char reason);
	void (*notify_scan_timeout) (void);
	void (*notify_connect_timeout) (void);
	void (*notify_disconnect_timeout) (void);
	void (*notify_new_station) (unsigned char status, unsigned char *mac);
	void (*notify_del_station_timeout) (void);
};

const
struct wifimgr_ctrl_ops *wifimgr_get_ctrl_ops(struct wifimgr_ctrl_cbs *cbs);

#endif
