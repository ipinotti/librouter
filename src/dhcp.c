/*
 * dhcp.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
/*#include <linux/config.h>*/
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
#include "dhcp.h"
#include "ip.h"
#include "ipsec.h"
#include "process.h"
#include "str.h"
#include "typedefs.h"

#ifdef UDHCPD
pid_t libconfig_udhcpd_pid_by_eth(int eth)
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

int libconfig_udhcpd_reload(int eth)
{
	pid_t pid;

	if ((pid = libconfig_udhcpd_pid_by_eth(eth)) != -1) {
		kill(pid, SIGHUP);
		return 0;
	}
	return -1;
}

int libconfig_udhcpd_kick_by_eth(int eth)
{
	pid_t pid;

	if ((pid = libconfig_udhcpd_pid_by_eth(eth)) != -1) {
		kill(pid, SIGUSR1); /* atualiza leases file */
		return 0;
	}
	return -1;
}

pid_t libconfig_udhcpd_pid_by_name(char *ifname)
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

int libconfig_udhcpd_kick_by_name(char *ifname)
{
	pid_t pid;

	if ((pid = libconfig_udhcpd_pid_by_name(ifname)) != -1) {
		kill(pid, SIGUSR1); /* perform renew */
		return 0;
	}
	return -1;
}
#endif

int libconfig_dhcp_get_status(void)
{
	int ret = DHCP_NONE;
	char daemon0[64];

	sprintf(daemon0, DHCPD_DAEMON, 0);

	if (libconfig_exec_check_daemon(daemon0))
		return DHCP_SERVER;

	if (libconfig_exec_check_daemon(DHCRELAY_DAEMON))
		return DHCP_RELAY;

	return DHCP_NONE;
}

int libconfig_dhcpd_set_status(int on_off, int eth)
{
	int ret;
	char daemon[64];

	if (on_off) {
		if ((ret = libconfig_udhcpd_reload(eth)) != -1)
			return ret;
		sprintf(daemon, DHCPD_DAEMON, eth);
		return libconfig_exec_init_program(1, daemon);
	}
	sprintf(daemon, DHCPD_DAEMON, 0);
	ret = libconfig_exec_init_program(0, daemon);

	return ret;
}

int libconfig_dhcp_set_none(void)
{
	int ret, pid;

	ret = libconfig_dhcp_get_status();
	if (ret == DHCP_SERVER) {
		if (libconfig_dhcpd_set_status(0, 0) < 0)
			return (-1);
#if 0 /* ifndef UDHCPD */
		pid=libconfig_udhcpd_pid_by_eth(?);
		if ((pid > 0) && (libconfig_process_wait_for(pid, 6) == 0)) return (-1);
#endif
	}
	if (ret == DHCP_RELAY) {
		if (libconfig_exec_init_program(0, DHCRELAY_DAEMON) < 0)
			return (-1);
		pid = get_pid(DHCRELAY_DAEMON);
		if ((pid) && (libconfig_process_wait_for(pid, 6) == 0))
			return (-1);
	}
	return 0;
}

int libconfig_dhcp_set_no_server(void)
{
#if 0
	int pid;
#endif

	if (libconfig_dhcpd_set_status(0, 0) < 0)
		return (-1);
#if 0 /* ifndef UDHCPD */
	pid=get_pid(DHCPD_DAEMON);
	if ((pid)&&(libconfig_process_wait_for(pid, 6) == 0)) return (-1);
#endif
	return 0;
}

int libconfig_dhcp_set_no_relay(void)
{
	int pid;

	if (libconfig_exec_init_program(0, DHCRELAY_DAEMON) < 0)
		return (-1);
	pid = get_pid(DHCRELAY_DAEMON);
	if ((pid) && (libconfig_process_wait_for(pid, 6) == 0))
		return (-1);

	return 0;
}

#define NEED_ETHERNET_SUBNET /* verifica se a rede/mascara eh a da ethernet */
/* ip dhcp server NETWORK MASK POOL-START POOL-END [dns-server DNS1 dns-server DNS2 router ROUTER domain-name DOMAIN default-lease-time D H M S max-lease-time D H M S] */
int libconfig_dhcp_set_server(int save_dns, char *cmdline)
{
	int i, eth;
	char *network = NULL, *mask = NULL, *pool_start = NULL, *pool_end =
	                NULL, *dns1 = NULL, *dns2 = NULL, *router = NULL,
	                *domain_name = NULL, *netbios_name_server = NULL,
	                *netbios_dd_server = NULL;
	char *p;
	int netbios_node_type = 0;
	unsigned long default_lease_time = 0L, max_lease_time = 0L;
	arglist *args;
	FILE *file;
	char filename[64];
#ifdef NEED_ETHERNET_SUBNET
	IP eth_addr, eth_mask, eth_network;
	IP dhcp_network, dhcp_mask;
#endif

	args = libconfig_make_args(cmdline);
	if (args->argc < 7) {
		libconfig_destroy_args(args);
		return (-1);
	}

	i = 3;
	network = args->argv[i++];
	mask = args->argv[i++];
	pool_start = args->argv[i++];
	pool_end = args->argv[i++];

#ifdef NEED_ETHERNET_SUBNET
	libconfig_ip_interface_get_info(libconfig_ip_ethernet_get_dev("ethernet0"), &eth_addr,
	                &eth_mask, 0, 0);
	eth_network.s_addr = eth_addr.s_addr & eth_mask.s_addr;
	inet_aton(network, &dhcp_network);
	inet_aton(mask, &dhcp_mask);
	if ((dhcp_network.s_addr != eth_network.s_addr) || (dhcp_mask.s_addr
	                != eth_mask.s_addr)) {
		libconfig_pr_error(0,
		                "network segment not in ethernet segment address");
		libconfig_destroy_args(args);
		return (-1);
	} else {
		eth = 0;
	}
#endif

	while (i < args->argc) {
		if (strcmp(args->argv[i], "dns-server") == 0) {
			i++;
			if (dns1)
				dns2 = args->argv[i];
			else
				dns1 = args->argv[i];

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
			libconfig_destroy_args(args);
			return (-1);
		}
		i++;
	}

	/* cria o arquivo de configuracao */
	sprintf(filename, FILE_DHCPDCONF, eth);
	if ((file = fopen(filename, "w")) == NULL) {
		libconfig_pr_error(1, "could not create %s", filename);
		libconfig_destroy_args(args);
		return (-1);
	}

	/* salva a configuracao de forma simplificado como um comentario na primeira
	 * linha (para facilitar a leitura posterior por get_dhcp_server).
	 */
	if (!save_dns) {
		if ((p = strstr(cmdline, "dns-server")) != NULL)
			*p = 0; /* cut dns configuration */
	}

	fprintf(file, "#%s\n", cmdline);

#ifdef UDHCPD
	fprintf(file, "interface ethernet%d\n", eth);
	fprintf(file, "lease_file "FILE_DHCPDLEASES"\n", eth);
	fprintf(file, "pidfile "FILE_DHCPD_PID_ETH"\n", eth);
	fprintf(file, "start %s\n", pool_start);
	fprintf(file, "end %s\n", pool_end);

	if (max_lease_time)
		fprintf(file, "decline_time %lu\n", max_lease_time);

	if (default_lease_time)
		fprintf(file, "opt lease %lu\n", default_lease_time);

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
#else
	fprintf(file, "ddns-update-style none;\n");
	fprintf(file, "subnet %s netmask %s\n", network, mask);
	fprintf(file, "{\n");
	fprintf(file, " range %s %s;\n", pool_start, pool_end);
	fprintf(file, " default-lease-time %lu;\n", default_lease_time ? default_lease_time : 300L);
	fprintf(file, " max-lease-time %lu;\n", max_lease_time ? max_lease_time : 300L);

	if (dns1) {
		fprintf(file, " option domain-name-servers %s", dns1);
		if (dns2) fprintf(file, ",%s;\n", dns2); else fprintf(file, ";\n");
	}

	if (router)
		fprintf(file, " option routers %s;\n", router);

	if (domain_name)
		fprintf(file, " option domain-name \"%s\";\n", domain_name);

	if (netbios_name_server)
		fprintf(file, " option netbios-name-servers %s;\n", netbios_name_server);

	if (netbios_dd_server)
		fprintf(file, " option netbios-dd-server %s;\n", netbios_dd_server);

	if (netbios_node_type)
		fprintf(file, " option netbios-node-type %d;\n", netbios_node_type);

	fprintf(file, "}\n");
#endif
	fclose(file);
	libconfig_destroy_args(args);

	sprintf(filename, FILE_DHCPDLEASES, eth);
	file = fopen(filename, "r");

	if (!file) {
		/* cria o arquivo de leases em branco */
		file = fopen(filename, "w");
	}

	fclose(file);

	/* se o dhcrelay estiver rodando, tira do ar! */
	if (libconfig_dhcp_get_status() == DHCP_RELAY)
		libconfig_dhcp_set_no_relay();

	/* poe o dhcpd para rodar */
	return libconfig_dhcpd_set_status(1, eth);
}

int libconfig_dhcp_get_server(char *buf)
{
	int len;
	FILE *file;
	char filename[64];

	if (!buf)
		return -1;
	buf[0] = 0;
	sprintf(filename, FILE_DHCPDCONF, 0);
	if ((file = fopen(filename, "r")) != NULL) {
		if (libconfig_udhcpd_pid_by_eth(0) != -1) {
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

int libconfig_dhcp_check_server(char *ifname)
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

		args = libconfig_make_args(buf);
		if (args->argc < 7) {
			libconfig_destroy_args(args);
			return (-1);
		}

		inet_aton(args->argv[3], &dhcp_network);
		inet_aton(args->argv[4], &dhcp_mask);
		libconfig_destroy_args(args);

		libconfig_ip_interface_get_info(libconfig_ip_ethernet_get_dev(ifname), &eth_addr, &eth_mask, 0, 0);
		eth_network.s_addr = eth_addr.s_addr & eth_mask.s_addr;

		if ((dhcp_network.s_addr != eth_network.s_addr) ||
			(dhcp_mask.s_addr != eth_mask.s_addr)) {

			fprintf(stderr, "%% disabling dhcp server on %s\n",
			                ifname);
			unlink(filename);
			sprintf(filename, DHCPD_DAEMON, eth);

			return libconfig_exec_init_program(0, filename);
		}
	}

	return 0;
}

int libconfig_dhcp_set_relay(char *servers)
{
	/* se o dhcpd ou o dhcrelay estiver rodando, tira do ar */
	if (libconfig_dhcp_set_none() < 0)
		return (-1);

	libconfig_str_replace_string_in_file("/etc/inittab", "/bin/dhcrelay -q -d", servers);

	/* poe o dhcrelay para rodar */
	return libconfig_exec_init_program(1, DHCRELAY_DAEMON);
}

int libconfig_dhcp_get_relay(char *buf)
{
	char *p;
	char cmdline[MAX_PROC_CMDLINE];
	int len;

	if (libconfig_process_get_info(DHCRELAY_DAEMON, NULL, cmdline) == 0)
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
pid_t libconfig_udhcpd_pid_local(void)
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

int libconfig_udhcpd_reload_local(void)
{
	pid_t pid;

	if ((pid = libconfig_udhcpd_pid_local()) != -1) {
		kill(pid, SIGHUP);
		return 0;
	}

	return -1;
}

int libconfig_dhcp_get_local(void)
{
	if (libconfig_exec_check_daemon(DHCPD_DAEMON_LOCAL))
		return DHCP_SERVER;
	else
		return DHCP_NONE;
}

int libconfig_dhcpd_set_local(int on_off)
{
	int ret;

	if (on_off && libconfig_dhcp_get_local() == DHCP_SERVER)
		return libconfig_udhcpd_reload_local();

	ret = libconfig_exec_init_program(on_off, DHCPD_DAEMON_LOCAL);

	libconfig_l2tp_exec(RESTART); /* L2TPd integration! */

	return ret;
}

/* Discover from file which ethernet interface is to be used in dhcp requests (L2TP) */
int libconfig_dhcp_l2tp_get_interface(void)
{
	int *eth_number, fd;

	fd = open(DHCP_INTF_FILE, O_RDWR);
	if (fd < 0)
		return DHCP_INTF_ETHERNET_0;
	eth_number = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
	                MAP_SHARED, fd, 0);
	close(fd);
	if (*eth_number == 1)
		return DHCP_INTF_ETHERNET_1;

	return DHCP_INTF_ETHERNET_0;
}

/*
 * l2tp pool <local|ethernet 0> POOL-START POOL-END [dns-server DNS1 dns-server DNS2 router ROUTER
 * domain-name DOMAIN default-lease-time D H M S max-lease-time D H M S mask MASK]
 */
int libconfig_dhcp_set_server_local(int save_dns, char *cmdline)
{
	int i = 3;
	char *mask = NULL, *pool_start = NULL, *pool_end = NULL, *dns1 = NULL,
	                *dns2 = NULL, *router = NULL, *domain_name = NULL,
	                *netbios_name_server = NULL, *netbios_dd_server = NULL;
	char *p;
	int netbios_node_type = 0;
	unsigned long default_lease_time = 0L, max_lease_time = 0L;
	arglist *args;
	FILE *file;

	args = libconfig_make_args(cmdline);
	if (strcmp(args->argv[2], "ethernet") == 0) {
		int eth_number = atoi(args->argv[3]);
		FILE *fd;

		fd = fopen(DHCP_INTF_FILE, "w+");
		fwrite(&eth_number, sizeof(int), 1, fd);
		fclose(fd);

		libconfig_dhcpd_set_local(0); /* turn off udhcpd local! */
		libconfig_destroy_args(args);
		return 0;
	}

	if (args->argc < 5) { /* l2tp pool local POOL-START POOL-END */
		libconfig_destroy_args(args);
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
			libconfig_destroy_args(args);
			return (-1);
		}
		i++;
	}

	/* cria o arquivo de configuracao */
	if ((file = fopen(FILE_DHCPDCONF_LOCAL, "w")) == NULL) {
		libconfig_pr_error(1, "could not create %s", FILE_DHCPDCONF_LOCAL);
		libconfig_destroy_args(args);
		return (-1);
	}

	/* salva a configuracao de forma simplificada como um comentario na primeira 
	 * linha (para facilitar a leitura posterior por get_dhcp_server). */
	if (!save_dns) {
		if ((p = strstr(cmdline, "dns-server")) != NULL)
			*p = 0; /* cut dns configuration */
	}

	fprintf(file, "#%s\n", cmdline);
	fprintf(file, "interface loopback0\n");
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
	libconfig_destroy_args(args);

	file = fopen(FILE_DHCPDLEASES_LOCAL, "r");
	if (!file) {
		/* cria o arquivo de leases em branco */
		file = fopen(FILE_DHCPDLEASES_LOCAL, "w");
	}
	fclose(file);

	return libconfig_dhcpd_set_local(1);
}

int libconfig_dhcp_get_server_local(char *buf)
{
	FILE *file;
	int len;

	if (!(file = fopen(FILE_DHCPDCONF_LOCAL, "r"))) {
		libconfig_pr_error(1, "could not open %s", FILE_DHCPDCONF_LOCAL);
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

