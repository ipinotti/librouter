/*
 * wifi.c
 *
 *  Created on: Jan 20, 2012
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "options.h"
#include "args.h"
#include "ip.h"
#include "str.h"
#include "exec.h"
#include "dev.h"
#include "device.h"

#ifdef OPTION_WIFI
#include "wifi.h"

#ifdef OPTION_HOSTAP

#define WIFI_MAJOR 0
#define SSID_KEY "ssid="
#define SSID_BROADCAST_KEY "ignore_broadcast_ssid="
#define WIFI_CHANNEL_KEY "channel="
#define WIFI_CH_MAX 13
#define WIFI_HW_MODE_KEY "hw_mode="

int librouter_wifi_ssid_set (char * ssid_name, int ssid_size)
{
	if (ssid_name == NULL)
		return  -1;

	if (ssid_size < 8 || ssid_size > 63)
		return -1;

	librouter_str_striplf(ssid_name);

	return librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, SSID_KEY, ssid_name);
}

int librouter_wifi_ssid_get (char * ssid_name, int ssid_size)
{
	if (ssid_name == NULL)
		return  -1;

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, SSID_KEY, ssid_name, ssid_size) < 0)
		return -1;

	return 0;
}

int librouter_wifi_ssid_broadcast_enable_set (int enable)
{
	if (enable < 0 || enable > 1)
		return -1;

	if (enable)
		return librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, SSID_BROADCAST_KEY, "0");
	else
		return librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, SSID_BROADCAST_KEY, "1");

	return 0;

}

int librouter_wifi_ssid_broadcast_enable_get (void)
{
	char * enable;

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, SSID_BROADCAST_KEY, enable, 1) < 0)
		return -1;

	return atoi(enable);

}

int librouter_wifi_channel_set (int channel)
{
	char ch[3];
	memset(ch, 0, sizeof(ch));

	if (channel < 0 || channel > WIFI_CH_MAX)
		return -1;

	snprintf(ch, sizeof(ch),"%d",channel);

	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_CHANNEL_KEY, ch) < 0)
		return -1;

	return 0;

}

int librouter_wifi_channel_get (void)
{
	char ch[3];
	memset(ch, 0, sizeof(ch));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_KEY, ch, sizeof(ch)) < 0)
		return -1;

	return atoi(ch);
}

int librouter_wifi_hw_mode_set (wifi_hw_mode hw_mode)
{

	return 0;
}

int librouter_wifi_hw_mode_get (wifi_hw_mode hw_mode)
{
	return 0;
}

int librouter_wifi_max_num_sta_set (int max_num_sta)
{
	return 0;
}

int librouter_wifi_max_num_sta_get (void)
{
	return 0;
}

int librouter_wifi_beacon_inteval_set (int beacon_inter)
{
	return 0;
}

int librouter_wifi_beacon_inteval_get (void)
{
	return 0;
}
int librouter_wifi_rts_threshold_set (int rts_thresh)
{
	return 0;
}

int librouter_wifi_rts_threshold_get (void)
{
	return 0;
}

int librouter_wifi_fragm_threshold_set (int fragm_thresh)
{
	return 0;
}

int librouter_wifi_fragm_threshold_get (void)
{
	return 0;
}

int librouter_wifi_dtim_inter_set (int dtim_inter)
{
	return 0;
}

int librouter_wifi_dtim_inter_get (void)
{
	return 0;
}

int librouter_wifi_preamble_type_set (wifi_preamble_type preamble_T)
{
	return 0;
}

int librouter_wifi_preamble_type_get (wifi_preamble_type preamble_T)
{
	return 0;
}

int librouter_wifi_sercurity_mode_set (librouter_wifi_security_mode_struct security)
{
	return 0;
}

int librouter_wifi_sercurity_mode_get (librouter_wifi_security_mode_struct security)
{
	return 0;
}

int librouter_wifi_all_configs_set (librouter_wifi_struct wifi_config)
{
	return 0;
}

int librouter_wifi_all_configs_get (librouter_wifi_struct wifi_config)
{
	return 0;

}

static int wifi_hostapd_enable (void)
{
	if (!librouter_exec_check_daemon(HOSTAPD)){
		librouter_exec_daemon(HOSTAPD);
	}
	return 0;
}

static int wifi_hostapd_disable (void)
{
	if (librouter_exec_check_daemon(HOSTAPD)){
		librouter_kill_daemon(HOSTAPD);
	}
	return 0;
}

int librouter_wifi_hostapd_enable_set (int enable)
{
	if (enable > 1 || enable < 0)
		return -1;

	if (enable)
		return wifi_hostapd_enable();
	else
		return wifi_hostapd_disable();
}

int librouter_wifi_hostapd_enable_get (void)
{
	if (librouter_exec_check_daemon(HOSTAPD))
		return 1;
	else
		return 0;
}

int librouter_wifi_interface_status(void)
{
	dev_family * fam;
	char intf[8];
	memset(intf, 0, sizeof(intf));

	fam = librouter_device_get_family_by_type(wlan);

	snprintf(intf, sizeof(intf),"%s%d",fam->linux_string, WIFI_MAJOR);

	if (librouter_wifi_hostapd_enable_get() || librouter_dev_get_link(intf))
		return 1;

	return 0;
}

int librouter_wifi_dump(FILE * out)
{
	return 0;

}

#endif
#endif
