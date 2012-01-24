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
#define WIFI_CONFIG_FILE "/etc/Wireless/file" /*MUST BE SETTED - for instance, dummy file is defined*/
#endif

#define HOSTAPD "hostapd"

typedef enum {
	a, b, g, n
} wifi_hw_mode;

typedef enum {
	short_p, _long_p
} wifi_preamble_type;

typedef enum {
	wep, wpa, wpa2, wpa_wpa2
} wifi_encryp_type;

typedef enum {
	open, shared
} wifi_wep_auth_mode;

typedef struct {
	wifi_encryp_type security_mode;
	wifi_wep_auth_mode wep_auth;
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

int librouter_wifi_ssid_set (char * ssid_name);
int librouter_wifi_ssid_get (char * ssid_name, int ssid_size);
int librouter_wifi_ssid_broadcast_enable_set (int enable);
int librouter_wifi_ssid_broadcast_enable_get (void);
int librouter_wifi_channel_set (int channel);
int librouter_wifi_channel_get (void);
int librouter_wifi_hw_mode_set (wifi_hw_mode hw_mode);
int librouter_wifi_hw_mode_get (wifi_hw_mode hw_mode);
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
int librouter_wifi_preamble_type_get (wifi_preamble_type preamble_T);
int librouter_wifi_sercurity_mode_set (librouter_wifi_security_mode_struct security);
int librouter_wifi_sercurity_mode_get (librouter_wifi_security_mode_struct security);

int librouter_wifi_all_configs_set (librouter_wifi_struct wifi_config);
int librouter_wifi_all_configs_get (librouter_wifi_struct wifi_config);

int librouter_wifi_hostapd_enable_set (int enable);
int librouter_wifi_hostapd_enable_get (void);

int librouter_wifi_dump(FILE * out);


#endif /* WIFI_H_ */
