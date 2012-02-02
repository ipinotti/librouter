/*
 * wifi.h
 *
 *  Created on: Jan 20, 2012
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */
#include <syslog.h>
#include "options.h"

#ifndef WIFI_H_
#define WIFI_H_


//#define DEBUG_WIFI_SYSLOG
#ifdef DEBUG_WIFI_SYSLOG
#define wifi_dbgs(x,...) \
		syslog(LOG_INFO,  "%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define wifi_dbgs(x,...)
#endif

//#define DEBUG_WIFI_PRINTF
#ifdef DEBUG_WIFI_PRINTF
#define wifi_dbgp(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define wifi_dbgp(x,...)
#endif

#ifdef OPTION_HOSTAP
#define HOSTAPD_CONFIG_FILE "/etc/hostapd.conf"
#define WIFI_CONFIG_FILE HOSTAPD_CONFIG_FILE
#else
#define WIFI_CONFIG_FILE "/etc/Wireless/file_example.conf" /*MUST BE SETTED - for instance, dummy file is defined*/
#endif

#define WIFI_MAJOR 0

#ifdef OPTION_HOSTAP

#define HOSTAPD "hostapd"
#define WIFI_SSID_KEY "ssid="
#define WIFI_SSID_BROADCAST_KEY "ignore_broadcast_ssid="
#define WIFI_CHANNEL_KEY "channel="
#define WIFI_CH_MAX 13
#define WIFI_HW_MODE_KEY "hw_mode="
#define WIFI_HW_MODE_N_KEY "ieee80211n="
#define WIFI_MAX_NUM_STA_KEY "max_num_sta="
#define WIFI_MAX_NUM_STA 1500
#define WIFI_BEACON_INTRVAL_KEY "beacon_int="
#define WIFI_MAX_BEACON_INTRVAL 65535
#define WIFI_RTS_THRESHOLD_KEY "rts_threshold="
#define WIFI_MAX_RTS_THRESHOLD 2347
#define WIFI_FRAGM_THRESHOLD_KEY "fragm_threshold="
#define WIFI_MAX_FRAGM_THRESHOLD 2346
#define WIFI_MIN_FRAGM_THRESHOLD 256
#define WIFI_DTIM_INTER_KEY "dtim_period="
#define WIFI_MAX_DTIM_INTER 255
#define WIFI_PREAMBLE_KEY "preamble="
#define WIFI_WEP_AUTH_KEY "auth_algs="
#define WIFI_WEP_DEF_KEY "wep_default_key="
#define WIFI_WEP_DEF_COD_KEY 0
#define WIFI_WEP_PASS_KEY "wep_key0="
#define WIFI_SEC_WPA_MODE_KEY "wpa="
#define WIFI_WPA_PSK_KEY "wpa_psk="
#define WIFI_WPA_PHR_KEY "wpa_passphrase="

#define WIFI_WPA_MGMT_KEY "wpa_key_mgmt=WPA-PSK\n"
#define WIFI_WPA_PW_KEY "wpa_pairwise=TKIP CCMP\n"
#define WIFI_WPA_RSN_KEY "rsn_pairwise=CCMP\n"

#endif

#define member_size(type, member) sizeof(((type *)0)->member)


typedef enum {
	a_hw, b_hw, g_hw, n_hw
} wifi_hw_mode;

typedef enum {
	long_p, short_p
} wifi_preamble_type;

typedef enum {
	wep, wpa, wpa2, wpa_wpa2, none_sec
} wifi_encryp_type;

typedef enum {
	open_a = 1, shared = 2
} wifi_wep_auth_mode;

typedef enum {
	key_ascii, key_hex
} wifi_wep_key_type;

typedef struct {
	wifi_encryp_type security_mode;
	wifi_wep_auth_mode wep_auth;
	wifi_wep_key_type wep_key_type;
	char wep_key[27];
	char wpa_psk[65];
	char wpa_phrase[64];
} librouter_wifi_security_mode_struct;


typedef struct {
	char ssid[33];
	int ssid_broadcast;
	int channel;
	wifi_hw_mode hw_mode;
	int max_num_stations;
	int beacon_interval;
	int rts_threshold;
	int fragm_threshold;
	int dtim_interval;
	wifi_preamble_type preamble_type;
	librouter_wifi_security_mode_struct security;
} librouter_wifi_struct;

#ifdef OPTION_HOSTAP
int librouter_wifi_interface_status(void);
int librouter_wifi_ssid_set (char * ssid_name, int ssid_size);
int librouter_wifi_ssid_get (char * ssid_name, int ssid_size);
int librouter_wifi_ssid_broadcast_enable_set (int enable);
int librouter_wifi_ssid_broadcast_enable_get (void);
int librouter_wifi_channel_set (int channel);
int librouter_wifi_channel_get (void);
int librouter_wifi_hw_mode_set (wifi_hw_mode hw_mode);
int librouter_wifi_hw_mode_get (void);
int librouter_wifi_max_num_sta_set (int max_num_sta);
int librouter_wifi_max_num_sta_get (void);
int librouter_wifi_beacon_inteval_set (int beacon_inter);
int librouter_wifi_beacon_inteval_get (void);
int librouter_wifi_rts_threshold_set (int rts_thresh);
int librouter_wifi_rts_threshold_get (void);
int librouter_wifi_fragm_threshold_set (int fragm_thresh);
int librouter_wifi_fragm_threshold_get (void);
int librouter_wifi_dtim_inter_set (int dtim_inter);
int librouter_wifi_dtim_inter_get (void);
int librouter_wifi_preamble_type_set (wifi_preamble_type preamble_T);
int librouter_wifi_preamble_type_get (void);
int librouter_wifi_security_mode_set (librouter_wifi_security_mode_struct * security);
int librouter_wifi_security_mode_get (librouter_wifi_security_mode_struct * security);

int librouter_wifi_all_configs_set (librouter_wifi_struct * wifi_config);
int librouter_wifi_all_configs_get (librouter_wifi_struct * wifi_config);

int librouter_wifi_hostapd_enable_set (int enable);
int librouter_wifi_hostapd_enable_get (void);

int librouter_wifi_dump(FILE * out);
#endif

#endif /* WIFI_H_ */
