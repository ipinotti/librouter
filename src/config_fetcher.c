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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <net/if_arp.h>
#include <linux/mii.h>

#include "options.h"
#include "defines.h"
#include "cish_defines.h"
#include "pam.h"
#include "device.h"
#include "quagga.h"
#include "snmp.h"
#include "ppp.h"
#include "exec.h"
#include "smcroute.h"
#include "ntp.h"
#include "dhcp.h"
#include "dns.h"
#include "args.h"
#include "ip.h"
#include "dev.h"
#include "qos.h"

#define PPPDEV "ppp"

#define CISH_CFG_FILE "/var/run/cish_cfg"

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

/***********************/
/* Main dump functions */
/***********************/
void libconfig_config_dump_version(FILE *f, cish_config *cish_cfg)
{
	fprintf(f, "version %s\n", libconfig_get_system_version());
	fprintf(f, "!\n");
}

void libconfig_config_dump_terminal(FILE *f, cish_config *cish_cfg)
{
	fprintf(f, "terminal length %d\n", cish_cfg->terminal_lines);
	fprintf(f, "terminal timeout %d\n", cish_cfg->terminal_timeout);
	fprintf(f, "!\n");
}

void libconfig_config_dump_secret(FILE *f, cish_config *cish_cfg)
{
	int printed_something = 0;

	if (cish_cfg->enable_secret[0]) {
		fprintf(f, "secret enable hash %s\n", cish_cfg->enable_secret);
		printed_something = 1;
	}

	if (cish_cfg->login_secret[0]) {
		fprintf(f, "secret login hash %s\n", cish_cfg->login_secret);
		printed_something = 1;
	}

	if (printed_something)
		fprintf(f, "!\n");
}

void libconfig_config_dump_aaa(FILE *f, cish_config *cish_cfg)
{
	int i;
	FILE *passwd;

	/* Dump RADIUS & TACACS servers */
	for (i = 0; i < MAX_SERVERS; i++) {
		if (cish_cfg->radius[i].ip_addr[0]) {

			fprintf(f, "radius-server host %s", cish_cfg->radius[i].ip_addr);

			if (cish_cfg->radius[i].authkey[0])
				fprintf(f, " key %s", cish_cfg->radius[i].authkey);

			if (cish_cfg->radius[i].timeout)
				fprintf(f, " timeout %d", cish_cfg->radius[i].timeout);

			fprintf(f, "\n");
		}
	}

	for (i = 0; i < MAX_SERVERS; i++) {
		if (cish_cfg->tacacs[i].ip_addr[0]) {

			fprintf(f, "tacacs-server host %s", cish_cfg->tacacs[i].ip_addr);

			if (cish_cfg->tacacs[i].authkey[0])
				fprintf(f, " key %s", cish_cfg->tacacs[i].authkey);

			if (cish_cfg->tacacs[i].timeout)
				fprintf(f, " timeout %d", cish_cfg->tacacs[i].timeout);

			fprintf(f, "\n");
		}
	}

	/* Dump aaa authentication login mode */
	switch (libconfig_pam_get_current_mode(FILE_PAM_GENERIC)) {
	case AAA_AUTH_NONE:
		fprintf(f, "aaa authentication login default none\n");
		break;
	case AAA_AUTH_LOCAL:
		fprintf(f, "aaa authentication login default local\n");
		break;
	case AAA_AUTH_RADIUS:
		fprintf(f, "aaa authentication login group radius\n");
		break;
	case AAA_AUTH_RADIUS_LOCAL:
		fprintf(f, "aaa authentication login group radius local\n");
		break;
	case AAA_AUTH_TACACS:
		fprintf(f, "aaa authentication login default group tacacs+\n");
		break;
	case AAA_AUTH_TACACS_LOCAL:
		fprintf(f, "aaa authentication login default group tacacs+ local\n");
		break;
	default:
		fprintf(f, "aaa authentication login none\n");
		break;
	}

	/* Dump aaa authentication web mode */
	switch (libconfig_pam_get_current_mode(FILE_PAM_WEB)) {
	case AAA_AUTH_NONE:
		fprintf(f, "aaa authentication web default none\n");
		break;
	case AAA_AUTH_LOCAL:
		fprintf(f, "aaa authentication web default local\n");
		break;
	case AAA_AUTH_RADIUS:
		fprintf(f, "aaa authentication web group radius\n");
		break;
	case AAA_AUTH_RADIUS_LOCAL:
		fprintf(f, "aaa authentication web group radius local\n");
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

	/* Dump aaa authorization mode */
	switch (libconfig_pam_get_current_author_mode(FILE_PAM_GENERIC)) {
	case AAA_AUTHOR_NONE:
		fprintf(f, "aaa authorization exec default none\n");
		break;
	case AAA_AUTHOR_TACACS:
		fprintf(f, "aaa authorization exec default group tacacs+\n");
		break;
	case AAA_AUTHOR_TACACS_LOCAL:
		fprintf(f, "aaa authorization exec default group tacacs+ local\n");
		break;
	}

	/* Dump aaa accounting mode */
	switch (libconfig_pam_get_current_acct_mode(FILE_PAM_GENERIC)) {
	case AAA_ACCT_NONE:
		fprintf(f, "aaa accounting exec default none\n");
		break;
	case AAA_ACCT_TACACS:
		fprintf(f, "aaa accounting exec default start-stop group tacacs+\n");
		break;
	}

	switch (libconfig_pam_get_current_acct_cmd_mode(FILE_PAM_GENERIC)) {
	case AAA_ACCT_TACACS_CMD_NONE:
		break;
	case AAA_ACCT_TACACS_CMD_1:
		fprintf(f, "aaa accounting commands 1 default start-stop group tacacs+\n");
		break;
	case AAA_ACCT_TACACS_CMD_15:
		fprintf(f, "aaa accounting commands 15 default start-stop group tacacs+\n");
		break;
	case AAA_ACCT_TACACS_CMD_ALL:
		fprintf(f, "aaa accounting commands 1 default start-stop group tacacs+\n");
		fprintf(f, "aaa accounting commands 15 default start-stop group tacacs+\n");
		break;
	}

	/* Dump users */
	if ((passwd = fopen(FILE_PASSWD, "r"))) {
		struct passwd *pw;

		while ((pw = fgetpwent(passwd))) {
			if (pw->pw_uid > 500 && !strncmp(pw->pw_gecos, "Local", 5)) {
				fprintf(f, "aaa username %s password hash %s\n",
				                pw->pw_name, pw->pw_passwd);
			}
		}

		fclose(passwd);
	}

	fprintf(f, "!\n");
}

void libconfig_config_dump_hostname(FILE *f)
{
	char buf[64];

	gethostname(buf, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;

	fprintf(f, "hostname %s\n!\n", buf);
}

void libconfig_config_dump_log(FILE *f)
{
	char buf[16];

	if (libconfig_exec_get_init_option_value(PROG_SYSLOGD, "-R", buf, 16) >= 0)
		fprintf(f, "logging remote %s\n!\n", buf);
}

void libconfig_config_bgp_dump_router(FILE *f, int main_nip)
{
	FILE *fd;
	char buf[1024];

	if (!libconfig_quagga_bgpd_is_running())
		return;

	/* dump router bgp info */
	fd = libconfig_quagga_bgp_get_conf(main_nip);
	if (fd) {
		while (!feof(fd)) {
			fgets(buf, 1024, fd);

			if (buf[0] == '!')
				break;

			libconfig_str_striplf(buf);
			fprintf(f, "%s\n", libconfig_device_from_linux_cmdline(libconfig_zebra_to_linux_cmdline(buf)));
		}

		fclose(fd);
	}

	fprintf(f, "!\n");
}

void libconfig_config_dump_ip(FILE *f, int conf_format)
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

#if 0 /* This are not present in earlier kernel versions ... is this PD3 invention ? */
	val = _get_procip_val ("icmp_destunreach_rate");
	fprintf (f, "ip icmp rate dest-unreachable %i\n", val);

	val = _get_procip_val ("icmp_echoreply_rate");
	fprintf (f, "ip icmp rate echo-reply %i\n", val);

	val = _get_procip_val ("icmp_paramprob_rate");
	fprintf (f, "ip icmp rate param-prob %i\n", val);

	val = _get_procip_val ("icmp_timeexceed_rate");
	fprintf (f, "ip icmp rate time-exceed %i\n", val);
#endif

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

void libconfig_config_dump_snmp(FILE *f, int conf_format)
{
	int print = 0;
	int len;
	char **sinks;
	char buf[512];

	if (libconfig_snmp_get_contact(buf, 511) == 0) {
		print = 1;
		fprintf(f, "snmp-server contact %s\n", buf);
	}

	if (libconfig_snmp_get_location(buf, 511) == 0) {
		print = 1;
		fprintf(f, "snmp-server location %s\n", buf);
	}

	if ((len = libconfig_snmp_get_trapsinks(&sinks)) > 0) {
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

	if (libconfig_snmp_dump_communities(f) > 0)
		print = 1;

	if (libconfig_snmp_is_running()) {
		print = 1;
		libconfig_snmp_dump_versions(f);
	}

	if (print)
		fprintf(f, "!\n");
}

#ifdef OPTION_RMON
void libconfig_config_dump_rmon(FILE *f)
{
	int i, k;
	struct rmon_config *shm_rmon_p;
	char tp[10], result[MAX_OID_LEN * 10];

	if (libconfig_snmp_rmon_get_access_cfg(&shm_rmon_p) == 1) {
		for (i = 0; i < NUM_EVENTS; i++) {
			if (shm_rmon_p->events[i].index > 0) {
				fprintf(f, "rmon event %d",shm_rmon_p->events[i].index);

				if (shm_rmon_p->events[i].do_log)
					fprintf(f, " log");

				if (shm_rmon_p->events[i].community[0] != 0)
					fprintf(f, " trap %s",
					                shm_rmon_p->events[i].community);

				if (shm_rmon_p->events[i].description[0] != 0)
					fprintf(f, " description %s",
					                shm_rmon_p->events[i].description);

				if (shm_rmon_p->events[i].owner[0] != 0)
					fprintf(f, " owner %s",
					                shm_rmon_p->events[i].owner);

				fprintf(f, "\n");
			}
		}

		for (i = 0; i < NUM_ALARMS; i++) {
			if (shm_rmon_p->alarms[i].index > 0) {

				result[0] = '\0';

				for (k = 0; k < shm_rmon_p->alarms[i].oid_len; k++) {
					sprintf(tp, "%lu.",
					                shm_rmon_p->alarms[i].oid[k]);
					strcat(result, tp);
				}

				*(result + strlen(result) - 1) = '\0';

				fprintf(f, "rmon alarm %d %s %d",
				                shm_rmon_p->alarms[i].index,
				                result,
				                shm_rmon_p->alarms[i].interval);

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
						fprintf(f, " %d",
						                shm_rmon_p->alarms[i].rising_event_index);
				}

				if (shm_rmon_p->alarms[i].falling_threshold) {

					fprintf(f, " falling-threshold %d",
					                shm_rmon_p->alarms[i].falling_threshold);

					if (shm_rmon_p->alarms[i].falling_event_index)
						fprintf(f, " %d",
						                shm_rmon_p->alarms[i].falling_event_index);
				}

				if (shm_rmon_p->alarms[i].owner[0] != 0)
					fprintf(f, " owner %s",
					                shm_rmon_p->alarms[i].owner);

				fprintf(f, "\n");
			}
		}

		libconfig_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	if (libconfig_exec_check_daemon(RMON_DAEMON))
		fprintf(f, "rmon agent\n");
	else
		fprintf(f, "no rmon agent\n");

	fprintf(f, "!\n");
}
#endif

void libconfig_config_dump_chatscripts(FILE *f)
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

void libconfig_config_ospf_dump_router(FILE *out)
{
	FILE *f;
	char buf[1024];

	if (!libconfig_quagga_ospfd_is_running())
		return;

	/* if config not written */
	fprintf(out, "router ospf\n");

	f = libconfig_quagga_ospf_get_conf(1, NULL);

	if (f) {
		/* skip line */
		fgets(buf, 1024, f);

		while (!feof(f)) {
			fgets(buf, 1024, f);

			if (buf[0] == '!')
				break;

			libconfig_str_striplf(buf);
			fprintf(out, "%s\n",
			                libconfig_device_from_linux_cmdline(libconfig_zebra_to_linux_cmdline(buf)));
		}

		fclose(f);
	}

	fprintf(out, "!\n");
}

void libconfig_config_ospf_dump_interface(FILE *out, char *intf)
{
	FILE *f;
	char buf[1024];

	if (!libconfig_quagga_ospfd_is_running())
		return;

	f = libconfig_quagga_ospf_get_conf(0, intf);

	if (!f)
		return;

	/* skip line */
	fgets(buf, 1024, f);

	while (!feof(f)) {
		fgets(buf, 1024, f);
		if (buf[0] == '!')
			break;

		libconfig_str_striplf(buf);

		fprintf(out, "%s\n", libconfig_device_from_linux_cmdline(libconfig_zebra_to_linux_cmdline(buf)));
	}

	fclose(f);
}

void libconfig_config_rip_dump_router(FILE *out)
{
	FILE *f;
	int end;
	char buf[1024];
	char keychain[] = "key chain";

	if (!libconfig_quagga_ripd_is_running())
		return;

	/* dump router rip info */
	/* if config not written */
	fprintf(out, "router rip\n");

	f = libconfig_quagga_rip_get_conf(1, NULL);

	if (f) {
		/* skip line */
		fgets(buf, 1024, f);

		while (!feof(f)) {

			fgets(buf, 1024, f);

			if (buf[0] == '!')
				break;

			libconfig_str_striplf(buf);
			fprintf(out," %s\n",
			                libconfig_device_from_linux_cmdline(
			                                libconfig_zebra_to_linux_cmdline(buf)));
		}

		fclose(f);
	}

	fprintf(out, "!\n");

	/* dump key info (after router rip!) */
	f = libconfig_quagga_get_conf(RIPD_CONF, keychain);

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

			libconfig_str_striplf(buf);

			fprintf(out, "%s\n", buf);
		}
		fclose(f);
	}
}

void libconfig_config_rip_dump_interface(FILE *out, char *intf)
{
	FILE *f;
	char buf[1024];

	if (!libconfig_quagga_ripd_is_running())
		return;

	f = libconfig_quagga_rip_get_conf(0, intf);

	if (!f)
		return;

	/* skip line */
	fgets(buf, 1024, f);

	while (!feof(f)) {

		fgets(buf, 1024, f);

		if (buf[0] == '!')
			break;

		libconfig_str_striplf(buf);
		fprintf(out, "%s\n", libconfig_device_from_linux_cmdline(
		                libconfig_zebra_to_linux_cmdline(buf)));
	}

	fclose(f);
}

void libconfig_config_dump_routing(FILE *f)
{
	libconfig_quagga_zebra_dump_static_routes(f);
}

void libconfig_config_clock_dump(FILE *out)
{
	int hours, mins;
	char name[16];

	if (get_timezone(name, &hours, &mins) == 0) {
		fprintf(out, "clock timezone %s %d", name, hours);

		if (mins > 0)
			fprintf(out, " %d\n", mins);
		else
			fprintf(out, "\n");

		fprintf(out, "!\n");
	}
}

void libconfig_config_ip_dump_servers(FILE *out)
{
	char buf[2048];
	int dhcp;

	dhcp = libconfig_dhcp_get_status();

	if (dhcp == DHCP_SERVER) {
		if (libconfig_dhcp_get_server(buf) == 0) {
			fprintf(out, "%s\n", buf);
			fprintf(out, "no ip dhcp relay\n");
		}
	} else if (dhcp == DHCP_RELAY) {
		if (libconfig_dhcp_get_relay(buf) == 0) {
			fprintf(out, "no ip dhcp server\n");
			fprintf(out, "ip dhcp relay %s\n", buf);
		}
	} else {
		fprintf(out, "no ip dhcp server\n");
		fprintf(out, "no ip dhcp relay\n");
	}

	fprintf(out, "%sip dns relay\n",
	                libconfig_exec_check_daemon(DNS_DAEMON) ? "" : "no ");

	fprintf(out, "%sip domain lookup\n",
	                libconfig_dns_domain_lookup_enabled() ? "" : "no ");

	libconfig_dns_dump_nameservers(out);

#ifdef OPTION_HTTP
	fprintf(out, "%sip http server\n", libconfig_exec_check_daemon(HTTP_DAEMON) ? "" : "no ");
#endif

#ifdef OPTION_HTTPS
	fprintf(out, "%sip https server\n", libconfig_exec_check_daemon(HTTPS_DAEMON) ? "" : "no ");
#endif

#ifdef OPTION_PIMD
	libconfig_pim_dump(out);
#endif

#ifdef OPTION_OPENSSH
	fprintf(out, "%sip ssh server\n", libconfig_exec_check_daemon(SSH_DAEMON) ? "" : "no ");
#else
	fprintf (out, "%sip ssh server\n", libconfig_exec_get_inetd_program(SSH_DAEMON) ? "" : "no ");
#endif

	fprintf(out, "%sip telnet server\n", libconfig_exec_get_inetd_program(TELNET_DAEMON) ? "" : "no ");
	fprintf(out, "!\n");
}

void libconfig_config_arp_dump(FILE *out)
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

		libconfig_str_striplf(tbuf);

		args = libconfig_make_args(tbuf);
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

		libconfig_destroy_args(args);
	}

	if (print_something)
		fprintf(out, "!\n");
}

/*************************/
/* Interface information */
/*************************/

static void _dump_policy_interface(FILE *out, char *intf)
{
	intf_qos_cfg_t *cfg;

	/* Skip sub-interfaces, except frame-relay dlci's */
	if (strchr(intf, '.') && strncmp(intf, "serial", 6))
		return;

	/* If qos file does not exist, create one and show default values*/
	if (libconfig_qos_get_interface_config(intf, &cfg) <= 0) {
		libconfig_qos_create_interface_config(intf);
		if (libconfig_qos_get_interface_config(intf, &cfg) <= 0)
			return;
	}

	if (cfg) {
		fprintf(out, " bandwidth %dkbps\n", cfg->bw / 1024);
		fprintf(out, " max-reserved-bandwidth %d\n", cfg->max_reserved_bw);

		if (cfg->pname[0] != 0)
			fprintf(out, " service-policy %s\n", cfg->pname);

		libconfig_qos_release_config(cfg);
	}
}

static void _dump_intf_iptables_config(FILE *out, struct interface_conf *conf)
{
	struct iptables_t *ipt = &conf->ipt;

	if (ipt->in_acl[0])
		fprintf(out, " ip access-group %s in\n", ipt->in_acl);

	if (ipt->out_acl[0])
		fprintf(out, " ip access-group %s out\n", ipt->out_acl);

	if (ipt->in_mangle[0])
		fprintf(out, " ip mark %s in\n", ipt->in_mangle);

	if (ipt->out_mangle[0])
		fprintf(out, " ip mark %s out\n", ipt->out_mangle);

	if (ipt->in_nat[0])
		fprintf(out, " ip nat %s in\n", ipt->in_nat);

	if (ipt->out_nat[0])
		fprintf(out, " ip nat %s out\n", ipt->out_nat);
}

static void _dump_intf_secondary_ipaddr_config(FILE *out,
                                               struct interface_conf *conf)
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

	if (ip->ipaddr[0])
		fprintf(out, " ip address %s %s\n", ip->ipaddr, ip->ipmask);
	else
		fprintf(out, " no ip address\n");
}

static void _dump_ethernet_config(FILE *out, struct interface_conf *conf)
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
	libconfig_pim_dump_interface(out, osdev);
#endif
	/* Dump QoS */
	_dump_policy_interface(out, osdev);

	/* Dump Quagga */
	libconfig_config_rip_dump_interface(out, osdev);
	libconfig_config_ospf_dump_interface(out, osdev);

	/* Print main IP address */
	if (strlen(daemon_dhcpc) && libconfig_exec_check_daemon(daemon_dhcpc))
		fprintf(out, " ip address dhcp\n");
	else
		_dump_intf_ipaddr_config(out, conf);

	/* Print secondary IP addresses */
	_dump_intf_secondary_ipaddr_config(out, conf);

	if (conf->mtu)
		fprintf(out, " mtu %d\n", conf->mtu);

	if (conf->txqueue)
		fprintf(out, " txqueuelen %d\n", conf->txqueue);

#if 0
	/* search for VLAN */
	strncpy(devtmp, osdev, 14);
	strcat(devtmp, ".");
	for (i = 0; i < MAX_NUM_LINKS; i++) {
		if (strncmp(conf->info->link[i].ifname, devtmp, strlen(devtmp)) == 0) {
			fprintf(out, " vlan %s\n", conf->info->link[i].ifname
					+ strlen(devtmp));
		}
	}
#endif

	/* Show line status if main interface. Avoid VLANs ... */
	if (strchr(osdev, '.') == NULL) {
		int bmcr;

		bmcr = libconfig_lan_get_phy_reg(osdev, MII_BMCR);

		if (bmcr & BMCR_ANENABLE)
			fprintf(out, " speed auto\n");
		else {
			fprintf(out, " speed %s %s\n",
					(bmcr & BMCR_SPEED100) ? "100" : "10",
			                (bmcr & BMCR_FULLDPLX) ? "full" : "half");
		}
	}

#ifdef OPTION_VRRP
	dump_vrrp_interface(out, osdev);
#endif

	/* Finally, return if interface is on or off */
	fprintf(out, " %sshutdown\n", conf->up ? "no " : "");

	return;
}

static void _dump_loopback_config(FILE *out, struct interface_conf *conf)
{
	char *osdev = conf->name; /* Interface name (linux name) */

	_dump_intf_iptables_config(out, conf);

	_dump_intf_ipaddr_config(out, conf);

	/* Search for aliases */
	_dump_intf_secondary_ipaddr_config(out, conf);

	fprintf(out, " %sshutdown\n", conf->up ? "no " : "");
}

#ifdef OPTION_TUNNEL
static void _dump_tunnel_config(FILE *out, struct interface_conf *conf)
{
	char *osdev = conf->name;

	/* Dump iptables configuration */
	_dump_intf_iptables_config(out, conf);

	libconfig_config_rip_dump_interface(out, osdev);
	libconfig_config_ospf_dump_interface(out, osdev);

	_dump_intf_ipaddr_config(out, conf);

	/* as */
	_dump_intf_secondary_ipaddr_config(out, conf);

	if (conf->mtu)
		fprintf(out, " mtu %d\n", conf->mtu);

	if (conf->txqueue)
		fprintf(out, " txqueuelen %d\n", conf->txqueue);

	libconfig_tunnel_dump_interface(out, 1, osdev);

	fprintf(out, " %sshutdown\n", conf->up ? "no " : "");
}
#endif

#ifdef OPTION_PPP
static void _dump_ppp_config(FILE *out, struct interface_conf *conf)
{
	ppp_config cfg;
	char *osdev = conf->name;
	int serial_no;

	cfg.up = conf->up;

	/* Get interface index */
	serial_no = atoi(osdev + strlen(PPPDEV));

	libconfig_ppp_get_config(serial_no, &cfg);

	_dump_intf_iptables_config(out, conf);
	_dump_policy_interface(out, osdev);

	libconfig_config_rip_dump_interface(out, osdev);
	libconfig_config_ospf_dump_interface(out, osdev);

	fprintf(out, " apn set %s\n", cfg.apn);
	fprintf(out, " username set %s\n", cfg.auth_user);
	fprintf(out, " password set %s\n", cfg.auth_pass);
	fprintf(out, " %sshutdown\n", cfg.up ? "no " : "");

}
#endif

/**
 * Show interface configuration
 *
 * @param out File descriptor to be written
 * @param conf Interface information
 */
static void libconfig_config_dump_interface(FILE *out, struct interface_conf *conf)
{
	struct iptables_t ipt;
	char *description;
	char *cish_dev;

	/* Get iptables config */
	memset(&ipt, 0, sizeof(struct iptables_t));

	libconfig_acl_get_iface_rules(conf->name, ipt.in_acl, ipt.out_acl);
	libconfig_mangle_get_iface_rules(conf->name, ipt.in_mangle, ipt.out_mangle);
	libconfig_nat_get_iface_rules(conf->name, ipt.in_nat, ipt.out_nat);

	cish_dev = libconfig_device_convert_os(conf->name, 0);

	/* skip ipsec ones... */
	if (conf->linktype == ARPHRD_TUNNEL6)
		return;

	fprintf(out, "interface %s\n", cish_dev);
	description = libconfig_dev_get_description(conf->name);

	if (description)
		fprintf(out, " description %s\n", description);

	switch (conf->linktype) {
#ifdef OPTION_PPP
	case ARPHRD_PPP:
		_dump_ppp_config(out, conf);
		break;
#endif
	case ARPHRD_ETHER:
		_dump_ethernet_config(out, conf);
		break;
	case ARPHRD_LOOPBACK:
		_dump_loopback_config(out, conf);
		break;
#ifdef OPTION_TUNNEL
	case ARPHRD_TUNNEL:
	case ARPHRD_IPGRE:
		_dump_tunnel_config (out, conf);
		break;
#endif
	default:
		printf("%% unknown link type: %d\n", conf->linktype);
		break;
	}

	/*  Generates configuration about the send of traps for every interface */
	if (libconfig_snmp_itf_should_sendtrap(conf->name))
		fprintf(out, " snmp trap link-status\n");

	/* End of interface configuration */
	fprintf(out, "!\n");
}

void libconfig_config_interfaces_dump(FILE *out)
{
	int i, ret;
	struct ip_t ip;

	char *cish_dev;
	char mac_bin[6], *description, devtmp[17];
	struct net_device_stats *st;

	struct interface_conf conf;

	char intf_list[32][16] = { "ethernet0", "ethernet1", "loopback0",
	                "ppp0", "ppp1", "ppp2", "\0" };

#if 0
	int vlan_cos=NONE_TO_COS;
#endif

	for (i = 0; intf_list[i][0] != '\0'; i++) {

		if (libconfig_ip_iface_get_config(intf_list[i], &conf) < 0)
			continue;

		st = conf.stats;

		cish_dev = libconfig_device_convert_os(conf.name, 1);

		/* Ignore if device is not recognized by CISH */
		if (cish_dev == NULL)
			continue;

		/* !!! change crypto-? linktype (temp!) */
		if (strncmp(conf.name, "ipsec", 5) == 0)
			conf.linktype = ARPHRD_TUNNEL6;

		/* TODO Check PHY/Switch status for ethernet interfaces */
#if 0
		switch (conf.linktype) {
		case ARPHRD_ETHER:
			phy_status = libconfig_lan_get_status(conf.name);
			/* vlan: interface must be up */
			running = (up && (phy_status & PHY_STAT_LINK) ? 1 : 0);
			/* VLAN */
			if (!strncmp(conf.name,"ethernet",8) && strstr(conf.name,"."))
				vlan_cos = libconfig_vlan_get_cos(conf.name);
			else
				vlan_cos = NONE_TO_COS;
			break;
		default:
			running = (link_table[i].flags & IFF_RUNNING) ? 1 : 0;
			break;
		}
#else
		conf.running = (conf.flags & IFF_RUNNING) ? 1 : 0;
#endif

		/* Ignore loopbacks that are down */
		if (conf.linktype == ARPHRD_LOOPBACK && !conf.running)
			continue;

		/* Start dumping information */
		libconfig_config_dump_interface(out, &conf);

		libconfig_ip_iface_free_config(&conf);
	}
}

/********************************/
/* End of Interface information */
/********************************/

/**
 * Write all configuration to file
 *
 * All router configuration is written to filename
 *
 * @param filename Open file descriptor
 * @param cish_cfg Cish configuration struct
 * @return
 */
int libconfig_config_write(char *filename, cish_config *cish_cfg)
{
	FILE * f;

	f = fopen(filename, "wt");
	if (!f)
		return -1;

	fprintf(f, "!\n");
	libconfig_config_dump_version(f, cish_cfg);
	libconfig_config_dump_terminal(f, cish_cfg);
	libconfig_config_dump_secret(f, cish_cfg);
	libconfig_config_dump_aaa(f, cish_cfg);
	libconfig_config_dump_hostname(f);
	libconfig_config_dump_log(f);
#ifdef OPTION_BGP
	libconfig_config_bgp_dump_router(f, 0);
#endif
	libconfig_config_dump_ip(f, 1);

	/* SNMP */
	libconfig_config_dump_snmp(f, 1);
#ifdef OPTION_RMON
	libconfig_config_dump_rmon(f);
#endif

	libconfig_config_dump_chatscripts(f);

	/* iptables */
	libconfig_acl_dump_policy(f);
	libconfig_acl_dump(0, f, 1);
	libconfig_nat_dump(0, f, 1);
	libconfig_mangle_dump(0, f, 1);

	/* qos */
	libconfig_qos_dump_config(f);

	libconfig_nat_dump_helper(f, cish_cfg);

	/* Quagga */
	libconfig_config_rip_dump_router(f);
	libconfig_config_ospf_dump_router(f);
#ifdef OPTION_BGP
	libconfig_config_bgp_dump_router(f, 1);
#endif
	libconfig_config_dump_routing(f);

	/* Multicast */
#ifdef OPTION_SMCROUTE
	libconfig_smc_route_dump(f);
#endif
	libconfig_config_interfaces_dump(f);
	libconfig_config_clock_dump(f);
	libconfig_ntp_dump(f);
	libconfig_config_ip_dump_servers(f);
	libconfig_config_arp_dump(f);
#ifdef OPTION_IPSEC
	libconfig_ipsec_dump(f);
#endif

	fclose(f);
}

static int _set_default_cfg(void)
{
	FILE *f;
	cish_config cfg;

	memset(&cfg, 0, sizeof(cfg));

	f = fopen(CISH_CFG_FILE, "wb");

	if (!f) {
		libconfig_pr_error(1, "Can't write configuration");
		return (-1);
	}

	fwrite(&cfg, sizeof(cish_config), 1, f);
	fclose(f);

	return 0;
}

static int _check_cfg(void)
{
	struct stat st;

	if (stat(CISH_CFG_FILE, &st))
		return _set_default_cfg();

	return 0;
}

cish_config* libconfig_config_mmap_cfg(void)
{
	int fd;
	cish_config *cish_cfg = NULL;

	_check_cfg();

	if ((fd = open(CISH_CFG_FILE, O_RDWR)) < 0) {
		libconfig_pr_error(1, "Could not open configuration");
		return NULL;
	}

	cish_cfg = mmap(NULL, sizeof(cish_config), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (cish_cfg == ((void *) -1)) {
		libconfig_pr_error(1, "Could not open configuration");
		return NULL;
	}

	close(fd);

	/* debug persistent */
	libconfig_debug_recover_all();

	return cish_cfg;
}

int libconfig_config_munmap_cfg(cish_config *cish_cfg)
{
	return (munmap(cish_cfg, sizeof(cish_config)) < 0);
}
