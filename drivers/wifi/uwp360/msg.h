#ifndef __MSG_H__
#define __MSG_H__

#include <zephyr.h>

#define E2P_SIZE  1024
#define MSG_STATUS_INIT 0
#define MSG_STATUS_MAILBOX_POST 1
#define MSG_STATUS_IPC_SEND_FAIL 2
#define MSG_STATUS_CP_RESPONSE_SUCC 3
#define MSG_STATUS_WAITING_RESPONSE 4
#define MSG_STATUS_WAITING_ERR 5
struct msg_queue {
	struct msg *pre;
	struct msg *next;
	struct k_sem lock;
	int cnt;
};

struct msg {
	volatile struct msg *pre;
	volatile struct msg *next;
	volatile u8_t type;
	volatile u8_t seq ;
	volatile u8_t need_ack;
	volatile struct k_sem complete;
	volatile void *data;//usually point to struct event,
	volatile int data_len;
	volatile struct pbuf *p;//maybe data point to a pbuf.
	volatile int status;
};

/* this structure is shared by cmd and event. */
struct trans_hdr{
    u8_t type;// event_type
    u8_t seq; //if cp response ap event,should set it the same as ap event seq,or else should set to 0xff.
    u8_t response;//to identify the event is a cp response ,or cp report.if it is a response ,set to 1,if is a report ,set to 0
    char status;//if response by cp,indicate cp handle fail or success,0 for success,other for fail.
    u16_t len;//msg len, incluld the event header
    char data[0];//the start of msg data.
}__attribute__ ((packed));

/*
 * NOTE: CMD_TYPE_END should NOT excced EVENT_TYPE_BEGIN
 */
enum cmd_type {
	CMD_TYPE_BEGIN = 0x00,
	//common
	CMD_GET_CP_VERSION, //get cp version number
	CMD_GET_CP_RUN_MODE,//get which mode cp is running
	CMD_SET_E2P,
	CMD_SET_WIFI_START, //start a wifi mode
	CMD_SET_WIFI_STOP,//stop wifi
	CMD_SET_NPI,
	CMD_COMMON_MAX,
	//sta
	CMD_STA_GET_TR_BYTECNT,//get how many bytes sta has tx/rx
	CMD_STA_GET_STA_RSSI,//get sta's RSSI
	CMD_STA_SET_ROOTAPINFO,//set connecting ap information for sta
	CMD_STA_SET_DISCONNECT,//disconnect sta with ap
	CMD_STA_SET_IGMP_MAC,//set multicast mac to wifi
	CMD_STA_GET_TX_RATE, //get sta conrrent tx rate
	CMD_STA_GET_APLIST, // set wifi scan,and wait for scan table(ap list),when receive this command ,cp will start scan.when scan done,response this command with scan table
	CMD_STA_GET_STATUS,// get sta connecting status,if connected with ap,return the connected ap information
	CMD_STA_MAX,

	//ap
	CMD_AP_GET_TR_BYTECNT,//get softap tx /rx bytes
	CMD_AP_SET_STA_DISCONNECT,//disconnect sta connected with us
	CMD_AP_SET_IGMP_MAC,//set multicast mac to wifi
	CMD_AP_SET_AP_CHANNEL,//set softap's channel
	CMD_AP_SET_APINFO,//set softap running information,such as ssid,auth mode,etc
	CMD_AP_MAX,

	CMD_TYPE_END,

};

enum event_type {
	EVENT_TYPE_BEGIN = 0x80,
	//common
	EVENT_COMMON_MAX,
	//sta
	EVENT_STA_CONNECTING_EVENT,//Event send by cp,to report sta connecting status with rootap
	EVENT_STA_MAX,//max of sta
	//ap
	EVENT_AP_STA_CONNECTING_EVENT,//event send by cp to report sta disconnected with us
	EVENT_AP_MAX,//max of ap

	EVENT_TYPE_END,
};

typedef enum event_type event_type;
typedef enum cmd_type cmd_type;
struct wifi_mode{
    char mode;//
	char mac[6];
}__attribute__ ((packed));


#define AUTH_MODE_OPEN  1
#define AUTH_MODE_SHARE 2
#define AUTH_MODE_WPAPSK 3
#define AUTH_MODE_WPA2PSK 4
#define AUTH_MODE_PSKMIX 5 //WPAPSK/WPA2PSK mix
#define ENCRYT_TYPE_NONE    1
#define ENCRYT_TYPE_WEP     2
#define ENCRYT_TYPE_TKIP    3
#define ENCRYT_TYPE_AES     4
#define ENCRYT_TYPE_TKIPAES 5  //TKIP/AES mix
struct apinfo{
    char ssid[32];
    char bssid[6];
    char key[128];
    char auth_mode;
    char encrytype;
    char channel;
    char phymode;
}__attribute__ ((packed));

//for get scan table
struct ap_list_entry{
    char ssid[32];
    char bssid[6];
    char auth_mode;
    char encrytype;
    char channel;
    char phymode;
}__attribute__ ((packed));

struct aplist {
    int apcnt;//how much ap_list_entry in the entry
    struct ap_list_entry entry[0];//start of ap list entry
}__attribute__ ((packed));

#define IGMP_MAC_ACTION_ADD     1
#define IGMP_MAC_ACTION_REMOVE  2
struct igmp_mac_set {
    int action; //action of tihs event,add or remove multicast mac
    char mac[6];
}__attribute__ ((packed));

struct tr_bytecnt{
    unsigned int tx_bytes;
    unsigned int rx_bytes;
}__attribute__ ((packed));

struct tx_rate{
    int rate;
}__attribute__ ((packed));

struct rssi{
    int ss;//speical streams,indicate how many entry used in rssi[4]
    char rssi[4];
}__attribute__ ((packed));


/************************************************
	structure definition for cmd
************************************************/
//get cp version
struct cmd_get_cp_version{
    struct trans_hdr trans_header; //common header for all event.
    char main_version;
    char sub_version;
    char patch_version;
}__attribute__ ((packed));

struct cmd_set_e2p{
    struct trans_hdr trans_header;
    char e2p[E2P_SIZE];
}__attribute__ ((packed));

struct cmd_wifi_start{
    struct trans_hdr trans_header;
    struct wifi_mode mode;
}__attribute__ ((packed));
struct cmd_wifi_stop{
    struct trans_hdr trans_header;
    struct wifi_mode mode;
}__attribute__ ((packed));

struct  cmd_get_wifi_run_mode {
    struct trans_hdr trans_header;
    struct wifi_mode mode;
}__attribute__ ((packed));
struct cmd_sta_set_rootap_info{
    struct trans_hdr trans_header;
    struct apinfo rootap;
}__attribute__ ((packed));
struct cmd_sta_set_disconnect{
    struct trans_hdr trans_header;
    char status;//for cp return status. ap set it to 0
}__attribute__ ((packed));

#define STA_CONNECT_STATUS_CONNECTED 1
#define STA_CONNECT_STATUS_DISCONNECT 2
struct cmd_sta_get_connect_status{
    struct trans_hdr trans_header;
    char status;//indecate sta connecting status, 1 for connected with rootap,2 for not connected yet
    struct apinfo rootap; //if status == 1, rootap is the connected ap information.
}__attribute__ ((packed));

struct cmd_sta_set_igmp_mac{
    struct trans_hdr trans_header;
    struct igmp_mac_set data;
}__attribute__ ((packed));
struct cmd_sta_get_tx_rate{
    struct trans_hdr trans_header;
    struct tx_rate  data;
}__attribute__ ((packed));
struct cmd_sta_get_aplist{
    struct trans_hdr trans_header;
    struct aplist list;
}__attribute__ ((packed));
struct cmd_sta_get_tr_bytecnt{
    struct trans_hdr trans_header;
    struct tr_bytecnt conuter;
}__attribute__ ((packed));

struct cmd_sta_get_rssi{
    struct trans_hdr trans_header;
    struct rssi rssi;
}__attribute__ ((packed));
struct cmd_ap_set_ap_info{
    struct trans_hdr trans_header;
    struct apinfo apinfo;
}__attribute__ ((packed));
struct cmd_ap_set_disconnect_sta{
    struct trans_hdr trans_header;
    char all;//if set to 1,disconnect all sta connect to ap,if set to 0,just disconnect the sta indentified by mac[6]
    char mac[6];
}__attribute__ ((packed));
struct cmd_ap_set_igmp_mac{
    struct trans_hdr trans_header;
    struct igmp_mac_set data;
}__attribute__ ((packed));
struct cmd_ap_get_tr_bytecnt{
    struct trans_hdr trans_header;
    struct tr_bytecnt conuter;
}__attribute__ ((packed));
struct cmd_ap_set_channel{
    struct trans_hdr trans_header;
    char chanel;
}__attribute__ ((packed));

/************************************************
	structure definition for event
************************************************/
#define EVENT_STA_AP_CONNECTED_SUCC  1
#define EVENT_STA_AP_CONNECTED_FAIL  2
#define EVENT_STA_AP_CONNECTED_LOST  3

#define EVENT_STA_CONNECT_ERRCODE_PW_ERR 1
#define EVENT_STA_CONNECT_ERRCODE_AP_OF_OUT_RANGE 2
struct event_sta_connecting_event{
    struct trans_hdr trans_header;
    int type;
    int errcode;
}__attribute__ ((packed));

#define EVENT_AP_STA_CONNECTED    1
#define EVENT_AP_STA_DISCONNECTED 2
struct event_ap_sta_connecting_event{
    struct trans_hdr trans_header;
    int type;
    char mac[6];
}__attribute__ ((packed));

struct adapter;
int wifi_msg_init(struct adapter *pAd);
int wifi_msg_send(struct msg *msg,char ** rbuf,int *rlen);
struct msg *wifi_msg_alloc(int data_length);
int wifi_msg_fill(struct msg *msg,cmd_type type,char * data,int len);

#endif

