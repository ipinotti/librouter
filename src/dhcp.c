/*
 * dhcp.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/route.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <sys/mman.h> /*mmap*/
#include <syslog.h> /*syslog*/

#include "options.h"
#include "args.h"
#include "error.h"
#include "exec.h"
#include "defines.h"
#include "dev.h"
#include "ip.h"
#include "ipsec.h"
#include "process.h"
#include "str.h"
#include "typedefs.h"
#include "dhcp.h"

#ifdef UDHCPD
pid_t librouter_udhcpd_pid_by_eth(int eth)
{
	FILE *F;
	pid_t pid;
	char buf[32];
	char filename[64];

	sprintf(filename, FILE_DHCPD_PID_ETH, eth);
	F = fopen(filename, "r");
	if (F) {
		fgets(buf, 32, F);
		pid = (pid_t) atoi(buf);
		fclose(F);
		return pid;
	}
	return (pid_t) -1;
}

int librouter_udhcpd_reload(int eth)
{
	pid_t pid;

	if ((pid = librouter_udhcpd_pid_by_eth(eth)) != -1) {
		kill(pid, SIGHUP);
		return 0;
	}
	return -1;
}

int librouter_udhcpd_kick_by_eth(int eth)
{
	pid_t pid;

	if ((pid = librouter_udhcpd_pid_by_eth(eth)) != -1) {
		kill(pid, SIGUSR1); /* atualiza leases file */
		return 0;
	}
	return -1;
}

pid_t librouter_udhcpd_pid_by_name(char *ifname)
{
	FILE *F;
	pid_t pid;
	char buf[32];
	char filename[64];

	snprintf(filename, 64, FILE_DHCPD_PID_STR, ifname);
	F = fopen(filename, "r");
	if (F) {
		fgets(buf, 32, F);
		pid = (pid_t) atoi(buf);
		fclose(F);
		return pid;
	}
	return (pid_t) -1;
}

int librouter_udhcpd_kick_by_name(char *ifname)
{
	pid_t pid;

	if ((pid = librouter_udhcpd_pid_by_name(ifname)) != -1) {
		kill(pid, SIGUSR1); /* perform renew */
		return 0;
	}
	return -1;
}
#endif

int librouter_dhcp_get_status(void)
{
	int ret = DHCP_NONE;
	char dhcp_server[64];

	sprintf(dhcp_server, DHCPD_DAEMON, INTF_DHCP_SERVER_DEFAULT);

	if (librouter_exec_check_daemon(dhcp_server))
		return DHCP_SERVER;

	if (librouter_exec_check_daemon(DHCRELAY_DAEMON))
		return DHCP_RELAY;

	return DHCP_NONE;
}

int librouter_dhcpd_set_status(int on_off, int eth)
{
	int ret;
	char daemon[64];

	if (on_off) {
		if ((ret = librouter_udhcpd_reload(eth)) != -1)
			return ret;
		sprintf(daemon, DHCPD_DAEMON, eth);
		return librouter_exec_init_program(1, daemon);
	}
	sprintf(daemon, DHCPD_DAEMON, eth);
	ret = librouter_exec_init_program(0, daemon);

	return ret;
}

int librouter_dhcp_set_none(void)
{
	int ret, pid;

	ret = librouter_dhcp_get_status();
	if (ret == DHCP_SERVER) {
		if (librouter_dhcpd_set_status(0, 0) < 0)
			return (-1);
#if 0 /* ifndef UDHCPD */
		pid=librouter_udhcpd_pid_by_eth(?);
		if ((pid > 0) && (librouter_process_wait_for(pid, 6) == 0)) return (-1);
#endif
	}
	if (ret == DHCP_RELAY) {
		if (librouter_exec_init_program(0, DHCRELAY_DAEMON) < 0)
			return (-1);
		pid = librouter_process_get_pid(DHCRELAY_DAEMON);
		if ((pid) && (librouter_process_wait_for(pid, 6) == 0))
			return (-1);
	}
	return 0;
}

int librouter_dhcp_set_no_server(void)
{
#if 0
	int pid;
#endif

	if (librouter_dhcpd_set_status(0, INTF_DHCP_SERVER_DEFAULT) < 0)
		return (-1);
#if 0 /* ifndef UDHCPD */
	pid=librouter_process_get_pid(DHCPD_DAEMON);
	if ((pid)&&(librouter_process_wait_for(pid, 6) == 0)) return (-1);
#endif
	return 0;
}

int librouter_dhcp_set_no_relay(void)
{
	int pid;

	if (librouter_exec_init_program(0, DHCRELAY_DAEMON) < 0)
		return (-1);
	pid = librouter_process_get_pid(DHCRELAY_DAEMON);
	if ((pid) && (librouter_process_wait_for(pid, 6) == 0))
		return (-1);

	return 0;
}

#define NEED_ETHERNET_SUBNET /* verifica se a rede/mascara eh a da ethernet */
/* ip dhcp server NETWORK MASK POOL-START POOL-END [dns-server DNS1 dns-server DNS2 router ROUTER domain-name DOMAIN default-lease-time D H M S max-lease-time D H M S] */

static int _check_subnet(int intf_idx, char *network, char *mask)
{
	IP eth_addr, eth_mask, eth_network;
	IP dhcp_network, dhcp_mask;
	char interface[32];

	sprintf(interface, "eth%d",  intf_idx);

	librouter_ip_interface_get_info(librouter_ip_ethernet_get_dev(interface),
	                                &eth_addr, &eth_mask, 0, 0);
	eth_network.s_addr = eth_addr.s_addr & eth_mask.s_addr;
	inet_aton(network, &dhcp_network);
	inet_aton(mask, &dhcp_mask);

	if ((dhcp_network.s_addr != eth_network.s_addr) || (dhcp_mask.s_addr != eth_mask.s_addr)) {
		librouter_pr_error(0, "network segment not in ethernet segment address");
		return -1;
	}

	return 0;
}

int librouter_dhcp_server_set_dnsserver(char *dns)
{
	char buf[32];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	if (dns != NULL) {
		snprintf(buf, sizeof(buf), "%s", dns);
		if (librouter_str_replace_string_in_file(filename, "opt dns", buf) == 0)
			return 0; /* Done */

		snprintf(buf, sizeof(buf), "opt dns %s\n", dns);
		librouter_str_add_line_to_file(filename, buf);
	} else
		librouter_str_del_line_in_file(filename, "opt dns");
	return 0;
}

int librouter_dhcp_server_get_dnsserver(char **dns)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	memset(buf, 0, sizeof(buf));

	librouter_str_find_string_in_file(filename, "opt dns", buf, sizeof(buf));

	if (buf[0])
		*dns = strdup(buf);
	else
		*dns = NULL;

	return 0;
}

int librouter_dhcp_server_set_leasetime(int time)
{
	char buf[32];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	memset(buf, 0, sizeof(buf));
	if (time > 0) {
		snprintf(buf, sizeof(buf), "%d", time);
		if (librouter_str_replace_string_in_file(filename, "opt lease ", buf) == 0)
			return 0; /* Done */

		snprintf(buf, sizeof(buf), "opt lease %d\n", time);
		librouter_str_add_line_to_file(filename, buf);
	} else
		librouter_str_del_line_in_file(filename, "opt lease");
	return 0;
}

int librouter_dhcp_server_get_leasetime(int *time)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	memset(buf, 0, sizeof(buf));

	librouter_str_find_string_in_file(filename, "opt lease", buf, sizeof(buf));

	if (buf[0])
		*time = atoi(buf);
	else
		*time = 0;

	return 0;
}

int librouter_dhcp_server_set_maxleasetime(int time)
{
	char buf[32];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	if (time > 0) {
		snprintf(buf, sizeof(buf), "%d", time);
		if (librouter_str_replace_string_in_file(filename, "decline_time ", buf)
		                == 0)
			return 0; /* Done */

		snprintf(buf, sizeof(buf), "decline_time %d\n", time);
		librouter_str_add_line_to_file(filename, buf);
	} else
		librouter_str_del_line_in_file(filename, "decline_time");

	return 0;
}

int librouter_dhcp_server_get_maxleasetime(int *time)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	memset(buf, 0, sizeof(buf));

	librouter_str_find_string_in_file(filename, "decline_time", buf, sizeof(buf));

	if (buf[0])
		*time = atoi(buf);
	else
		*time = 0;

	return 0;
}

int librouter_dhcp_server_set_domain(char *domain)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	if (domain != NULL) {
		snprintf(buf, sizeof(buf), "%s", domain);
		if (librouter_str_replace_string_in_file(filename, "opt domain", buf)
		                == 0)
			return 0; /* Done */

		snprintf(buf, sizeof(buf), "opt domain %s\n", domain);
		librouter_str_add_line_to_file(filename, buf);
	} else
		librouter_str_del_line_in_file(filename, "opt domain");
	return 0;
}

int librouter_dhcp_server_get_domain(char **domain)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	memset(buf, 0, sizeof(buf));

	librouter_str_find_string_in_file(filename, "opt domain", buf, sizeof(buf));

	if (buf[0])
		*domain = strdup(buf);
	else
		*domain = NULL;

	return 0;
}

#ifdef OPTION_DHCP_NETBIOS
int librouter_dhcp_server_set_nbns(char *ns)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "%s", ns);
	if (librouter_str_replace_string_in_file(filename,
					"opt wins ", buf) == 0)
	return 0; /* Done */

	snprintf(buf, sizeof(buf), "opt wins %s\n", ns);
	librouter_str_add_line_to_file(filename, buf);
	return 0;
}

int librouter_dhcp_server_set_nbdd(char *dd)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "%s", dd);
	if (librouter_str_replace_string_in_file(filename,
					"opt winsdd ", buf) == 0)
	return 0; /* Done */

	snprintf(buf, sizeof(buf), "opt winsdd %s\n", dd);
	librouter_str_add_line_to_file(filename, buf);
	return 0;
}

int librouter_dhcp_server_set_nbnt(int nt)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d", nt);
	if (librouter_str_replace_string_in_file(filename,
					"opt lease ", buf) == 0)
	return 0; /* Done */

	snprintf(buf, sizeof(buf), "opt lease %d\n", nt);
	librouter_str_add_line_to_file(filename, buf);
	return 0;
}
#endif /* OPTION_DHCP_NETBIOS */

int librouter_dhcp_server_set_router(char *router)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	if (router != NULL) {
		snprintf(buf, sizeof(buf), "%s", router);
		if (librouter_str_replace_string_in_file(filename, "opt router", buf)
		                == 0)
			return 0; /* Done */

		snprintf(buf, sizeof(buf), "opt router %s\n", router);
		librouter_str_add_line_to_file(filename, buf);
	} else
		librouter_str_del_line_in_file(filename, "opt router");

	return 0;
}

int librouter_dhcp_server_get_router(char **router)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	memset(buf, 0, sizeof(buf));

	librouter_str_find_string_in_file(filename, "opt router", buf, sizeof(buf));

	if (buf[0])
		*router = strdup(buf);
	else
		*router = NULL;

	return 0;
}

int librouter_dhcp_server_set_network(char *network, char *mask)
{
	return 0;
}

int librouter_dhcp_server_get_network(char **network, char **mask)
{
	return 0;
}

int librouter_dhcp_server_set_pool(char *start, char *end)
{
	char filename[32];
	char buf[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);
	if (start != NULL) {
		if (librouter_str_replace_string_in_file(filename, "start", start) < 0) {
			snprintf(buf, sizeof(buf), "start %s\n", start);
			librouter_str_add_line_to_file(filename, buf);
		}
	}
	if (end != NULL) {
		if (librouter_str_replace_string_in_file(filename, "end", end) < 0) {
			snprintf(buf, sizeof(buf), "end %s\n", end);
			librouter_str_add_line_to_file(filename, buf);
		}
	}

	return 0;
}

int librouter_dhcp_server_get_pool(char **start, char **end)
{
	char buf[64];
	char filename[32];

	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);

	memset(buf, 0, sizeof(buf));
	librouter_str_find_string_in_file(filename, "start", buf, sizeof(buf));
	if (buf[0])
		*start = strdup(buf);
	else
		*start = NULL;

	memset(buf, 0, sizeof(buf));
	librouter_str_find_string_in_file(filename, "end", buf, sizeof(buf));
	if (buf[0])
		*end = strdup(buf);
	else
		*end = NULL;

	return 0;
}

int librouter_dhcp_server_set(int enable)
{
	if (enable) {
		char filename[64];
		FILE *file;

		sprintf(filename, FILE_DHCPDLEASES, INTF_DHCP_SERVER_DEFAULT);
		file = fopen(filename, "r");

		if (!file) {
			/* cria o arquivo de leases em branco */
			file = fopen(filename, "w");
		}

		fclose(file);
		/* se o dhcrelay estiver rodando, tira do ar! */
		if (librouter_dhcp_get_status() == DHCP_RELAY)
			librouter_dhcp_set_no_relay();

		/* poe o dhcpd para rodar */
		return librouter_dhcpd_set_status(1, INTF_DHCP_SERVER_DEFAULT);
	} else
		return librouter_dhcpd_set_status(0, INTF_DHCP_SERVER_DEFAULT);
}

/**
 * librouter_dhcp_server_set_config 	Configure DHCP Server
 *
 * @param dhcp
 * @return 0 if success, -1 otherwise
 */
int librouter_dhcp_server_set_config(struct dhcp_server_conf_t *dhcp)
{
	librouter_dhcp_server_set_pool(dhcp->pool_start, dhcp->pool_end);
	librouter_dhcp_server_set_dnsserver(dhcp->dnsserver);
	librouter_dhcp_server_set_domain(dhcp->domain);
	librouter_dhcp_server_set_leasetime(dhcp->default_lease_time);
	librouter_dhcp_server_set_maxleasetime(dhcp->max_lease_time);
	librouter_dhcp_server_set_router(dhcp->default_router);
	librouter_dhcp_server_set(dhcp->enable);

	return 0;
}

/**
 * librouter_dhcp_server_get_config	Get DHCP Server configuration
 *
 * @param dhcp
 * @return
 */
int librouter_dhcp_server_get_config(struct dhcp_server_conf_t *dhcp)
{
	librouter_dhcp_server_get_pool(&dhcp->pool_start, &dhcp->pool_end);
	librouter_dhcp_server_get_dnsserver(&dhcp->dnsserver);
	librouter_dhcp_server_get_domain(&dhcp->domain);
	librouter_dhcp_server_get_leasetime(&dhcp->default_lease_time);
	librouter_dhcp_server_get_maxleasetime(&dhcp->max_lease_time);
	librouter_dhcp_server_get_router(&dhcp->default_router);

	if (librouter_dhcp_get_status() == DHCP_SERVER)
		dhcp->enable = 1;
	else
		dhcp->enable = 0;

	return 0;
}

int librouter_dhcp_server_free_config(struct dhcp_server_conf_t *dhcp)
{
	if (dhcp->pool_start)
		free(dhcp->pool_start);
	if (dhcp->pool_end)
		free(dhcp->pool_end);
	if (dhcp->default_router)
		free(dhcp->default_router);
	if (dhcp->domain)
		free(dhcp->domain);
	if (dhcp->dnsserver)
		free(dhcp->dnsserver);
}

int _getleases(struct dhcp_lease_t *leases, int intf_index)
{
	int fd;
	int i;
	struct dyn_lease lease;
	struct in_addr addr;
	unsigned d, h, m;
	int64_t written_at, curr, expires_abs;
	char line[128];
	char file[32];
	unsigned expires;

	sprintf(file, FILE_DHCPDLEASES, intf_index);
	fd = open(file, O_RDONLY);

	if (read(fd, &written_at, sizeof(written_at)) != sizeof(written_at))
		return 0;

	//written_at = ntoh64(written_at); /* FIXME Doesn't work for little endian */
	curr = time(NULL);
	if (curr < written_at)
		written_at = curr; /* lease file from future! :) */

	while (read(fd, &lease, sizeof(lease)) == sizeof(lease)) {
		sprintf(line, "%02x:%02x:%02x:%02x:%02x:%02x",
				lease.lease_mac[0], lease.lease_mac[1], lease.lease_mac[2],
				lease.lease_mac[3], lease.lease_mac[4], lease.lease_mac[5]);
		leases->mac = strdup(line);
		dhcp_dbg("MAC : %s\n", leases->mac);

		addr.s_addr = lease.lease_nip;
		/* actually, 15+1 and 19+1, +1 is a space between columns */
		/* lease.hostname is char[20] and is always NUL terminated */
		sprintf(line, " %-16s%-20s", inet_ntoa(addr), lease.hostname);
		leases->ipaddr = strdup(line);
		dhcp_dbg("IP : %s\n", leases->ipaddr);

		expires_abs = ntohl(lease.expires) + written_at;
		if (expires_abs <= curr) {
			sprintf(line, "expired");
			leases->lease_time = strdup(line);
			continue;
		}
		expires = expires_abs - curr;
		d = expires / (24*60*60); expires %= (24*60*60);
		h = expires / (60*60); expires %= (60*60);
		m = expires / 60; expires %= 60;

		if (d) {
			sprintf(line, "%u days %02u:%02u:%02u\n", d, h, m, (unsigned)expires);
		} else {
			sprintf(line, "%02u:%02u:%02u\n", h, m, (unsigned)expires);
		}
		leases->lease_time = strdup(line);
		leases++;
	}

	return 0;
}

int librouter_dhcp_server_get_leases(struct dhcp_lease_t *leases)
{
	int i;
	char filename[64];
	FILE *f;

	for (i = 0; i < MAX_LAN_INTF; i++) {
		if (librouter_udhcpd_kick_by_eth(i) == 0) { /* Upgrade leases file */
			sprintf(filename, FILE_DHCPDLEASES, i);
			f = fopen(filename, "r");
			if (!f)
				continue;
			fclose(f);

			_getleases(leases, i);
		}
	}

	return 0;
}

int librouter_dhcp_server_free_leases(struct dhcp_lease_t *leases)
{
	int i;

	if (leases == NULL)
		return 0;

	for (i=0; i < DHCP_MAX_NUM_LEASES; i++) {
		if (leases->mac != NULL)
			free(leases->mac);
		if (leases->ipaddr != NULL)
			free(leases->ipaddr);
		if (leases->lease_time != NULL)
			free(leases->lease_time);
		leases++;
	}

	return 0;
}

int librouter_dhcp_get_server(char *buf)
{
	int len;
	FILE *file;
	char filename[64];

	if (!buf)
		return -1;
	buf[0] = 0;
	sprintf(filename, FILE_DHCPDCONF, INTF_DHCP_SERVER_DEFAULT);
	if ((file = fopen(filename, "r")) != NULL) {
		if (librouter_udhcpd_pid_by_eth(INTF_DHCP_SERVER_DEFAULT) != -1) {
			/* pula o '#' */
			fseek(file, 1, SEEK_SET);
			fgets(buf, 1023, file);
		}
		fclose(file);
	}

	len = strlen(buf);
	if (len > 0)
		buf[len - 1] = 0; /* overwrite '\n' */

	return 0;
}

int librouter_dhcp_check_server(char *ifname)
{
	int eth;
	FILE *file;
	arglist *args;
	IP eth_addr, eth_mask, eth_network;
	IP dhcp_network, dhcp_mask;
	char filename[64];
	char buf[256];

	eth = atoi(ifname + 8); /* ethernetX */
	sprintf(filename, FILE_DHCPDCONF, eth);

	if ((file = fopen(filename, "r")) != NULL) {

		/* pula o '#' */
		fseek(file, 1, SEEK_SET);

		/* ip dhcp server 192.168.1.0 255.255.255.0 192.168.1.2 192.168.1.10 */
		fgets(buf, 255, file);
		fclose(file);

		args = librouter_make_args(buf);
		if (args->argc < 7) {
			librouter_destroy_args(args);
			return (-1);
		}

		inet_aton(args->argv[3], &dhcp_network);
		inet_aton(args->argv[4], &dhcp_mask);
		librouter_destroy_args(args);

		librouter_ip_interface_get_info(librouter_ip_ethernet_get_dev(ifname), &eth_addr,
		                &eth_mask, 0, 0);
		eth_network.s_addr = eth_addr.s_addr & eth_mask.s_addr;

		if ((dhcp_network.s_addr != eth_network.s_addr) || (dhcp_mask.s_addr
		                != eth_mask.s_addr)) {

			fprintf(stderr, "%% disabling dhcp server on %s\n", ifname);
			unlink(filename);
			sprintf(filename, DHCPD_DAEMON, eth);

			return librouter_exec_init_program(0, filename);
		}
	}

	return 0;
}

int librouter_dhcp_set_relay(char *servers)
{
	/* se o dhcpd ou o dhcrelay estiver rodando, tira do ar */
	if (librouter_dhcp_set_none() < 0)
		return (-1);

	librouter_str_replace_string_in_file("/etc/inittab", "/bin/dhcrelay -q -d", servers);

	/* poe o dhcrelay para rodar */
	return librouter_exec_init_program(1, DHCRELAY_DAEMON);
}

int librouter_dhcp_get_relay(char *buf)
{
	char *p;
	char cmdline[MAX_PROC_CMDLINE];
	int len;

	if (librouter_process_get_info(DHCRELAY_DAEMON, NULL, cmdline) == 0)
		return (-1);

	p = strstr(cmdline, DHCRELAY_DAEMON" -q -d ");
	if (!p)
		return (-1);

	p += sizeof(DHCRELAY_DAEMON) + sizeof(" -q -d ") - 2;
	strcpy(buf, p);
	len = strlen(buf);

	if (len > 0)
		buf[len - 1] = 0;

	return 0;
}

#ifdef OPTION_IPSEC
pid_t librouter_udhcpd_pid_local(void)
{
	FILE *F;
	pid_t pid;
	char buf[32];

	F = fopen(FILE_DHCPDPID_LOCAL, "r");
	if (F) {
		fgets(buf, 32, F);
		pid = (pid_t) atoi(buf);
		fclose(F);
		return pid;
	}

	return (pid_t) -1;
}

int librouter_udhcpd_reload_local(void)
{
	pid_t pid;

	if ((pid = librouter_udhcpd_pid_local()) != -1) {
		kill(pid, SIGHUP);
		return 0;
	}

	return -1;
}

int librouter_dhcp_get_local(void)
{
	if (librouter_exec_check_daemon(DHCPD_DAEMON_LOCAL))
		return DHCP_SERVER;
	else
		return DHCP_NONE;
}

int librouter_dhcpd_set_local(int on_off)
{
	int ret;

	if (on_off && librouter_dhcp_get_local() == DHCP_SERVER)
		return librouter_udhcpd_reload_local();

	ret = librouter_exec_init_program(on_off, DHCPD_DAEMON_LOCAL);

	librouter_l2tp_exec(RESTART); /* L2TPd integration! */

	return ret;
}

/* Discover from file which ethernet interface is to be used in dhcp requests (L2TP) */
int librouter_dhcp_l2tp_get_interface(void)
{
	int *eth_number, fd;

	fd = open(DHCP_INTF_FILE, O_RDWR);
	if (fd < 0)
		return DHCP_INTF_ETHERNET_0;
	eth_number = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (*eth_number == 1)
		return DHCP_INTF_ETHERNET_1;

	return DHCP_INTF_ETHERNET_0;
}

/*
 * l2tp pool <local|eth 0> POOL-START POOL-END [dns-server DNS1 dns-server DNS2 router ROUTER
 * domain-name DOMAIN default-lease-time D H M S max-lease-time D H M S mask MASK]
 */
int librouter_dhcp_set_server_local(int save_dns, char *cmdline)
{
	int i = 3;
	char *mask = NULL, *pool_start = NULL, *pool_end = NULL, *dns1 = NULL, *dns2 = NULL,
	                *router = NULL, *domain_name = NULL, *netbios_name_server = NULL,
	                *netbios_dd_server = NULL;
	char *p;
	int netbios_node_type = 0;
	unsigned long default_lease_time = 0L, max_lease_time = 0L;
	arglist *args;
	FILE *file;

	args = librouter_make_args(cmdline);
	if (strcmp(args->argv[2], "eth") == 0) {
		int eth_number = atoi(args->argv[3]);
		FILE *fd;

		fd = fopen(DHCP_INTF_FILE, "w+");
		fwrite(&eth_number, sizeof(int), 1, fd);
		fclose(fd);

		librouter_dhcpd_set_local(0); /* turn off udhcpd local! */
		librouter_destroy_args(args);
		return 0;
	}

	if (args->argc < 5) { /* l2tp pool local POOL-START POOL-END */
		librouter_destroy_args(args);
		return -1;
	}

	pool_start = args->argv[i++];
	pool_end = args->argv[i++];
	while (i < args->argc) {

		if (strcmp(args->argv[i], "dns-server") == 0) {
			i++;
			if (dns1)
				dns2 = args->argv[i];
			else
				dns1 = args->argv[i];

		} else if (strcmp(args->argv[i], "mask") == 0) {
			mask = args->argv[++i];

		} else if (strcmp(args->argv[i], "router") == 0) {
			router = args->argv[++i];

		} else if (strcmp(args->argv[i], "domain-name") == 0) {
			domain_name = args->argv[++i];

		} else if (strcmp(args->argv[i], "default-lease-time") == 0) {
			default_lease_time = atoi(args->argv[++i]) * 86400;
			default_lease_time += atoi(args->argv[++i]) * 3600;
			default_lease_time += atoi(args->argv[++i]) * 60;
			default_lease_time += atoi(args->argv[++i]);

		} else if (strcmp(args->argv[i], "max-lease-time") == 0) {
			max_lease_time = atoi(args->argv[++i]) * 86400;
			max_lease_time += atoi(args->argv[++i]) * 3600;
			max_lease_time += atoi(args->argv[++i]) * 60;
			max_lease_time += atoi(args->argv[++i]);

		} else if (strcmp(args->argv[i], "netbios-name-server") == 0) {
			netbios_name_server = args->argv[++i];

		} else if (strcmp(args->argv[i], "netbios-dd-server") == 0) {
			netbios_dd_server = args->argv[++i];

		} else if (strcmp(args->argv[i], "netbios-node-type") == 0) {
			switch (args->argv[++i][0]) {
			case 'B':
				netbios_node_type = 1;
				break;
			case 'P':
				netbios_node_type = 2;
				break;
			case 'M':
				netbios_node_type = 4;
				break;
			case 'H':
				netbios_node_type = 8;
				break;
			}
		} else {
			librouter_destroy_args(args);
			return (-1);
		}
		i++;
	}

	/* cria o arquivo de configuracao */
	if ((file = fopen(FILE_DHCPDCONF_LOCAL, "w")) == NULL) {
		librouter_pr_error(1, "could not create %s", FILE_DHCPDCONF_LOCAL);
		librouter_destroy_args(args);
		return (-1);
	}

	/* salva a configuracao de forma simplificada como um comentario na primeira 
	 * linha (para facilitar a leitura posterior por get_dhcp_server). */
	if (!save_dns) {
		if ((p = strstr(cmdline, "dns-server")) != NULL)
			*p = 0; /* cut dns configuration */
	}

	fprintf(file, "#%s\n", cmdline);
	fprintf(file, "interface lo0\n");
	fprintf(file, "lease_file "FILE_DHCPDLEASES_LOCAL"\n");
	fprintf(file, "pidfile "FILE_DHCPDPID_LOCAL"\n");
	fprintf(file, "start %s\n", pool_start);
	fprintf(file, "end %s\n", pool_end);

	if (max_lease_time)
		fprintf(file, "decline_time %lu\n", max_lease_time);
	/* !!! Reduce lease time to conserve resources... */

	if (default_lease_time)
		fprintf(file, "opt lease %lu\n", default_lease_time);

	if (mask)
		fprintf(file, "opt subnet %s\n", mask);

	if (dns1) {
		fprintf(file, "opt dns %s", dns1);
		if (dns2)
			fprintf(file, " %s\n", dns2);
		else
			fprintf(file, "\n");
	}

	if (router)
		fprintf(file, "opt router %s\n", router);

	if (domain_name)
		fprintf(file, "opt domain %s\n", domain_name);

	if (netbios_name_server)
		fprintf(file, "opt wins %s\n", netbios_name_server);

	if (netbios_dd_server)
		fprintf(file, "opt winsdd %s\n", netbios_dd_server);

	if (netbios_node_type)
		fprintf(file, "opt winsnode %d\n", netbios_node_type);

	fclose(file);
	librouter_destroy_args(args);

	file = fopen(FILE_DHCPDLEASES_LOCAL, "r");
	if (!file) {

		/* cria o arquivo de leases em branco */
		file = fopen(FILE_DHCPDLEASES_LOCAL, "w");
	}
	fclose(file);

	return librouter_dhcpd_set_local(1);
}

int librouter_dhcp_get_server_local(char *buf)
{
	FILE *file;
	int len;

	if (!(file = fopen(FILE_DHCPDCONF_LOCAL, "r"))) {
		librouter_pr_error(1, "could not open %s", FILE_DHCPDCONF_LOCAL);
		return (-1);
	}
	fseek(file, 1, SEEK_SET); // pula o '#'
	fgets(buf, 1023, file);
	fclose(file);
	len = strlen(buf);
	if (len > 0)
		buf[len - 1] = 0;
	return 0;
}
#endif

