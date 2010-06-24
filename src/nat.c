/*
 * nat.c
 *
 *  Created on: Jun 2, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "args.h"
#include "cish_defines.h"
#include "ip.h"

#define trimcolumn(x) tmp=strchr(x, ' '); if (tmp != NULL) *tmp=0;

static void print_nat_rule(const char *action,
                    const char *proto,
                    const char *src,
                    const char *dst,
                    const char *sports,
                    const char *dports,
                    char *acl,
                    FILE *out,
                    int conf_format,
                    int mc,
                    char *to,
                    char *masq_ports)
{
	char src_ports[32];
	char dst_ports[32];
	char *nat_addr1 = NULL;
	char *nat_addr2 = NULL;
	char *nat_port1 = NULL;
	char *nat_port2 = NULL;
	char *_src;
	char *_dst;
	const char *src_netmask;
	const char *dst_netmask;

	_src = strdup(src);
	_dst = strdup(dst);
	src_ports[0] = 0;
	dst_ports[0] = 0;
	src_netmask = libconfig_ip_extract_mask(_src);
	dst_netmask = libconfig_ip_extract_mask(_dst);
	libconfig_acl_set_ports(sports, src_ports);
	libconfig_acl_set_ports(dports, dst_ports);
	if (conf_format)
		fprintf(out, "nat-rule ");
	if (conf_format)
		fprintf(out, "%s ", acl);
	else
		fprintf(out, "    ");
	if (strcmp(proto, "all") == 0)
		fprintf(out, "ip ");
	else
		fprintf(out, "%s ", proto);
	if (strcasecmp(src, "0.0.0.0/0") == 0)
		fprintf(out, "any ");
	else if (strcmp(src_netmask, "255.255.255.255") == 0)
		fprintf(out, "host %s ", _src);
	else
		fprintf(out, "%s %s ", _src, libconfig_ip_ciscomask(src_netmask));
	if (*src_ports)
		fprintf(out, "%s ", src_ports);
	if (strcasecmp(dst, "0.0.0.0/0") == 0)
		fprintf(out, "any ");
	else if (strcmp(dst_netmask, "255.255.255.255") == 0)
		fprintf(out, "host %s ", _dst);
	else
		fprintf(out, "%s %s ", _dst, libconfig_ip_ciscomask(dst_netmask));
	if (*dst_ports)
		fprintf(out, "%s ", dst_ports);
	if (to) {
		char *p;

		nat_addr1 = to;
		p = strchr(to, ':');
		if (p) {
			*p = 0;
			nat_port1 = p + 1;
			p = strchr(nat_port1, '-');
			if (p) {
				*p = 0;
				nat_port2 = p + 1;
			}
		}
		p = strchr(to, '-');
		if (p) {
			*p = 0;
			nat_addr2 = p + 1;
		}
	}
	if (masq_ports) {
		char *p;

		nat_port1 = masq_ports;

		p = strchr(masq_ports, '-');
		if (p) {
			*p = 0;
			nat_port2 = p + 1;
		}
	}
	if (strcasecmp(action, "dnat") == 0)
		fprintf(out, "change-destination-to ");
	else if (strcasecmp(action, "snat") == 0)
		fprintf(out, "change-source-to ");
	else if (strcasecmp(action, "masquerade") == 0)
		fprintf(out, "change-source-to interface-address ");
	if (nat_addr1) {
		if (nat_addr2)
			fprintf(out, "pool %s %s ", nat_addr1, nat_addr2);
		else
			fprintf(out, "%s ", nat_addr1);
	}
	if (nat_port1) {
		if (nat_port2)
			fprintf(out, "port range %s %s ", nat_port1, nat_port2);
		else
			fprintf(out, "port %s ", nat_port1);
	}
	if (!conf_format)
		fprintf(out, " (%i matches)", mc);
	fprintf(out, "\n");
}



void lconfig_nat_dump(char *xacl, FILE *F, int conf_format)
{
	FILE *ipc;
	char *tmp;
	char acl[101];
	char *type = NULL;
	char *prot = NULL;
	char *input = NULL;
	char *output = NULL;
	char *source = NULL;
	char *dest = NULL;
	char *sports = NULL;
	char *dports = NULL;
	char *to = NULL;
	char *masq_ports = NULL;
	char *mcount;
	int aclp = 1;
	FILE *procfile;
	char buf[512];

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return;
	fclose(procfile);

	acl[0] = 0;

	ipc = popen("/bin/iptables -t nat -L -nv", "r");

	if (!ipc) {
		fprintf(stderr, "%% NAT subsystem not found\n");
		return;
	}

	while (!feof(ipc)) {
		buf[0] = 0;
		fgets(buf, sizeof(buf)-1, ipc);
		tmp = strchr(buf, '\n');
		if (tmp)
			*tmp = 0;

		if (strncmp(buf, "Chain ", 6) == 0) {
			//if (conf_format && aclp) fprintf(F, "!\n");
			trimcolumn(buf+6);
			strncpy(acl, buf + 6, 100);
			acl[100] = 0;
			aclp = 0;
		} else if (strncmp(buf, " pkts", 5) != 0) {
			if ((strlen(buf)) && ((xacl == NULL) || (strcmp(xacl,
			                acl) == 0))) {
				arglist *args;
				char *p;

				p = buf;
				while ((*p) && (*p == ' '))
					p++;
				args = libconfig_make_args(p);
				if (args->argc < 9) {
					libconfig_destroy_args(args);
					continue;
				}
				type = args->argv[2];
				prot = args->argv[3];
				input = args->argv[5];
				output = args->argv[6];
				source = args->argv[7];
				dest = args->argv[8];
				sports = strstr(buf, "spts:");
				if (sports)
					sports += 5;
				else {
					sports = strstr(buf, "spt:");
					if (sports)
						sports += 4;
				}
				dports = strstr(buf, "dpts:");
				if (dports)
					dports += 5;
				else {
					dports = strstr(buf, "dpt:");
					if (dports)
						dports += 4;
				}
				to = strstr(buf, "to:");
				if (to)
					to += 3;
				masq_ports = strstr(buf, "masq ports: ");
				if (masq_ports)
					masq_ports += 12;

				if (sports)
					trimcolumn(sports);
				if (dports)
					trimcolumn(dports);
				if (to)
					trimcolumn(to);
				if (masq_ports)
					trimcolumn(masq_ports);

				if ((strcmp(type, "MASQUERADE") == 0)
				                || (strcmp(type, "DNAT") == 0)
				                || (strcmp(type, "SNAT") == 0)) {
					if (strcmp(acl, "INPUT") != 0
					                && strcmp(acl,
					                                "PREROUTING")
					                                != 0
					                && strcmp(acl, "OUTPUT")
					                                != 0
					                && strcmp(acl,
					                                "POSTROUTING")
					                                != 0) /* filter CHAINs */
					{
						if ((!aclp) && (!conf_format)) {
							fprintf(
							                F,
							                "NAT rule %s\n",
							                acl);
						}
						aclp = 1;
						mcount = buf;
						if (!conf_format) {
							while (*mcount == ' ')
								++mcount;
						}
						print_nat_rule(type, prot,
						                source, dest,
						                sports, dports,
						                acl, F,
						                conf_format,
						                atoi(mcount),
						                to, masq_ports);
					}
				} else {
					if (!conf_format) {
						if (strstr(acl, "ROUTING")) /* PRE|POST ROUTING */
						{
							if (strcmp(input, "*"))
								fprintf(
								                F,
								                "interface %s in nat-rule %s\n",
								                input,
								                type);
							if (strcmp(output, "*"))
								fprintf(
								                F,
								                "interface %s out nat-rule %s\n",
								                output,
								                type);
						}
					}
				}
				libconfig_destroy_args(args);
			}
		}
	}
	pclose(ipc);
}


void lconfig_nat_dump_helper(FILE *f, cish_config *cish_cfg)
{
	if (cish_cfg->nat_helper_ftp_ports[0]) {
		fprintf(f, "ip nat helper ftp ports %s\n", cish_cfg->nat_helper_ftp_ports);
	} else {
		fprintf(f, "no ip nat helper ftp\n");
	}
	if (cish_cfg->nat_helper_irc_ports[0]) {
		fprintf(f, "ip nat helper irc ports %s\n", cish_cfg->nat_helper_irc_ports);
	} else {
		fprintf(f, "no ip nat helper irc\n");
	}
	if (cish_cfg->nat_helper_tftp_ports[0]) {
		fprintf(f, "ip nat helper tftp ports %s\n", cish_cfg->nat_helper_tftp_ports);
	} else {
		fprintf(f, "no ip nat helper tftp\n");
	}
	fprintf(f, "!\n");
}

int lconfig_nat_get_iface_rules(char *iface, char *in_acl, char *out_acl)
{
	typedef enum {
		chain_in, chain_out, chain_other
	} acl_chain;
	FILE *F;
	char buf[256];
	acl_chain chain = chain_other;
	char *iface_in_, *iface_out_, *target;
	char *acl_in = NULL, *acl_out = NULL;
	FILE *procfile;

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose(procfile);

	F = popen("/bin/iptables -t nat -L -nv", "r");

	if (!F) {
		fprintf(stderr, "%% NAT subsystem not found\n");
		return 0;
	}

	while (!feof(F)) {
		buf[0] = 0;
		fgets(buf, 255, F);
		buf[255] = 0;
		libconfig_str_striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0) {
			if (strncmp(buf + 6, "PREROUTING", 10) == 0)
				chain = chain_in;
			else if (strncmp(buf + 6, "POSTROUTING", 11) == 0)
				chain = chain_out;
			else
				chain = chain_other;
		} else if ((strncmp(buf, " pkts", 5) != 0)
		                && (strlen(buf) > 40)) {
			arglist *args;
			char *p;
			p = buf;
			while ((*p) && (*p == ' '))
				p++;
			args = libconfig_make_args(p);

			if (args->argc < 7) {
				libconfig_destroy_args(args);
				continue;
			}

			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];

			if ((chain == chain_in) && (strcmp(iface, iface_in_)
			                == 0)) {
				acl_in = target;
				strncpy(in_acl, acl_in, 100);
				in_acl[100] = 0;
			}

			if ((chain == chain_out) && (strcmp(iface, iface_out_)
			                == 0)) {
				acl_out = target;
				strncpy(out_acl, acl_out, 100);
				out_acl[100] = 0;
			}
			if (acl_in && acl_out)
				break;

			libconfig_destroy_args(args);
		}
	}
	pclose(F);
	return 0;
}
