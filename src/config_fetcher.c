/*
 * config_fetcher.c
 *
 *  Created on: May 31, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <dirent.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <net/if_arp.h>
#include <linux/autoconf.h>
#include <linux/mii.h>
#include <syslog.h>

#include "ipv6.h"
#include "options.h"
#include "defines.h"
#include "pam.h"
#include "device.h"
#include "quagga.h"
#include "snmp.h"
#include "ppp.h"
#include "pppoe.h"
#include "pptp.h"
#include "exec.h"
#include "smcroute.h"
#include "ntp.h"
#include "dhcp.h"
#include "dns.h"
#include "args.h"
#include "ip.h"
#include "dev.h"
#include "qos.h"
#include "config_mapper.h"
#include "acl.h"
#include "bridge.h"
#include "lan.h"
#include "pbr.h"

#ifdef OPTION_MANAGED_SWITCH
#if defined(OPTION_SWITCH_MICREL_KSZ8863)
#include "ksz8863.h"
#elif defined(OPTION_SWITCH_MICREL_KSZ8895)
#include "ksz8895.h"
#elif defined(OPTION_SWITCH_BROADCOM)
#include "bcm53115s.h"
#endif
#endif /* OPTION_MANAGED_SWITCH */

#ifdef OPTION_EFM
#include "efm.h"
#endif

#define PPPDEV "ppp"

/********************/
/* Helper functions */
/********************/
/**
 * Read network information from /proc
 *
 * @param parm Proc name
 * @return proc value if success, -1 if failure
 */
static int _get_procip_val(const char *parm)
{
	int fid;
	char buf[128];

	snprintf(buf, sizeof(buf), "/proc/sys/net/ipv4/%s", parm);

	fid = open(buf, O_RDONLY);
	if (fid < 0) {
		printf("%% Error opening %s\n%% %s\n", buf, strerror(errno));
		return -1;
	}

	read(fid, buf, 16);
	close(fid);

	return atoi(buf);
}

/**
 * Read network IPv6 information from /proc
 *
 * @param parm Proc name
 * @return proc value if success, -1 if failure
 */
static int _get_procipv6_all_val(const char *parm)
{
	int fid;
	char buf[128];

	snprintf(buf, sizeof(buf), "/proc/sys/net/ipv6/conf/all/%s", parm);

	fid = open(buf, O_RDONLY);
	if (fid < 0) {
		printf("%% Error opening %s\n%% %s\n", buf, strerror(errno));
		return -1;
	}

	read(fid, buf, 16);
	close(fid);

	return atoi(buf);
}

/***********************/
/* Main dump functions */
/***********************/
void librouter_config_dump_version(FILE *f, struct router_config *cfg)
{
	fprintf(f, "version %s\n", librouter_get_system_version());
	fprintf(f, "!\n");
}

void librouter_config_dump_terminal(FILE *f, struct router_config *cfg)
{
	fprintf(f, "terminal length %d\n", cfg->terminal_lines);
	fprintf(f, "terminal timeout %d\n", cfg->terminal_timeout);
	fprintf(f, "!\n");
}

void librouter_config_dump_secret(FILE *f, struct router_config *cfg)
{
	int printed_something = 0;

	if (cfg->enable_secret[0]) {
		fprintf(f, "secret enable hash %s\n", cfg->enable_secret);
		printed_something = 1;
	}

	if (cfg->login_secret[0]) {
		fprintf(f, "secret login hash %s\n", cfg->login_secret);
		printed_something = 1;
	}

	if (printed_something)
		fprintf(f, "!\n");
}

void librouter_config_dump_aaa(FILE *f, struct router_config *cfg)
{
	int i;
	FILE *passwd;
	struct auth_server tacacs_server[AUTH_MAX_SERVERS];
	struct auth_server radius_server[AUTH_MAX_SERVERS];

	memset(tacacs_server, 0, sizeof(tacacs_server));
	memset(radius_server, 0, sizeof(radius_server));

	librouter_pam_get_radius_servers(radius_server);
	librouter_pam_get_tacacs_servers(tacacs_server);

	/* Dump RADIUS & TACACS servers */
	for (i = 0; i < AUTH_MAX_SERVERS; i++) {
		if (radius_server[i].ipaddr) {

			fprintf(f, "radius-server host %s", radius_server[i].ipaddr);

			if (radius_server[i].key)
				fprintf(f, " key %s", radius_server[i].key);

			if (radius_server[i].timeout)
				fprintf(f, " timeout %d", radius_server[i].timeout);

			fprintf(f, "\n");
		}
	}

	for (i = 0; i < AUTH_MAX_SERVERS; i++) {
		if (tacacs_server[i].ipaddr) {

			fprintf(f, "tacacs-server host %s", tacacs_server[i].ipaddr);

			if (tacacs_server[i].key)
				fprintf(f, " key %s", tacacs_server[i].key);

			if (tacacs_server[i].timeout)
				fprintf(f, " timeout %d", tacacs_server[i].timeout);

			fprintf(f, "\n");
		}
	}

	librouter_pam_free_servers(AUTH_MAX_SERVERS, tacacs_server);
	librouter_pam_free_servers(AUTH_MAX_SERVERS, radius_server);

	/* Dump aaa authentication login mode */
	switch (librouter_pam_get_current_mode(FILE_PAM_LOGIN)) {
	case AAA_AUTH_NONE:
		fprintf(f, "aaa authentication cli default none\n");
		break;
	case AAA_AUTH_LOCAL:
		fprintf(f, "aaa authentication cli default local\n");
		break;
	case AAA_AUTH_RADIUS:
		fprintf(f, "aaa authentication cli default group radius\n");
		break;
	case AAA_AUTH_RADIUS_LOCAL:
		fprintf(f, "aaa authentication cli default group radius local\n");
		break;
	case AAA_AUTH_TACACS:
		fprintf(f, "aaa authentication cli default group tacacs+\n");
		break;
	case AAA_AUTH_TACACS_LOCAL:
		fprintf(f, "aaa authentication cli default group tacacs+ local\n");
		break;
	default:
		fprintf(f, "aaa authentication cli none\n");
		break;
	}

	/* Dump aaa authentication login mode */
	switch (librouter_pam_get_current_mode(FILE_PAM_ENABLE)) {
	case AAA_AUTH_NONE:
		fprintf(f, "aaa authentication enable default none\n");
		break;
	case AAA_AUTH_LOCAL:
		fprintf(f, "aaa authentication enable default local\n");
		break;
	case AAA_AUTH_RADIUS:
		fprintf(f, "aaa authentication enable default group radius\n");
		break;
	case AAA_AUTH_RADIUS_LOCAL:
		fprintf(f, "aaa authentication enable default group radius local\n");
		break;
	case AAA_AUTH_TACACS:
		fprintf(f, "aaa authentication enable default group tacacs+\n");
		break;
	case AAA_AUTH_TACACS_LOCAL:
		fprintf(f, "aaa authentication enable default group tacacs+ local\n");
		break;
	default:
		fprintf(f, "aaa authentication enable none\n");
		break;
	}

	/* Dump aaa authentication web mode */
	switch (librouter_pam_get_current_mode(FILE_PAM_WEB)) {
	case AAA_AUTH_NONE:
		fprintf(f, "aaa authentication web default none\n");
		break;
	case AAA_AUTH_LOCAL:
		fprintf(f, "aaa authentication web default local\n");
		break;
	case AAA_AUTH_RADIUS:
		fprintf(f, "aaa authentication web default group radius\n");
		break;
	case AAA_AUTH_RADIUS_LOCAL:
		fprintf(f, "aaa authentication web default group radius local\n");
		break;
	case AAA_AUTH_TACACS:
		fprintf(f, "aaa authentication web default group tacacs+\n");
		break;
	case AAA_AUTH_TACACS_LOCAL:
		fprintf(f, "aaa authentication web default group tacacs+ local\n");
		break;
	default:
		fprintf(f, "aaa authentication web none\n");
		break;
	}

#ifdef OPTION_AAA_AUTHORIZATION
	/* Dump aaa authorization mode */
	switch (librouter_pam_get_current_author_mode(FILE_PAM_LOGIN)) {
	case AAA_AUTHOR_NONE:
		fprintf(f, "aaa authorization exec default none\n");
		break;
	case AAA_AUTHOR_TACACS:
		fprintf(f, "aaa authorization exec default group tacacs+\n");
		break;
	case AAA_AUTHOR_TACACS_LOCAL:
		fprintf(f, "aaa authorization exec default group tacacs+ local\n");
		break;
	case AAA_AUTHOR_RADIUS:
		fprintf(f, "aaa authorization exec default group radius\n");
		break;
	case AAA_AUTHOR_RADIUS_LOCAL:
		fprintf(f, "aaa authorization exec default group radius local\n");
		break;
	}

	switch (librouter_pam_get_current_author_mode(FILE_PAM_CLI)) {
	case AAA_AUTHOR_NONE:
		fprintf(f, "aaa authorization commands default none\n");
		break;
	case AAA_AUTHOR_TACACS:
		fprintf(f, "aaa authorization commands default group tacacs+\n");
		break;
	case AAA_AUTHOR_TACACS_LOCAL:
		fprintf(f, "aaa authorization commands default group tacacs+ local\n");
		break;
	case AAA_AUTHOR_RADIUS:
		fprintf(f, "aaa authorization commands default group radius\n");
		break;
	case AAA_AUTHOR_RADIUS_LOCAL:
		fprintf(f, "aaa authorization commands default group radius local\n");
		break;
	}
#endif

#ifdef OPTION_AAA_ACCOUNTING
	/* Login file does accounting for exec */
	switch (librouter_pam_get_current_acct_mode(FILE_PAM_LOGIN)) {
	case AAA_ACCT_NONE:
		fprintf(f, "aaa accounting exec default none\n");
		break;
	case AAA_ACCT_TACACS:
		fprintf(f, "aaa accounting exec default start-stop group tacacs+\n");
		break;
	case AAA_ACCT_RADIUS:
		fprintf(f, "aaa accounting exec default start-stop group radius\n");
		break;
	}

	/* CLI file does accounting for commands */
	switch (librouter_pam_get_current_acct_mode(FILE_PAM_CLI)) {
	case AAA_ACCT_NONE:
		fprintf(f, "aaa accounting commands default none\n");
		break;
	case AAA_ACCT_TACACS:
		fprintf(f, "aaa accounting commands default start-stop group tacacs+\n");
		break;
	case AAA_ACCT_RADIUS:
		fprintf(f, "aaa accounting commands default start-stop group radius\n");
		break;
	}
#endif

	/* Dump users */
	if ((passwd = fopen(FILE_PASSWD, "r"))) {
		struct passwd *pw;

		while ((pw = fgetpwent(passwd))) {
			if (pw->pw_uid > 500 && !strncmp(pw->pw_gecos, "Local", 5)) {
				fprintf(f, "aaa username %s password hash %s privilege %d\n", pw->pw_name,
				                pw->pw_passwd, librouter_pam_get_privilege_by_name(pw->pw_name));
			}
		}

		fclose(passwd);
	}

	fprintf(f, "!\n");
}

void librouter_config_dump_hostname(FILE *f)
{
	char buf[64];

	gethostname(buf, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;

	fprintf(f, "hostname %s\n!\n", buf);
}

void librouter_config_dump_log(FILE *f)
{
	char buf[16];

	if (librouter_exec_get_init_option_value(PROG_SYSLOGD, "-R", buf, 16) >= 0)
		fprintf(f, "logging remote %s\n!\n", buf);
}

#ifdef OPTION_ROUTER
#ifdef OPTION_BGP
void librouter_config_bgp_dump_router(FILE *f, int main_nip)
{
	FILE *fd;
	char buf[1024];

	if (!librouter_quagga_bgpd_is_running())
		return;

	/* dump router bgp info */
	fd = librouter_quagga_bgp_get_conf(main_nip);
	if (fd) {
		while (!feof(fd)) {
			fgets(buf, 1024, fd);

			if (buf[0] == '!')
				break;

			librouter_str_striplf(buf);
			fprintf(f, "%s\n", librouter_device_from_linux_cmdline(
			                librouter_zebra_to_linux_cmdline(buf)));
		}

		fclose(fd);
	}

	fprintf(f, "!\n");
}
#endif /* OPTION_BGP */
#endif /* OPTION_ROUTER */

#ifdef OPTION_ROUTER
void librouter_config_dump_ip(FILE *f, int conf_format)
{
	int val;

	val = _get_procip_val("ip_forward");
	fprintf(f, val ? "ip routing\n" : "no ip routing\n");

#ifdef OPTION_PIMD
	val = _get_procip_val("conf/all/mc_forwarding");
	fprintf(f, val ? "ip multicast-routing\n" : "no ip multicast-routing\n");
#endif

	val = _get_procip_val("ip_no_pmtu_disc");
	fprintf(f, val ? "no ip pmtu-discovery\n" : "ip pmtu-discovery\n");

	val = _get_procip_val("ip_default_ttl");
	fprintf(f, "ip default-ttl %i\n", val);

	val = _get_procip_val("conf/all/rp_filter");
	fprintf(f, val ? "ip rp-filter\n" : "no ip rp-filter\n");

	val = _get_procip_val("icmp_echo_ignore_all");
	fprintf(f, val ? "ip icmp ignore all\n" : "no ip icmp ignore all\n");

	val = _get_procip_val("icmp_echo_ignore_broadcasts");
	fprintf(f, val ? "ip icmp ignore broadcasts\n" : "no ip icmp ignore broadcasts\n");

	val = _get_procip_val("icmp_ignore_bogus_error_responses");
	fprintf(f, val ? "ip icmp ignore bogus\n" : "no ip icmp ignore bogus\n");

	val = _get_procip_val("ipfrag_high_thresh");
	fprintf(f, "ip fragment high %i\n", val);

	val = _get_procip_val("ipfrag_low_thresh");
	fprintf(f, "ip fragment low %i\n", val);

	val = _get_procip_val("ipfrag_time");
	fprintf(f, "ip fragment time %i\n", val);

	val = _get_procip_val("tcp_ecn");
	fprintf(f, val ? "ip tcp ecn\n" : "no ip tcp ecn\n");

	val = _get_procip_val("tcp_syncookies");
	fprintf(f, val ? "ip tcp syncookies\n" : "no ip tcp syncookies\n");

	fprintf(f, "!\n");
}

#ifdef OPTION_IPV6
void librouter_config_dump_ipv6(FILE *f, int conf_format)
{
	int val;

	val = !_get_procipv6_all_val("disable_ipv6");
	fprintf(f, val ? "ipv6 enable\n" : "no ipv6 enable\n");

	val = _get_procipv6_all_val("forwarding");
	fprintf(f, val ? "ipv6 routing\n" : "no ipv6 routing\n");

#ifdef NOT_YET_IMPLEMENTED
	if (_get_procipv6_all_val("accept_ra") && _get_procipv6_all_val("autoconf"))
		val = 1;
	fprintf(f, val ? "ipv6 auto-configuration\n" : "no ipv6 auto-configuration\n");
#endif

	fprintf(f, "!\n");
}
#endif

#endif /* OPTION_ROUTER */

void librouter_config_dump_snmp(FILE *f, int conf_format)
{
	int print = 0;
	int len;
	char **sinks;
	char buf[512];

	if (librouter_snmp_get_contact(buf, 511) == 0) {
		print = 1;
		fprintf(f, "snmp-server contact %s\n", buf);
	}

	if (librouter_snmp_get_location(buf, 511) == 0) {
		print = 1;
		fprintf(f, "snmp-server location %s\n", buf);
	}

	if ((len = librouter_snmp_get_trapsinks(&sinks)) > 0) {
		if (sinks) {
			int i;

			for (i = 0; i < len; i++) {
				if (sinks[i]) {
					print = 1;
					fprintf(f, "snmp-server trapsink %s\n", sinks[i]);
					free(sinks[i]);
				}
			}
			free(sinks);
		}
	}

	if (librouter_snmp_dump_communities(f) > 0)
		print = 1;

	if (librouter_snmp_is_running()) {
		print = 1;
#ifdef OPTION_SNMP_VERSION_SELECT
		librouter_snmp_dump_versions(f);
#endif
		fprintf(f, "snmp-server enable\n");
	}

	if (print)
		fprintf(f, "!\n");
}

#ifdef OPTION_RMON
void librouter_config_dump_rmon(FILE *f)
{
	int i, k;
	struct rmon_config *shm_rmon_p;
	char tp[10], result[MAX_OID_LEN * 10];

	if (librouter_snmp_rmon_get_access_cfg(&shm_rmon_p) == 1) {
		for (i = 0; i < NUM_EVENTS; i++) {
			if (shm_rmon_p->events[i].index > 0) {
				fprintf(f, "rmon event %d", shm_rmon_p->events[i].index);

				if (shm_rmon_p->events[i].do_log)
					fprintf(f, " log");

				if (shm_rmon_p->events[i].community[0] != 0)
					fprintf(f, " trap %s", shm_rmon_p->events[i].community);

				if (shm_rmon_p->events[i].description[0] != 0)
					fprintf(f, " description %s",
					                shm_rmon_p->events[i].description);

				if (shm_rmon_p->events[i].owner[0] != 0)
					fprintf(f, " owner %s", shm_rmon_p->events[i].owner);

				fprintf(f, "\n");
			}
		}

		for (i = 0; i < NUM_ALARMS; i++) {
			if (shm_rmon_p->alarms[i].index > 0) {

				result[0] = '\0';

				for (k = 0; k < shm_rmon_p->alarms[i].oid_len; k++) {
					sprintf(tp, "%lu.", shm_rmon_p->alarms[i].oid[k]);
					strcat(result, tp);
				}

				*(result + strlen(result) - 1) = '\0';

				fprintf(f, "rmon alarm %d %s %d", shm_rmon_p->alarms[i].index,
				                result, shm_rmon_p->alarms[i].interval);

				switch (shm_rmon_p->alarms[i].sample_type) {
				case SAMPLE_ABSOLUTE:
					fprintf(f, " absolute");
					break;
				case SAMPLE_DELTA:
					fprintf(f, " delta");
					break;
				}

				if (shm_rmon_p->alarms[i].rising_threshold) {

					fprintf(f, " rising-threshold %d",
					                shm_rmon_p->alarms[i].rising_threshold);

					if (shm_rmon_p->alarms[i].rising_event_index)
						fprintf(
						                f,
						                " %d",
						                shm_rmon_p->alarms[i].rising_event_index);
				}

				if (shm_rmon_p->alarms[i].falling_threshold) {

					fprintf(f, " falling-threshold %d",
					                shm_rmon_p->alarms[i].falling_threshold);

					if (shm_rmon_p->alarms[i].falling_event_index)
						fprintf(
						                f,
						                " %d",
						                shm_rmon_p->alarms[i].falling_event_index);
				}

				if (shm_rmon_p->alarms[i].owner[0] != 0)
					fprintf(f, " owner %s", shm_rmon_p->alarms[i].owner);

				fprintf(f, "\n");
			}
		}
		fprintf(f, "rmon snmp-version %s\n", shm_rmon_p->version == SNMP_VERSION_1 ? "1" :
				shm_rmon_p->version == SNMP_VERSION_2c ? "2c" : "3");
		librouter_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	if (librouter_exec_check_daemon(RMON_DAEMON))
		fprintf(f, "rmon agent\n");
	else
		fprintf(f, "no rmon agent\n");

	fprintf(f, "!\n");
}
#endif

void librouter_config_dump_chatscripts(FILE *f)
{
	FILE *fd;
	int printed_something = 0;
	struct dirent **namelist;
	int n;
	char filename[64];
	char buf[256];

	n = scandir(PPP_CHAT_DIR, &namelist, 0, alphasort);

	if (n < 0) {
		printf("%% cannot open dir "PPP_CHAT_DIR"\n");
		return;
	}

	while (n--) {
		if (namelist[n]->d_name[0] != '.') {
			sprintf(filename, "%s%s", PPP_CHAT_DIR, namelist[n]->d_name);

			fd = fopen(filename, "r");
			if (fd) {
				memset(buf, 0, sizeof(buf));

				fgets(buf, sizeof(buf) - 1, fd);

				fprintf(f, "chatscript %s %s\n", namelist[n]->d_name, buf);
				fclose(fd);
				printed_something = 1;
			}
		}
		free(namelist[n]);
	}

	free(namelist);

	if (printed_something)
		fprintf(f, "!\n");
}

void librouter_config_ospf_dump_router(FILE *out)
{
	FILE *f;
	char buf[1024];

	if (!librouter_quagga_ospfd_is_running())
		return;

	/* if config not written */
	fprintf(out, "router ospf\n");

	f = librouter_quagga_ospf_get_conf(1, NULL);

	if (f) {
		/* skip line */
		fgets(buf, 1024, f);

		while (!feof(f)) {
			fgets(buf, 1024, f);

			if (buf[0] == '!')
				break;

			librouter_str_striplf(buf);
			fprintf(out, "%s\n", librouter_device_from_linux_cmdline(
			                librouter_zebra_to_linux_cmdline(buf)));
		}

		fclose(f);
	}

	fprintf(out, "!\n");
}

void librouter_config_ospf_dump_interface(FILE *out, char *intf)
{
	FILE *f;
	char buf[1024];

	if (!librouter_quagga_ospfd_is_running())
		return;

	f = librouter_quagga_ospf_get_conf(0, intf);

	if (!f)
		return;

	/* skip line */
	fgets(buf, 1024, f);

	while (!feof(f)) {
		fgets(buf, 1024, f);
		if (buf[0] == '!')
			break;

		librouter_str_striplf(buf);

		fprintf(out, "%s\n", librouter_device_from_linux_cmdline(
		                librouter_zebra_to_linux_cmdline(buf)));
	}

	fclose(f);
}

void librouter_config_rip_dump_router(FILE *out)
{
	FILE *f;
	int end;
	char buf[1024];
	char keychain[] = "key chain";

	if (!librouter_quagga_ripd_is_running())
		return;

	/* dump router rip info */
	/* if config not written */
	fprintf(out, "router rip\n");

	f = librouter_quagga_rip_get_conf(1, NULL);

	if (f) {
		/* skip line */
		fgets(buf, 1024, f);

		while (!feof(f)) {

			fgets(buf, 1024, f);

			if (buf[0] == '!')
				break;

			librouter_str_striplf(buf);
			fprintf(out, " %s\n", librouter_device_from_linux_cmdline(
			                librouter_zebra_to_linux_cmdline(buf)));
		}

		fclose(f);
	}

	fprintf(out, "!\n");

	/* dump key info (after router rip!) */
	f = librouter_quagga_get_conf(RIPD_CONF, keychain);

	if (f) {
		end = 0;
		while (!feof(f)) {

			fgets(buf, 1024, f);

			if (end && (strncmp(buf, keychain, sizeof(keychain) != 0)))
				break;
			else
				end = 0;

			if (buf[0] == '!')
				end = 1;

			librouter_str_striplf(buf);

			fprintf(out, "%s\n", buf);
		}
		fclose(f);
	}
}

void librouter_config_rip_dump_interface(FILE *out, char *intf)
{
	FILE *f;
	char buf[1024];

	if (!librouter_quagga_ripd_is_running())
		return;

	f = librouter_quagga_rip_get_conf(0, intf);

	if (!f)
		return;

	/* skip line */
	fgets(buf, 1024, f);

	while (!feof(f)) {

		fgets(buf, 1024, f);

		if (buf[0] == '!')
			break;

		librouter_str_striplf(buf);
		fprintf(out, "%s\n", librouter_device_from_linux_cmdline(
		                librouter_zebra_to_linux_cmdline(buf)));
	}

	fclose(f);
}

void librouter_config_dump_routing(FILE *f)
{
	librouter_quagga_zebra_dump_static_routes(f, 4); /*adicionado [int ip_version = 4] para IPv4*/
}

void librouter_config_dump_routing_ipv6(FILE *f)
{
	librouter_quagga_zebra_dump_static_routes(f, 6); /*adicionado [int ip_version = 6] para IPv6*/
}

void librouter_config_clock_dump(FILE *out)
{
	int hours, mins;
	char name[16];

	if (librouter_time_get_timezone(name, &hours, &mins) == 0) {
		fprintf(out, "clock timezone %s %d", name, hours);

		if (mins > 0)
			fprintf(out, " %d\n", mins);
		else
			fprintf(out, "\n");

		fprintf(out, "!\n");
	}
}

static void _dump_dhcp_server(FILE *out)
{
	struct dhcp_server_conf_t dhcp;

	librouter_dhcp_server_get_config(&dhcp);

	fprintf(out, "ip dhcp server\n");

	if (dhcp.pool_start && dhcp.pool_end)
		fprintf(out, " pool %s %s\n", dhcp.pool_start, dhcp.pool_end);

	if (dhcp.dnsserver)
		fprintf(out, " dns %s\n", dhcp.dnsserver);
	if (dhcp.domain)
		fprintf(out, " domain-name %s\n", dhcp.domain);

	if (dhcp.default_lease_time) {
		int days = dhcp.default_lease_time / (24 * 3600);
		dhcp.default_lease_time -= days * 24 * 3600;
		int hours = dhcp.default_lease_time / 3600;
		dhcp.default_lease_time -= hours * 3600;
		int minutes = dhcp.default_lease_time / 60;
		dhcp.default_lease_time -= minutes * 60;
		int seconds = dhcp.default_lease_time;
		fprintf(out, " default-lease-time %d %d %d %d\n", days, hours, minutes, seconds);
	}

	if (dhcp.max_lease_time) {
		int days = dhcp.max_lease_time / (24 * 3600);
		dhcp.max_lease_time -= days * 24 * 3600;
		int hours = dhcp.max_lease_time / 3600;
		dhcp.max_lease_time -= hours * 3600;
		int minutes = dhcp.max_lease_time / 60;
		dhcp.max_lease_time -= minutes * 60;
		int seconds = dhcp.max_lease_time;
		fprintf(out, " max-lease-time %d %d %d %d\n", days, hours, minutes, seconds);
	}

	if (dhcp.default_router)
		fprintf(out, " router %s\n", dhcp.default_router);

	if (dhcp.enable)
		fprintf(out, " enable\n");
	else
		fprintf(out, " no enable\n");

	librouter_dhcp_server_free_config(&dhcp);
	fprintf(out, "!\n");
}

void librouter_config_ip_dump_servers(FILE *out)
{
	char buf[2048];
	int dhcp_mode = librouter_dhcp_get_status();

	_dump_dhcp_server(out);

	if (dhcp_mode == DHCP_RELAY) {
		if (librouter_dhcp_get_relay(buf) == 0)
			fprintf(out, "ip dhcp relay %s\n", buf);
	} else
		fprintf(out, "no ip dhcp relay\n");

	fprintf(out, "%sip dns relay\n", librouter_exec_check_daemon(DNS_DAEMON) ? "" : "no ");

	fprintf(out, "%sip domain lookup\n", librouter_dns_domain_lookup_enabled() ? "" : "no ");

	librouter_dns_dump_nameservers(out);

#ifdef OPTION_HTTP
	fprintf(out, "%sip http server\n", librouter_exec_check_daemon(HTTP_DAEMON) ? "" : "no ");
#endif

#ifdef OPTION_HTTPS
	fprintf(out, "%sip https server\n", librouter_exec_check_daemon(HTTPS_DAEMON) ? "" : "no ");
#endif

#ifdef OPTION_PIMD
	librouter_pim_dump(out);
#endif

#ifdef OPTION_OPENSSH
	fprintf(out, "%sip ssh server\n", librouter_exec_check_daemon(SSH_DAEMON) ? "" : "no ");
#else
	fprintf (out, "%sip ssh server\n", librouter_exec_get_inetd_program(SSH_DAEMON) ? "" : "no ");
#endif

	fprintf(out, "%sip telnet server\n",
	                librouter_exec_get_inetd_program(TELNET_DAEMON) ? "" : "no ");
	fprintf(out, "!\n");
}

void librouter_config_arp_dump(FILE *out)
{
	FILE *F;
	char *ipaddr;
	char *hwaddr;
	char *type;
	char *osdev;
	long flags;
	arglist *args;
	int print_something = 0;
	char tbuf[128];

	F = fopen("/proc/net/arp", "r");

	if (!F) {
		printf("%% Unable to read ARP table\n");
		return;
	}

	fgets(tbuf, 127, F);

	while (!feof(F)) {

		tbuf[0] = 0;

		fgets(tbuf, 127, F);

		tbuf[127] = 0;

		librouter_str_striplf(tbuf);

		args = librouter_make_args(tbuf);
		if (args->argc >= 6) {
			ipaddr = args->argv[0];
			hwaddr = args->argv[3];
			type = args->argv[1];
			osdev = args->argv[5];
			flags = strtoul(args->argv[2], 0, 16);

			/* permanent entry */
			if (flags & ATF_PERM) {
				fprintf(out, "arp %s %s\n", ipaddr, hwaddr);
				print_something = 1;
			}
		}

		librouter_destroy_args(args);
	}

	if (print_something)
		fprintf(out, "!\n");
}

/*************************/
/* Interface information */
/*************************/

static void _dump_policy_interface(FILE *out, char *intf)
{
#ifdef OPTION_QOS
	intf_qos_cfg_t *cfg;

	/* Skip sub-interfaces, except frame-relay dlci's */
	if (strchr(intf, '.') && strncmp(intf, "serial", 6))
		return;

	/* If qos file does not exist, create one and show default values*/
	if (librouter_qos_get_interface_config(intf, &cfg) <= 0) {
		librouter_qos_create_interface_config(intf);
		if (librouter_qos_get_interface_config(intf, &cfg) <= 0)
			return;
	}

	if (cfg) {
		fprintf(out, " bandwidth %dkbps\n", cfg->bw / 1024);
		fprintf(out, " max-reserved-bandwidth %d\n", cfg->max_reserved_bw);

		if (cfg->pname[0] != 0)
			fprintf(out, " service-policy %s\n", cfg->pname);

		librouter_qos_release_config(cfg);
	}
#endif
}

static void _dump_intf_iptables_config(FILE *out, struct interface_conf *conf)
{
	struct iptables_t *ipt = &conf->ipt;

#ifdef OPTION_FIREWALL
	if (ipt->in_acl[0])
		fprintf(out, " ip access-group %s in\n", ipt->in_acl);

	if (ipt->out_acl[0])
		fprintf(out, " ip access-group %s out\n", ipt->out_acl);
#endif
#ifdef OPTION_QOS
	if (ipt->in_mangle[0])
		fprintf(out, " ip mark %s in\n", ipt->in_mangle);

	if (ipt->out_mangle[0])
		fprintf(out, " ip mark %s out\n", ipt->out_mangle);
#endif
#ifdef OPTION_NAT
	if (ipt->in_nat[0])
		fprintf(out, " ip nat %s in\n", ipt->in_nat);

	if (ipt->out_nat[0])
		fprintf(out, " ip nat %s out\n", ipt->out_nat);
#endif
}

static void _dump_intf_secondary_ipaddr_v6_config(FILE *out, struct interfacev6_conf *conf)
{
	int i;
	struct ipv6_t *ipv6 = &conf->sec_ip[0];

	/* Go through IP configuration */
	for (i = 0; i < MAX_NUM_IPS; i++, ipv6++) {

		if (ipv6->ipv6addr[0] == 0)
			break;

		fprintf(out, " ip address %s/%s secondary\n", ipv6->ipv6addr, ipv6->ipv6mask);
	}
}

static void _dump_intf_ipaddr_v6_config(FILE *out, struct interfacev6_conf *conf)
{
	int i;
	struct ipv6_t *ipv6 = &conf->main_ip[0];
	char *dev = librouter_ip_ethernet_get_dev(conf->name); /* ethernet enslaved by bridge? */

	if (!strcmp(conf->name, dev)) {
		if (ipv6->ipv6addr[0] == 0){
			fprintf(out, " no ip address\n");
		}
		else
			for (i = 0; i < MAX_NUM_IPS; i++, ipv6++) {
				if (ipv6->ipv6addr[0])
					fprintf(out, " ipv6 address %s %s \n", ipv6->ipv6addr, ipv6->ipv6mask);
			}
	}
#if NOT_YET_IMPLEMENTED
	else {
		struct ipa_t ip;
		librouter_br_get_ipaddr(dev, &ip);

		if (ip.addr[0])
			fprintf(out, " ip address %s %s\n", ip.addr, ip.mask);
		else
			fprintf(out, " no ip address\n");
	}
#endif
}

static void _dump_intf_secondary_ipaddr_config(FILE *out, struct interface_conf *conf)
{
	int i;
	struct ip_t *ip = &conf->sec_ip[0];

	/* Go through IP configuration */
	for (i = 0; i < MAX_NUM_IPS; i++, ip++) {

		if (ip->ipaddr[0] == 0)
			break;

		fprintf(out, " ip address %s %s secondary\n", ip->ipaddr, ip->ipmask);
	}
}

static void _dump_intf_ipaddr_config(FILE *out, struct interface_conf *conf)
{
	struct ip_t *ip = &conf->main_ip;
	char *dev = librouter_ip_ethernet_get_dev(conf->name); /* ethernet enslaved by bridge? */

	if (!strcmp(conf->name, dev)) {
		if (ip->ipaddr[0])
			fprintf(out, " ip address %s %s\n", ip->ipaddr, ip->ipmask);
		else
			fprintf(out, " no ip address\n");
	} else {
		struct ipa_t ip;
		librouter_br_get_ipaddr(dev, &ip);

		if (ip.addr[0])
			fprintf(out, " ip address %s %s\n", ip.addr, ip.mask);
		else
			fprintf(out, " no ip address\n");
	}
}

static void _dump_vlans(FILE *out, struct interface_conf *conf)
{
	char *osdev = conf->name;
	char devtmp[32];
	int i;

	/* search for VLAN */
	strncpy(devtmp, osdev, 14);
	strcat(devtmp, ".");
	for (i = 0; i < MAX_NUM_LINKS; i++) {
		if (strncmp(conf->info->link[i].ifname, devtmp, strlen(devtmp)) == 0) {
			fprintf(out, " vlan %s\n", conf->info->link[i].ifname + strlen(devtmp));
		}
	}
}

static void _dump_interface_bridge(FILE *out, char *intf)
{
	int n;

	for (n = 0; n <= MAX_BRIDGE; n++) {
		char brname[32];
		sprintf(brname, "%s%d", BRIDGE_NAME, n);
		if (librouter_br_checkif(brname, intf))
			fprintf(out, " bridge-group %d\n", n);
	}
}
static void _dump_ethernet_config(FILE *out, struct interface_conf *conf, struct interfacev6_conf *conf_v6)
{
	/* Interface name (linux name) */
	char *osdev = conf->name;
	/* Ethernet index */
	int ether_no;
	/* Sub-interface index */
	int minor;
	char daemon_dhcpc[32];
	char devtmp[32];
	int i;
	char *p;

	ether_no = atoi(osdev + strlen(ETHERNETDEV));

	/* skip '.' */
	if ((p = strchr(osdev, '.')) != NULL)
		minor = atoi(p + 1);

	/* Don't allow DHCP for sub-interfaces */
	if (minor)
		daemon_dhcpc[0] = 0;
	else
		sprintf(daemon_dhcpc, DHCPC_DAEMON, osdev);

	/* Dump iptables configuration */
	_dump_intf_iptables_config(out, conf);

#ifdef OPTION_PIMD
	librouter_pim_dump_interface(out, osdev);
#endif
	/* Dump QoS */
	_dump_policy_interface(out, osdev);

#ifdef OPTION_BRIDGE
	/* Dump Bridge */
	_dump_interface_bridge(out, osdev);
#endif

	/* Dump Quagga */
	librouter_config_rip_dump_interface(out, osdev);
	librouter_config_ospf_dump_interface(out, osdev);

	/* Print main IP address */
	if (strlen(daemon_dhcpc) && librouter_exec_check_daemon(daemon_dhcpc))
		fprintf(out, " ip address dhcp\n");
	else
		_dump_intf_ipaddr_config(out, conf);

	/* Print secondary IP addresses */
	_dump_intf_secondary_ipaddr_config(out, conf);

	/* Print main IPv6 address */
#if 0
	if (strlen(daemon_dhcpc) && librouter_exec_check_daemon(daemon_dhcpc))
		fprintf(out, " ip address dhcp\n");
	else
#endif
		_dump_intf_ipaddr_v6_config(out, conf_v6);

	if (conf->mtu)
		fprintf(out, " mtu %d\n", conf->mtu);

#ifdef CONFIG_DEVELOPMENT
	if (conf->txqueue)
		fprintf(out, " txqueuelen %d\n", conf->txqueue);
#endif

	_dump_vlans(out, conf);

	/* Show line status if main interface. Avoid VLANs ... */
	if (!conf->is_subiface && librouter_phy_probe(conf->name)) {
		struct lan_status st;
		int phy_status;

		phy_status = librouter_lan_get_status(conf->name, &st);

		if (phy_status < 0)
			return;

		if (st.autoneg)
			fprintf(out, " speed auto\n");
		else
			fprintf(out, " speed %d %s\n", st.speed, st.duplex ? "full" : "half");
	}


#ifdef OPTION_VRRP
	dump_vrrp_interface(out, osdev);
#endif

	/* Finally, return if interface is on or off */
	fprintf(out, " %sshutdown\n", conf->up ? "no " : "");

#ifdef OPTION_MANAGED_SWITCH
#if defined(OPTION_SWITCH_MICREL_KSZ8863)
	if (!strcmp(osdev, KSZ8863_ETH_IFACE)) /* Check for the right network interface */
		librouter_ksz8863_dump_config(out);
#elif defined(OPTION_SWITCH_MICREL_KSZ8895)
	if (!strcmp(osdev, KSZ8895_ETH_IFACE)) /* Check for the right network interface */
		librouter_ksz8895_dump_config(out);
#elif defined(OPTION_SWITCH_BROADCOM)
	if (!strcmp(osdev, BCM53115S_ETH_IFACE)) /* Check for the right network interface */
		librouter_bcm53115s_dump_config(out);
#endif
#endif /* OPTION_MANAGED_SWITCH */
	return;
}

static void _dump_efm_config(FILE *out, struct interface_conf *conf, struct interfacev6_conf *conf_v6)
{
	/* Interface name (linux name) */
	char *osdev = conf->name;
	/* Ethernet index */
	int ether_no;
	/* Sub-interface index */
	int minor;
	char daemon_dhcpc[32];
	char devtmp[32];
	int i;
	char *p;

	ether_no = atoi(osdev + strlen(ETHERNETDEV));

	/* skip '.' */
	if ((p = strchr(osdev, '.')) != NULL)
		minor = atoi(p + 1);

	/* Don't allow DHCP for sub-interfaces */
	if (minor)
		daemon_dhcpc[0] = 0;
	else
		sprintf(daemon_dhcpc, DHCPC_DAEMON, osdev);

	/* Dump iptables configuration */
	_dump_intf_iptables_config(out, conf);

#ifdef OPTION_PIMD
	librouter_pim_dump_interface(out, osdev);
#endif
	/* Dump QoS */
	_dump_policy_interface(out, osdev);

#ifdef OPTION_BRIDGE
	/* Dump Bridge */
	_dump_interface_bridge(out, osdev);
#endif

	/* Dump Quagga */
	librouter_config_rip_dump_interface(out, osdev);
	librouter_config_ospf_dump_interface(out, osdev);

	/* Print main IP address */
	if (strlen(daemon_dhcpc) && librouter_exec_check_daemon(daemon_dhcpc))
		fprintf(out, " ip address dhcp\n");
	else
		_dump_intf_ipaddr_config(out, conf);

	/* Print secondary IP addresses */
	_dump_intf_secondary_ipaddr_config(out, conf);

	/* Print main IPv6 address */
#if 0
	if (strlen(daemon_dhcpc) && librouter_exec_check_daemon(daemon_dhcpc))
		fprintf(out, " ipv6 address dhcp\n");
	else
#endif
		_dump_intf_ipaddr_v6_config(out, conf_v6);

	if (conf->mtu)
		fprintf(out, " mtu %d\n", conf->mtu);

#ifdef CONFIG_DEVELOPMENT
	if (conf->txqueue)
		fprintf(out, " txqueuelen %d\n", conf->txqueue);
#endif

#ifdef OPTION_EFM
	if (!conf->is_subiface) {
		struct orionplus_conf conf;

		librouter_efm_get_mode(&conf);
		if (conf.mode) {
			fprintf(out, " mode cpe\n");
		} else {
			fprintf(out, " mode co %s %d\n",
					conf.modulation == GTI_16_TCPAM_MODE ? "TCPAM-16" : "TCPAM-32",
					conf.linerate);

		}
	}
#endif

	_dump_vlans(out, conf);


#ifdef OPTION_VRRP
	dump_vrrp_interface(out, osdev);
#endif

	/* Finally, return if interface is on or off */
	fprintf(out, " %sshutdown\n", conf->up ? "no " : "");

	return;
}

static void _dump_loopback_config(FILE *out, struct interface_conf *conf, struct interfacev6_conf *conf_v6)
{
	char *osdev = conf->name; /* Interface name (linux name) */

	_dump_intf_iptables_config(out, conf);

	_dump_intf_ipaddr_config(out, conf);

	/* Search for aliases */
	_dump_intf_secondary_ipaddr_config(out, conf);

	_dump_intf_ipaddr_v6_config(out, conf_v6);

	fprintf(out, " %sshutdown\n", conf->up ? "no " : "");
}

#ifdef OPTION_TUNNEL
static void _dump_tunnel_config(FILE *out, struct interface_conf *conf, struct interfacev6_conf *conf_v6)
{
	char *osdev = conf->name;

	/* Dump iptables configuration */
	_dump_intf_iptables_config(out, conf);

	librouter_config_rip_dump_interface(out, osdev);
	librouter_config_ospf_dump_interface(out, osdev);

	_dump_intf_ipaddr_config(out, conf);

	/* as */
	_dump_intf_secondary_ipaddr_config(out, conf);

	_dump_intf_ipaddr_v6_config(out, conf_v6);

	if (conf->mtu)
	fprintf(out, " mtu %d\n", conf->mtu);

#ifdef CONFIG_DEVELOPMENT
	if (conf->txqueue)
		fprintf(out, " txqueuelen %d\n", conf->txqueue);
#endif

	librouter_tunnel_dump_interface(out, 1, osdev);

	fprintf(out, " %sshutdown\n", conf->up ? "no " : "");
}
#endif

#if defined (OPTION_PPP)
static void _dump_ppp_config(FILE *out, struct interface_conf *conf)
{
	ppp_config cfg;
	char *osdev = conf->name;
	int serial_no;
	char *intf_back=NULL;
#if defined(OPTION_MODEM3G)
#if defined (CONFIG_DIGISTAR_3G)
	int simcard_main_alias = 0, simcard_bckp_alias = 0;
#endif
#endif

	/* Get interface index */
	serial_no = atoi(osdev + strlen(PPPDEV));

	librouter_ppp_get_config(serial_no, &cfg);

	_dump_intf_iptables_config(out, conf);
	_dump_policy_interface(out, osdev);

	librouter_config_rip_dump_interface(out, osdev);
	librouter_config_ospf_dump_interface(out, osdev);

#if defined(OPTION_MODEM3G)
#if defined (CONFIG_DIGISTAR_3G)
	if (serial_no != BTIN_M3G_ALIAS) {
		if (strcmp(cfg.sim_main.apn, "") != 0)
			fprintf(out, " apn set %s\n", cfg.sim_main.apn);

		if (strcmp(cfg.sim_main.username, "") != 0)
			fprintf(out, " username set %s\n", cfg.sim_main.username);

		if (strcmp(cfg.sim_main.password, "") != 0)
			fprintf(out, " password set %s\n", cfg.sim_main.password);

	} else {
		simcard_main_alias = librouter_modem3g_sim_get_aliasport_by_realport(cfg.sim_main.sim_num);
		simcard_bckp_alias = librouter_modem3g_sim_get_aliasport_by_realport(cfg.sim_backup.sim_num);

		if (strcmp(cfg.sim_main.apn, "") != 0)
			fprintf(out, " sim %d apn set %s\n", simcard_main_alias, cfg.sim_main.apn);

		if (strcmp(cfg.sim_main.username, "") != 0)
			fprintf(out, " sim %d username set %s\n", simcard_main_alias,
							cfg.sim_main.username);

		if (strcmp(cfg.sim_main.password, "") != 0)
			fprintf(out, " sim %d password set %s\n", simcard_main_alias,
							cfg.sim_main.password);

		if (strcmp(cfg.sim_backup.apn, "") != 0)
			fprintf(out, " sim %d apn set %s\n", simcard_bckp_alias,
			                cfg.sim_backup.apn);

		if (strcmp(cfg.sim_backup.username, "") != 0)
			fprintf(out, " sim %d username set %s\n", simcard_bckp_alias,
			                cfg.sim_backup.username);

		if (strcmp(cfg.sim_backup.password, "") != 0)
			fprintf(out, " sim %d password set %s\n", simcard_bckp_alias,
			                cfg.sim_backup.password);

		if (strcmp(cfg.sim_main.apn, "") != 0)
			if (librouter_modem3g_sim_order_is_enable())
				fprintf(out, " sim-order %d %d\n", librouter_modem3g_sim_get_aliasport_by_realport(librouter_modem3g_sim_order_get_mainsim()),
						librouter_modem3g_sim_get_aliasport_by_realport(!librouter_modem3g_sim_order_get_mainsim()));
			else
				fprintf(out, " sim-order %d\n", librouter_modem3g_sim_get_aliasport_by_realport(librouter_modem3g_sim_order_get_mainsim()));
	}

#elif defined(CONFIG_DIGISTAR_EFM)
	if (strcmp(cfg.sim_main.apn, "") != 0)
		fprintf(out, " apn set %s\n", cfg.sim_main.apn);
	if (strcmp(cfg.sim_main.username, "") != 0)
		fprintf(out, " username set %s\n", cfg.sim_main.username);
	if (strcmp(cfg.sim_main.password, "") != 0)
		fprintf(out, " password set %s\n", cfg.sim_main.password);
#endif

	if (cfg.bckp_conf.method == BCKP_METHOD_LINK)
		fprintf(out, " backup-method link\n");
	else
		fprintf(out, " backup-method ping %s\n", cfg.bckp_conf.ping_address);
	if (cfg.bckp_conf.is_backup){
		intf_back = malloc(strlen(cfg.bckp_conf.main_intf_name));
		snprintf(intf_back,strlen(cfg.bckp_conf.main_intf_name),"%s",cfg.bckp_conf.main_intf_name);
		fprintf(out, " backup-interface %s %c\n", intf_back,cfg.bckp_conf.main_intf_name[strlen(cfg.bckp_conf.main_intf_name)-1]);
		free (intf_back);
	}
	else
		fprintf(out, " no backup-interface\n");

	if (cfg.bckp_conf.is_default_gateway)
		fprintf(out, " default-gateway %d\n", cfg.bckp_conf.default_route_distance);
	else
		fprintf(out, " no default-gateway\n");

	fprintf(out, " %sshutdown\n", cfg.up ? "no " : "");
#endif
}
#endif /*OPTION_PPP */

#ifdef OPTION_PPTP
static void _dump_pptp_config(FILE * out, struct interface_conf *conf)
{
	char *osdev = conf->name;
	int serial_no;

	/* Get interface index */
	serial_no = atoi(osdev + strlen(PPPDEV));

	_dump_intf_iptables_config(out, conf);
	_dump_policy_interface(out, osdev);

	librouter_config_rip_dump_interface(out, osdev);
	librouter_config_ospf_dump_interface(out, osdev);

	librouter_pptp_dump(out);
}
#endif

#ifdef OPTION_PPPOE
static void _dump_pppoe_config(FILE * out, struct interface_conf *conf)
{
	char *osdev = conf->name;
	int serial_no;

	/* Get interface index */
	serial_no = atoi(osdev + strlen(PPPDEV));

	_dump_intf_iptables_config(out, conf);
	_dump_policy_interface(out, osdev);

	librouter_config_rip_dump_interface(out, osdev);
	librouter_config_ospf_dump_interface(out, osdev);

	librouter_pppoe_dump(out);
}
#endif

/**
 * Show interface configuration
 *
 * @param out File descriptor to be written
 * @param conf Interface information
 */
void librouter_config_dump_interface(FILE *out, struct interface_conf *conf, struct interfacev6_conf *conf_v6)
{
	char *description;
	char *cish_dev;
	char pppid[10];
	int txqueuelen;

	/* Get iptables config */

#ifdef OPTION_FIREWALL
	librouter_acl_get_iface_rules(conf->name, conf->ipt.in_acl, conf->ipt.out_acl);
#endif
#ifdef OPTION_QOS
	librouter_mangle_get_iface_rules(conf->name, conf->ipt.in_mangle, conf->ipt.out_mangle);
#endif
#ifdef OPTION_NAT
	librouter_nat_get_iface_rules(conf->name, conf->ipt.in_nat, conf->ipt.out_nat);
#endif

	cish_dev = librouter_device_linux_to_cli(conf->name, 0);

	/* skip ipsec ones... */
	if (conf->linktype == ARPHRD_TUNNEL6)
		return;

	fprintf(out, "interface %s\n", cish_dev);
	description = librouter_dev_get_description(conf->name);

	if (description)
		fprintf(out, " description %s\n", description);

	conf->txqueue = librouter_dev_get_qlen(conf->name);

	switch (conf->linktype) {
#ifdef OPTION_PPP
	case ARPHRD_PPP:
#ifdef OPTION_PPTP
		if (strstr(cish_dev, "pptp"))
			_dump_pptp_config(out, conf);
#endif
#ifdef OPTION_PPPOE
		if (strstr(cish_dev, "pppoe"))
			_dump_pppoe_config(out, conf);
#endif
		if (strstr(cish_dev, "m3G"))
			_dump_ppp_config(out, conf);
		break;
#endif
	case ARPHRD_ETHER:
#ifdef OPTION_EFM
		if (strstr(cish_dev, "efm"))
			_dump_efm_config(out, conf, conf_v6);
		else
#endif
		_dump_ethernet_config(out, conf, conf_v6);
		break;
	case ARPHRD_LOOPBACK:
		_dump_loopback_config(out, conf, conf_v6);
		break;
#ifdef OPTION_TUNNEL
		case ARPHRD_SIT:
		case ARPHRD_TUNNEL:
		case ARPHRD_IPGRE:
		_dump_tunnel_config (out, conf, conf_v6);
		break;
#endif
	default:
		printf("%% unknown link type: %d\n", conf->linktype);
		break;
	}

	/*  Generates configuration about the send of traps for every interface */
	if (librouter_snmp_itf_should_sendtrap(conf->name))
		fprintf(out, " snmp trap link-status\n");

	/* End of interface configuration */
	fprintf(out, "!\n");
}

void librouter_config_interfaces_dump(FILE *out)
{
	int i, ret;
	struct ip_t ip;

	char *cish_dev;
	char mac_bin[6], *description, devtmp[17];
	struct net_device_stats *st;

	struct interface_conf conf;
	struct interfacev6_conf conf_v6;
	struct intf_info info;

	char *intf_list[MAX_NUM_LINKS];

	/* Get interface names */
	librouter_ip_get_if_list(&info);
	for (i = 0; i < MAX_NUM_LINKS; i++)
		intf_list[i] = info.link[i].ifname;

	for (i = 0; intf_list[i][0] != '\0'; i++) {

		if ( librouter_ip_iface_get_config(intf_list[i], &conf, &info) < 0){
			syslog(LOG_INFO, "%s not found in ip\n", intf_list[i]);
			continue;
		}

		if ( librouter_ipv6_iface_get_config(intf_list[i], &conf_v6, NULL) < 0)
			syslog(LOG_INFO, "%s not found in ipv6\n", intf_list[i]);

		/* Ignore the following interfaces: bridge*/
		if (strstr(conf.name, "bridge"))
			continue;

		st = conf.stats;

		cish_dev = librouter_device_linux_to_cli(conf.name, 1);

		/* Ignore if device is not recognized by CISH */
		if (cish_dev == NULL)
			continue;

		/* !!! change crypto-? linktype (temp!) */
		if (strncmp(conf.name, "ipsec", 5) == 0)
			conf.linktype = ARPHRD_TUNNEL6;

		conf.running = (conf.flags & IFF_RUNNING) ? 1 : 0;

		/* Ignore loopbacks that are down */
		if (conf.linktype == ARPHRD_LOOPBACK && !conf.running)
			continue;

		/* Start dumping information */
		librouter_config_dump_interface(out, &conf, &conf_v6);

		librouter_ip_iface_free_config(&conf);
	}
}

#ifdef OPTION_PBR
void librouter_config_dump_policy_route(FILE *f)
{
	librouter_pbr_dump(f);
}
#endif

/********************************/
/* End of Interface information */
/********************************/

/**
 * Write all configuration to file
 *
 * All router configuration is written to filename
 *
 * @param filename Open file descriptor
 * @param cfg Cish configuration struct
 * @return
 */
int librouter_config_write(char *filename, struct router_config *cfg)
{
	FILE * f;

	f = fopen(filename, "wt");
	if (!f)
		return -1;

	fprintf(f, "!\n");
	librouter_config_dump_version(f, cfg);
	librouter_config_dump_terminal(f, cfg);
	librouter_config_dump_secret(f, cfg);
	librouter_config_dump_aaa(f, cfg);
	librouter_config_dump_hostname(f);
	librouter_config_dump_log(f);
#ifdef OPTION_BGP
	librouter_config_bgp_dump_router(f, 0);
#endif
#ifdef OPTION_ROUTER
	librouter_config_dump_ip(f, 1);
#ifdef OPTION_IPV6
	librouter_config_dump_ipv6(f, 1);
#endif
#endif

	/* SNMP */
	librouter_config_dump_snmp(f, 1);
#ifdef OPTION_RMON
	librouter_config_dump_rmon(f);
#endif
#ifdef OPTION_BRIDGE
	librouter_br_dump_bridge(f);
#endif
	librouter_config_dump_chatscripts(f);

	/* iptables */
#ifdef OPTION_FIREWALL
	librouter_acl_dump_policy(f);
	librouter_acl_dump(0, f, 1);
#endif
#ifdef OPTION_NAT
	librouter_nat_dump(0, f, 1);
#endif
#ifdef OPTION_QOS
	librouter_mangle_dump(0, f, 1);
	librouter_qos_dump_config(f);
#endif

#ifdef OPTION_NAT
	librouter_nat_dump_helper(f, cfg);
#endif

	/* Quagga */
	librouter_config_rip_dump_router(f);
	librouter_config_ospf_dump_router(f);
#ifdef OPTION_BGP
	librouter_config_bgp_dump_router(f, 1);
#endif
	librouter_config_dump_routing(f);
#ifdef OPTION_IPV6
	librouter_config_dump_routing_ipv6(f);
#endif

	/* Multicast */
#ifdef OPTION_SMCROUTE
	librouter_smc_route_dump(f);
#endif
	librouter_config_interfaces_dump(f);
	librouter_config_clock_dump(f);
	librouter_ntp_dump(f);
	librouter_config_ip_dump_servers(f);
	librouter_config_arp_dump(f);
#ifdef OPTION_IPSEC
	librouter_ipsec_dump(f);
#endif

/* É Necessario esperar as interfaces serem montadas para
 * realizar o dump do policy route */
#ifdef OPTION_PBR
	librouter_config_dump_policy_route(f);
#endif

	fclose(f);
}
