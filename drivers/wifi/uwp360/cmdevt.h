#ifndef __CMDEVT_H__
#define __CMDEVT_H__
//int wifi_cmd_load_ini(u8_t *pAd);
int wifi_cmd_set_sta_connect_info(u8_t *pAd,char *ssid,char *key);
int wifi_cmd_start_apsta(u8_t *pAd);
int wifi_cmd_start_sta(u8_t *pAd);
int wifi_cmd_start_ap(u8_t *pAd);
int wifi_cmd_config_sta(u8_t *pAd,struct apinfo *config);
int wifi_cmd_config_ap(u8_t *pAd,struct apinfo *config);
int wifi_cmd_npi_send(int ictx_id,char * t_buf,uint32_t t_len,char *r_buf,uint32_t *r_len);
int wifi_cmd_npi_get_mac(int ictx_id,char * buf);

extern int wifi_cmd_send(cmd_type type,char *data,int len,char * rbuf,int *rlen);
extern int wifi_cmdevt_init(void);

#endif

