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

int librouter_wifi_ssid_set(char * ssid_name, int ssid_size)
{
	if (ssid_name == NULL)
		return -1;

	if (ssid_size < 8 || ssid_size > 63)
		return -1;

	librouter_str_striplf(ssid_name);

	return librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_SSID_KEY, ssid_name);
}

int librouter_wifi_ssid_get(char * ssid_name, int ssid_size)
{
	if (ssid_name == NULL)
		return -1;

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_SSID_KEY, ssid_name,
	                ssid_size) < 0)
		return -1;

	return 0;
}

int librouter_wifi_ssid_broadcast_enable_set(int enable)
{
	if (enable < 0 || enable > 1)
		return -1;

	if (enable)
		return librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE,
		                WIFI_SSID_BROADCAST_KEY, "0");
	else
		return librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE,
		                WIFI_SSID_BROADCAST_KEY, "1");

	return 0;
}

int librouter_wifi_ssid_broadcast_enable_get(void)
{
	char enable[2];
	memset(enable, 0, sizeof(enable));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_SSID_BROADCAST_KEY,
	                enable, sizeof(enable)) < 0)
		return -1;

	return atoi(enable);
}

int librouter_wifi_channel_set(int channel)
{
	char ch[3];
	memset(ch, 0, sizeof(ch));

	if (channel < 0 || channel > WIFI_CH_MAX)
		return -1;

	snprintf(ch, sizeof(ch), "%d", channel);

	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_CHANNEL_KEY, ch) < 0)
		return -1;

	return 0;
}

int librouter_wifi_channel_get(void)
{
	char ch[3];
	memset(ch, 0, sizeof(ch));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_CHANNEL_KEY, ch,
	                sizeof(ch)) < 0)
		return -1;

	return atoi(ch);
}

int librouter_wifi_hw_mode_set(wifi_hw_mode hw_mode)
{
	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_N_KEY, "0") < 0)
		return -1;

	switch (hw_mode) {
	case a_hw:
		if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_KEY,
		                "a") < 0)
			return -1;
		break;
	case b_hw:
		if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_KEY,
		                "b") < 0)
			return -1;
		break;
	case g_hw:
		if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_KEY,
		                "g") < 0)
			return -1;
		break;
	case n_hw:
		if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_KEY,
		                "g") < 0)
			return -1;
		if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_N_KEY,
		                "1") < 0)
			return -1;
		break;
	default:
		return -1;
		break;
	}

	return 0;
}

int librouter_wifi_hw_mode_get(void)
{
	char hw[2];
	char hw_N[2];
	memset(hw, 0, sizeof(hw));
	memset(hw_N, 0, sizeof(hw_N));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_KEY, hw,
	                sizeof(hw)) < 0)
		return -1;

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_HW_MODE_N_KEY, hw_N,
	                sizeof(hw_N)) < 0)
		return -1;

	if (atoi(hw_N) == 1) {
		return (int) n_hw;
	}

	switch ((char) hw[0]) {
	case 'a':
		return (int) a_hw;
		break;
	case 'b':
		return (int) b_hw;
		break;
	case 'g':
		return (int) g_hw;
		break;
	default:
		return -1;
		break;
	}

	return 0;
}

int librouter_wifi_max_num_sta_set(int max_num_sta)
{
	char sta_num[6];
	memset(sta_num, 0, sizeof(sta_num));

	if (max_num_sta < 1 || max_num_sta > WIFI_MAX_NUM_STA)
		return -1;

	snprintf(sta_num, sizeof(sta_num), "%d", max_num_sta);

	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_MAX_NUM_STA_KEY,
	                sta_num) < 0)
		return -1;

	return 0;
}

int librouter_wifi_max_num_sta_get(void)
{
	char sta_num[6];
	memset(sta_num, 0, sizeof(sta_num));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_MAX_NUM_STA_KEY, sta_num,
	                sizeof(sta_num)) < 0)
		return -1;

	return atoi(sta_num);
}

int librouter_wifi_beacon_inteval_set(int beacon_inter)
{
	char beacon_value[6];
	memset(beacon_value, 0, sizeof(beacon_value));

	if (beacon_inter < 15 || beacon_inter > WIFI_MAX_BEACON_INTRVAL)
		return -1;

	snprintf(beacon_value, sizeof(beacon_value), "%d", beacon_inter);

	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_BEACON_INTRVAL_KEY,
	                beacon_value) < 0)
		return -1;

	return 0;
}

int librouter_wifi_beacon_inteval_get(void)
{
	char beacon_value[6];
	memset(beacon_value, 0, sizeof(beacon_value));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_BEACON_INTRVAL_KEY,
	                beacon_value, sizeof(beacon_value)) < 0)
		return -1;

	return atoi(beacon_value);
}

int librouter_wifi_rts_threshold_set(int rts_thresh)
{
	char rts_value[6];
	memset(rts_value, 0, sizeof(rts_value));

	if (rts_thresh < 0 || rts_thresh > WIFI_MAX_RTS_THRESHOLD)
		return -1;

	snprintf(rts_value, sizeof(rts_value), "%d", rts_thresh);

	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_RTS_THRESHOLD_KEY,
	                rts_value) < 0)
		return -1;

	return 0;
}

int librouter_wifi_rts_threshold_get(void)
{
	char rts_value[6];
	memset(rts_value, 0, sizeof(rts_value));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_RTS_THRESHOLD_KEY,
	                rts_value, sizeof(rts_value)) < 0)
		return -1;

	return atoi(rts_value);
}

int librouter_wifi_fragm_threshold_set(int fragm_thresh)
{
	char fragm_value[6];
	memset(fragm_value, 0, sizeof(fragm_value));

	if (fragm_thresh < WIFI_MIN_FRAGM_THRESHOLD || fragm_thresh > WIFI_MAX_FRAGM_THRESHOLD)
		return -1;

	snprintf(fragm_value, sizeof(fragm_value), "%d", fragm_thresh);

	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_FRAGM_THRESHOLD_KEY,
	                fragm_value) < 0)
		return -1;

	return 0;
}

int librouter_wifi_fragm_threshold_get(void)
{
	char fragm_value[6];
	memset(fragm_value, 0, sizeof(fragm_value));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_FRAGM_THRESHOLD_KEY,
	                fragm_value, sizeof(fragm_value)) < 0)
		return -1;

	return atoi(fragm_value);
}

int librouter_wifi_dtim_inter_set(int dtim_inter)
{
	char dtim_value[6];
	memset(dtim_value, 0, sizeof(dtim_value));

	if (dtim_inter < 1 || dtim_inter > WIFI_MAX_DTIM_INTER)
		return -1;

	snprintf(dtim_value, sizeof(dtim_value), "%d", dtim_inter);

	if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_DTIM_INTER_KEY,
	                dtim_value) < 0)
		return -1;

	return 0;
}

int librouter_wifi_dtim_inter_get(void)
{
	char dtim_value[6];
	memset(dtim_value, 0, sizeof(dtim_value));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_DTIM_INTER_KEY,
	                dtim_value, sizeof(dtim_value)) < 0)
		return -1;

	return atoi(dtim_value);
}

int librouter_wifi_preamble_type_set(wifi_preamble_type preamble_T)
{
	switch (preamble_T) {
	case long_p:
		if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_PREAMBLE_KEY,
		                "0") < 0)
			return -1;
		break;
	case short_p:
		if (librouter_str_replace_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_PREAMBLE_KEY,
		                "1") < 0)
			return -1;
		break;
	default:
		return -1;
		break;
	}

	return 0;
}

int librouter_wifi_preamble_type_get(void)
{
	char preamble[2];
	memset(preamble, 0, sizeof(preamble));

	if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_PREAMBLE_KEY, preamble,
	                sizeof(preamble)) < 0)
		return -1;

	switch (atoi(preamble)) {
	case long_p:
		return (int) long_p;
		break;
	case short_p:
		return (int) short_p;
		break;
	default:
		return -1;
		break;
	}

	return 0;
}

static int wifi_disable_all_security_in_file(void)
{
	//wep remove config lines
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WEP_AUTH_KEY) < 0)
		return -1;
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WEP_DEF_KEY) < 0)
		return -1;
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WEP_PASS_KEY) < 0)
		return -1;

	//wpa remove config lines
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_SEC_WPA_MODE_KEY) < 0)
		return -1;
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WPA_PHR_KEY) < 0)
		return -1;
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WPA_PSK_KEY) < 0)
		return -1;
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WPA_MGMT_KEY) < 0)
		return -1;
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WPA_PW_KEY) < 0)
		return -1;
	if (librouter_str_del_line_in_file(WIFI_CONFIG_FILE, WIFI_WPA_RSN_KEY) < 0)
		return -1;

	return 0;
}

int librouter_wifi_security_mode_set(librouter_wifi_security_mode_struct * security)
{
	int ret = 0;
	int line_sizeof = 80;
	char * line = NULL;

	if (wifi_disable_all_security_in_file() < 0)
		return -1;

	line = malloc(line_sizeof);

	switch (security->security_mode) {
	case wep:
		memset(line, 0, line_sizeof);
		if (security->wep_auth == shared) {
			snprintf(line, line_sizeof, "%s%d\n", WIFI_WEP_AUTH_KEY, shared);
			if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, line)) < 0)
				break;

			memset(line, 0, line_sizeof);
			snprintf(line, line_sizeof, "%s%d\n", WIFI_WEP_DEF_KEY, WIFI_WEP_DEF_COD_KEY);
			if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, line)) < 0)
				break;

			memset(line, 0, line_sizeof);
			if (security->wep_key_type == key_ascii)
				snprintf(line, line_sizeof, "%s\"%s\"\n", WIFI_WEP_PASS_KEY, security->wep_key);
			else
				snprintf(line, line_sizeof, "%s%s\n", WIFI_WEP_PASS_KEY, security->wep_key);

			if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, line)) < 0)
				break;
		} else {
			memset(line, 0, line_sizeof);
			snprintf(line, line_sizeof, "%s%d\n", WIFI_WEP_AUTH_KEY, open_a);
			if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, line)) < 0)
				break;
		}
		break;

	case wpa:
	case wpa2:
	case wpa_wpa2:
		memset(line, 0, line_sizeof);
		snprintf(line, line_sizeof, "%s%d\n", WIFI_SEC_WPA_MODE_KEY, security->security_mode);
		if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, line)) < 0)
			break;

		ret = -1;

		if (strlen(security->wpa_psk) == 64) {
			memset(line, 0, line_sizeof);
			snprintf(line, line_sizeof, "%s%s\n", WIFI_WPA_PSK_KEY, security->wpa_psk);
			if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, line)) < 0)
				break;
		} else {
			if (strlen(security->wpa_phrase) > 7) {
				memset(line, 0, line_sizeof);
				snprintf(line, line_sizeof, "%s%s\n", WIFI_WPA_PHR_KEY, security->wpa_phrase);
				if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, line)) < 0)
					break;
			}
		}

		if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, WIFI_WPA_MGMT_KEY)) < 0)
			break;
		if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, WIFI_WPA_PW_KEY)) < 0)
			break;
		if ((ret = librouter_str_add_line_to_file(WIFI_CONFIG_FILE, WIFI_WPA_RSN_KEY)) < 0)
			break;

		break;

	case none_sec:
		ret = 0;
		break;

	default:
		ret = -1;
		break;
	}

	free(line);
	return ret;
}

int librouter_wifi_security_mode_get(librouter_wifi_security_mode_struct * security)
{
	char mode[2];
	memset(mode, 0, sizeof(mode));
	memset(security, 0, sizeof(librouter_wifi_security_mode_struct));

	if (librouter_str_find_string_in_file_return_stat(WIFI_CONFIG_FILE, "auth_algs=2")) {
		if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_WEP_PASS_KEY,
		                security->wep_key, member_size(librouter_wifi_security_mode_struct, wep_key))
		                < 0)
			return -1;
		librouter_str_strip_quot_marks(security->wep_key);
		security->security_mode = wep;
		security->wep_auth = shared;
		return 0;
	} else if (librouter_str_find_string_in_file_return_stat(WIFI_CONFIG_FILE, "auth_algs=1")) {
		security->security_mode = wep;
		security->wep_auth = open_a;
		return 0;
	}

	if (librouter_str_find_string_in_file_return_stat(WIFI_CONFIG_FILE, "wpa")) {
		if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE, WIFI_SEC_WPA_MODE_KEY,
		                mode, sizeof(mode)) < 0)
			return -1;
		switch (atoi(mode)) {
		case 1:
			security->security_mode = wpa;
			break;
		case 2:
			security->security_mode = wpa2;
			break;
		case 3:
			security->security_mode = wpa_wpa2;
			break;
		default:
			return -1;
			break;
		}

		if (librouter_str_find_string_in_file_return_stat(WIFI_CONFIG_FILE, WIFI_WPA_PSK_KEY)) {
			if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE,
			                WIFI_WPA_PSK_KEY, security->wpa_psk,
			                member_size(librouter_wifi_security_mode_struct, wpa_psk)) < 0)
				return -1;
		} else if (librouter_str_find_string_in_file_without_space(WIFI_CONFIG_FILE,
		                WIFI_WPA_PHR_KEY, security->wpa_phrase,
		                member_size(librouter_wifi_security_mode_struct, wpa_phrase)) < 0)
			return -1;

		return 0;
	}

	security->security_mode = none_sec;

	return 0;
}

int librouter_wifi_all_configs_set(librouter_wifi_struct * wifi_config)
{
	if (librouter_wifi_ssid_set(wifi_config->ssid, member_size(librouter_wifi_struct, ssid)) < 0)
		return -1;

	if (librouter_wifi_ssid_broadcast_enable_set(wifi_config->ssid_broadcast) < 0)
		return -1;

	if (librouter_wifi_channel_set(wifi_config->channel) < 0)
		return -1;

	if (librouter_wifi_hw_mode_set(wifi_config->hw_mode) < 0)
		return -1;

	if (librouter_wifi_max_num_sta_set(wifi_config->max_num_stations) < 0)
		return -1;

	if (librouter_wifi_beacon_inteval_set(wifi_config->beacon_interval) < 0)
		return -1;

	if (librouter_wifi_rts_threshold_set(wifi_config->rts_threshold) < 0)
		return -1;

	if (librouter_wifi_fragm_threshold_set(wifi_config->fragm_threshold) < 0)
		return -1;

	if (librouter_wifi_dtim_inter_set(wifi_config->dtim_interval) < 0)
		return -1;

	if (librouter_wifi_preamble_type_set(wifi_config->preamble_type) < 0)
		return -1;

	if (librouter_wifi_security_mode_set(&wifi_config->security) < 0)
		return -1;

	return 0;
}

int librouter_wifi_all_configs_get(librouter_wifi_struct * wifi_config)
{
	int check_value = 0;

	if (librouter_wifi_ssid_get(wifi_config->ssid, member_size(librouter_wifi_struct, ssid)) < 0)
		return -1;

	if ((wifi_config->ssid_broadcast = librouter_wifi_ssid_broadcast_enable_get()) < 0)
		return -1;

	if ((wifi_config->channel = librouter_wifi_channel_get()) < 0)
		return -1;

	if ((check_value = librouter_wifi_hw_mode_get()) < 0)
		return -1;
	else {
		wifi_config->hw_mode = check_value;
		check_value = 0;
	}
	if ((wifi_config->max_num_stations = librouter_wifi_max_num_sta_get()) < 0)
		return -1;

	if ((wifi_config->beacon_interval = librouter_wifi_beacon_inteval_get()) < 0)
		return -1;

	if ((wifi_config->rts_threshold = librouter_wifi_rts_threshold_get()) < 0)
		return -1;

	if ((wifi_config->fragm_threshold = librouter_wifi_fragm_threshold_get()) < 0)
		return -1;

	if ((wifi_config->dtim_interval = librouter_wifi_dtim_inter_get()) < 0)
		return -1;

	if ((check_value = librouter_wifi_preamble_type_get()) < 0)
		return -1;
	else {
		wifi_config->preamble_type = check_value;
		check_value = 0;
	}

	if (librouter_wifi_security_mode_get(&wifi_config->security) < 0)
		return -1;

	return 0;
}

static int wifi_hostapd_enable(void)
{
	if (!librouter_exec_check_daemon(HOSTAPD)) {
		librouter_exec_daemon(HOSTAPD);
	}
	return 0;
}

static int wifi_hostapd_disable(void)
{
	if (librouter_exec_check_daemon(HOSTAPD)) {
		librouter_kill_daemon(HOSTAPD);
	}
	return 0;
}

int librouter_wifi_hostapd_enable_set(int enable)
{
	if (enable > 1 || enable < 0)
		return -1;

	if (enable)
		return wifi_hostapd_enable();
	else
		return wifi_hostapd_disable();
}

int librouter_wifi_hostapd_enable_get(void)
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

	snprintf(intf, sizeof(intf), "%s%d", fam->linux_string, WIFI_MAJOR);

	if (librouter_wifi_hostapd_enable_get() || librouter_dev_get_link(intf))
		return 1;

	return 0;
}

int librouter_wifi_dump(FILE * out)
{
	librouter_wifi_struct wifi_cfg;
	char hw;
	char encrpt[10];
	memset(encrpt, 0, sizeof(encrpt));
	memset(&wifi_cfg, 0, sizeof(wifi_cfg));

	librouter_wifi_all_configs_get(&wifi_cfg);

	fprintf(out, " ap-manager\n");

	fprintf(out, "  ssid %s\n", wifi_cfg.ssid);

	fprintf(out, "  %sssid-broadcast\n", wifi_cfg.ssid_broadcast ? "no " : "");

	fprintf(out, "  channel %d\n", wifi_cfg.channel);

	switch (wifi_cfg.hw_mode) {
#ifdef NOT_YET_IMPLEMENTED
	case a_hw:
	hw = 'a';
	break;
#endif
	case b_hw:
		hw = 'b';
		break;
	case g_hw:
		hw = 'g';
		break;
	case n_hw:
		hw = 'n';
		break;
	default:
		break;
	}
	fprintf(out, "  hw-mode %c\n", hw);

	fprintf(out, "  max-num-station %d\n", wifi_cfg.max_num_stations);

	fprintf(out, "  beacon-interval %d\n", wifi_cfg.beacon_interval);

	fprintf(out, "  rts-threshold %d\n", wifi_cfg.rts_threshold);

	fprintf(out, "  fragmentation %d\n", wifi_cfg.fragm_threshold);

	fprintf(out, "  dtim-interval %d\n", wifi_cfg.dtim_interval);

	fprintf(out, "  preamble-type %s\n", wifi_cfg.preamble_type ? "short" : "long");

	switch (wifi_cfg.security.security_mode) {
	case wep:
		if (wifi_cfg.security.wep_auth == open_a) {
			fprintf(out, "  security-mode wep open\n");
			break;
		}
		if (wifi_cfg.security.wep_auth == shared) {
			librouter_str_striplf(wifi_cfg.security.wep_key);
			if (strlen(wifi_cfg.security.wep_key) == 10) {
				fprintf(out, "  security-mode wep shared hex 64Bit %s\n",
				                wifi_cfg.security.wep_key);
				break;
			}
			if (strlen(wifi_cfg.security.wep_key) == 26) {
				fprintf(out, "  security-mode wep shared hex 128Bit %s\n",
				                wifi_cfg.security.wep_key);
				break;
			}
			if (strlen(wifi_cfg.security.wep_key) == 5) {
				fprintf(out, "  security-mode wep shared ascii 64Bit %s\n",
				                wifi_cfg.security.wep_key);
				break;
			}
			if (strlen(wifi_cfg.security.wep_key) == 13) {
				fprintf(out, "  security-mode wep shared ascii 128Bit %s\n",
				                wifi_cfg.security.wep_key);
				break;
			}

		}
		break;
	case wpa:
		snprintf(encrpt, sizeof(encrpt), "wpa");
		goto wpa_set;
		break;
	case wpa2:
		snprintf(encrpt, sizeof(encrpt), "wpa2");
		goto wpa_set;
		break;
	case wpa_wpa2:
		snprintf(encrpt, sizeof(encrpt), "wpa/wpa2");

		wpa_set: if (strlen(wifi_cfg.security.wpa_phrase) > 7) {
			fprintf(out, "  security-mode %s phrase %s\n", encrpt, wifi_cfg.security.wpa_phrase);
			break;
		}
		if (strlen(wifi_cfg.security.wpa_psk) == 64) {
			fprintf(out, "  security-mode %s psk %s\n", encrpt, wifi_cfg.security.wpa_psk);
			break;
		}
		break;
	case none_sec:
		fprintf(out, "  no security-mode\n");
		break;
	default:
		break;
	}

	return 0;
}

#endif /* OPTION_HOSTAP*/

#endif /* OPTION_WIFI*/
