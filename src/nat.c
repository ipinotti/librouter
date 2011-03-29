/*
 * nat.c
 *
 *  Created on: Jun 2, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <ctype.h>

#include <arpa/inet.h>

#include "options.h"
#include "args.h"
#include "nat.h"
#include "ip.h"

#ifdef OPTION_NAT
int librouter_nat_rule_exists(char *nat_rule)
{
	FILE *f;
	char *tmp, buf[256];
	int nat_rule_exists = 0;

	f = popen("/bin/iptables -t nat -L -n", "r");

	if (!f) {
		fprintf(stderr, "%% NAT subsystem not found\n");
		return 0;
	}

	while (!feof(f)) {
		buf[0] = 0;
		fgets(buf, 255, f);
		buf[255] = 0;
		librouter_str_striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0) {
			tmp = strchr(buf + 6, ' ');
			if (tmp) {
				*tmp = 0;
				if (strcmp(buf + 6, nat_rule) == 0) {
					nat_rule_exists = 1;
					break;
				}
			}
		}
	}
	pclose(f);
	return nat_rule_exists;
}

#if 0
int librouter_nat_exact_rules_exists(struct nat_config *n)
{
	FILE *f;
	arg_list argl = NULL;
	int k, l, n, insert = 0;
	char line[512];
	int ruleexists;

	if (n->mode == insert_nat)
	insert = 1;

	f = fopen(TMP_CFG_FILE, "w+");

	if (f == NULL) {
		fprintf(stderr, "Could not open temporary file\n");
		return -1;
	}

	librouter_nat_dump(0, f, 1);

	fseek(f, 0, SEEK_SET);

	while (fgets(line, sizeof(line), f)) {
		if ((n = librouter_parse_args_din(line, &argl)) > 3) {
			if (n == (args->argc - insert)) {
				if (!strcmp(args->argv[0], "nat-rule")) {
					for (k = 0, l = 0, ruleexists = 1; k < args->argc; k++, l++) {
						if (k == 2 && insert) {
							l--;
							continue;
						}
						if (strcmp(args->argv[k], argl[l])) {
							ruleexists = 0;
							break;
						}
					}
					if (ruleexists) {
						librouter_destroy_args_din(&argl);
						break;
					}
				}
			}
		}
		librouter_destroy_args_din(&argl);
	}

	fclose(f);
	return ruleexists;
}
#endif

/**
 * librouter_nat_apply_rule	Apply rule from structure
 *
 * @param n
 * @return 0 if success
 */
int librouter_nat_apply_rule(struct nat_config *n)
{
	char cmd[256];

	if (!librouter_nat_rule_exists(n->name)) {
		sprintf(cmd, "/bin/iptables -t nat -N %s", n->name);

		nat_dbg("Creating NAT rule: %s\n", cmd);
		if (system(cmd) != 0)
			return -1;
	}

	sprintf(cmd, "/bin/iptables -t nat ");
	switch (n->mode) {
	case insert_nat:
		strcat(cmd, "-I ");
		break;
	case remove_nat:
		strcat(cmd, "-D ");
		break;
	default:
		strcat(cmd, "-A ");
		break;
	}
	/* Print rule's name */
	strcat(cmd, n->name);
	strcat(cmd, " ");

	switch (n->protocol) {
	case proto_tcp:
		strcat(cmd, "-p tcp ");
		break;
	case proto_udp:
		strcat(cmd, "-p udp ");
		break;
	case proto_icmp:
		strcat(cmd, "-p icmp ");
		break;
	default:
		sprintf(cmd + strlen(cmd), "-p %d ", n->protocol);
	}

	/* Source */
	if (strcmp(n->src_address, "0.0.0.0/0")) {
		sprintf(cmd + strlen(cmd), "-s %s ", n->src_address);
	}

	if (strlen(n->src_portrange)) {
		sprintf(cmd + strlen(cmd), "--sport %s ", n->src_portrange);
	}

	if (strcmp(n->dst_address, "0.0.0.0/0")) {
		sprintf(cmd + strlen(cmd), "-d %s ", n->dst_address);
	}

	if (strlen(n->dst_portrange)) {
		sprintf(cmd + strlen(cmd), "--dport %s ", n->dst_portrange);
	}

	if (n->masquerade) {
		if (n->action != snat) {
			fprintf(stderr,
			                "%% Change to interface-address is valid only with source NAT\n");
			return;
		}

		strcat(cmd, "-j MASQUERADE ");

		if (n->nat_port1[0])
			sprintf(cmd + strlen(cmd), "--to-ports %s", n->nat_port1);

		if (n->nat_port2[0])
			sprintf(cmd + strlen(cmd), "-%s ", n->nat_port2);

	} else {
		sprintf(cmd + strlen(cmd), "-j %cNAT --to %s", (n->action == snat) ? 'S' : 'D',
		                n->nat_addr1);
		if (n->nat_addr2[0])
			sprintf(cmd + strlen(cmd), "-%s", n->nat_addr2);
		if (n->nat_port1[0])
			sprintf(cmd + strlen(cmd), ":%s", n->nat_port1);
		if (n->nat_port2[0])
			sprintf(cmd + strlen(cmd), "-%s", n->nat_port2);
	}

#if 0
	/* Check if this exact rule already exists */
	if (librouter_nat_exact_rules_exists(n))
		printf("%% Rule already exists\n");
	else {
#endif
	nat_dbg("Applying NAT rule: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	return 0;
}

/**
 * librouter_nat_check_interface_rule	Check if rule is applied
 *
 * This function needs to be replaced by smaller ones.
 * At the moment, it uses the parameters passed and ignores
 * the ones that are NULL.
 *
 * @param acl
 * @param iface_in
 * @param iface_out
 * @param chain
 * @return 1 if rule is applied, 0 otherwise
 */
int librouter_nat_check_interface_rule(char *acl, char *iface_in, char *iface_out, char *chain)
{
	FILE *f;
	char *tmp, buf[256];
	int acl_exists = 0;
	int in_chain = 0;
	char *iface_in_, *iface_out_, *target;

	f = popen("/bin/iptables -t nat -L -nv", "r");

	if (!f) {
		fprintf(stderr, "%% NAT subsystem not found\n");
		return 0;
	}

	while (fgets(buf, sizeof(buf), f)) {
		librouter_str_striplf(buf);

		if (strncmp(buf, "Chain ", 6) == 0) {
			if (in_chain)
				break; // chegou `a proxima chain sem encontrar - finaliza
			tmp = strchr(buf + 6, ' ');
			if (tmp) {
				*tmp = 0;
				if (strcmp(buf + 6, chain) == 0)
					in_chain = 1;
			}
		} else if ((in_chain) && (strncmp(buf, " pkts", 5) != 0) && (strlen(buf) > 40)) {
			arglist *args;
			char *p;
			p = buf;
			while ((*p) && (*p == ' '))
				p++;
			args = librouter_make_args(p);

			if (args->argc < 7) {
				librouter_destroy_args(args);
				continue;
			}

			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];

			if (((iface_in == NULL) || (strcmp(iface_in_, iface_in) == 0))
			                && ((iface_out == NULL) || (strcmp(iface_out_, iface_out)
			                                == 0)) && ((acl == NULL) || (strcmp(target,
			                acl) == 0))) {
				acl_exists = 1;
				librouter_destroy_args(args);
				break;
			}

			librouter_destroy_args(args);
		}
	}
	pclose(f);
	return acl_exists;
}

int librouter_nat_bind_interface_to_rule(char *interface, char *rulename, nat_chain chain)
{
	char cmd[256];

	memset(cmd, 0, sizeof(cmd));

	if (chain == nat_chain_in)
		sprintf(cmd, "/bin/iptables -t nat -A PREROUTING -i %s -j %s", interface, rulename);
	else {
		if(chain == nat_chain_out)
			sprintf(cmd, "/bin/iptables -t nat -A POSTROUTING -o %s -j %s", interface, rulename);
	}

	nat_dbg("Binding interface: %s\n", cmd);

	if (system(cmd) != 0)
		return -1;

	return 0;
}

/**
 * librouter_nat_delete_rule: 	Delete NAT rule
 * @param name
 * @return 0 if ok, -1 if not
 */
int librouter_nat_delete_rule(char *name)
{
	char cmd[256];

	memset(cmd, 0, sizeof(cmd));

	sprintf(cmd, "/bin/iptables -t nat -F %s", name); /* flush */
	if (system(cmd) != 0)
		return -1;

	sprintf(cmd, "/bin/iptables -t nat -X %s", name); /* delete */
	if (system(cmd) != 0)
		return -1;

	return 0;
}

/**
 * librouter_nat_rule_refcount		Get rule reference count
 *
 * @param nat_rule
 * @return number of references
 */
int librouter_nat_rule_refcount(char *nat_rule)
{
	FILE *f;
	char *tmp;
	char buf[256];

	f = popen("/bin/iptables -t nat -L -n", "r");

	if (!f) {
		fprintf(stderr, "%% NAT subsystem not found\n");
		return 0;
	}

	while (fgets(buf, sizeof(buf), f)) {
		librouter_str_striplf(buf); /* strip new line */

		if (strncmp(buf, "Chain ", 6) == 0) {
			tmp = strchr(buf + 6, ' ');
			if (tmp) {
				*tmp = 0;
				if (strcmp(buf + 6, nat_rule) == 0) {
					tmp = strchr(tmp + 1, '(');
					if (!tmp)
						return 0;
					tmp++;
					return atoi(tmp);
				}
			}
		}
	}
	pclose(f);
	return 0;
}

/**
 * librouter_nat_clean_iface_rules	Remove rules from an interface
 *
 * @param iface
 * @return 0 if success
 */
int librouter_nat_clean_iface_rules(char *iface)
{
	FILE *f;
	char buf[256];
	char cmd[256];
	char chain[16];
	char *p, *iface_in_, *iface_out_, *target;
	FILE *procfile;

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose(procfile);

	f = popen("/bin/iptables -t nat -L -nv", "r");

	if (!f) {
		fprintf(stderr, "%% NAT subsystem not found\n");
		return 0;
	}

	while (fgets(buf, 255, f)) {
		librouter_str_striplf(buf);

		if (strncmp(buf, "Chain ", 6) == 0) {
			p = strchr(buf + 6, ' ');
			if (p) {
				*p = 0;
				strncpy(chain, buf + 6, 16);
				chain[15] = 0;
			}
		} else if ((strncmp(buf, " pkts", 5) != 0) && (strlen(buf) > 40)) {
			arglist *args;

			p = buf;
			while ((*p) && (*p == ' '))
				p++;
			args = librouter_make_args(p);
			if (args->argc < 7) {
				librouter_destroy_args(args);
				continue;
			}
			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];
			if (strncmp(iface, iface_in_, strlen(iface)) == 0) {
				sprintf(cmd, "/bin/iptables -t nat -D %s -i %s -j %s", chain,
				                iface_in_, target);

				sprintf(cmd, "/bin/iptables -t nat -D %s -i %s -j %s", chain,
				                iface_in_, target);
				nat_dbg("%s\n", cmd);
				system(cmd);
			}
			if (strncmp(iface, iface_out_, strlen(iface)) == 0) {
				sprintf(cmd, "/bin/iptables -t nat -D %s -o %s -j %s", chain,
				                iface_out_, target);

				nat_dbg("%s\n", cmd);
				system(cmd);
			}
			librouter_destroy_args(args);
		}
	}
	pclose(f);
	return 0;
}

#define trimcolumn(x) tmp=strchr(x, ' '); if (tmp != NULL) *tmp=0;

static void _print_nat_rule(const char *action,
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

	src_netmask = librouter_ip_extract_mask(_src);
	dst_netmask = librouter_ip_extract_mask(_dst);

	librouter_acl_set_ports(sports, src_ports);
	librouter_acl_set_ports(dports, dst_ports);

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
		fprintf(out, "%s %s ", _src, librouter_ip_ciscomask(src_netmask));

	if (*src_ports)
		fprintf(out, "%s ", src_ports);

	if (strcasecmp(dst, "0.0.0.0/0") == 0)
		fprintf(out, "any ");
	else if (strcmp(dst_netmask, "255.255.255.255") == 0)
		fprintf(out, "host %s ", _dst);
	else
		fprintf(out, "%s %s ", _dst, librouter_ip_ciscomask(dst_netmask));

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

void librouter_nat_dump(char *xacl, FILE *f, int conf_format)
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
		fgets(buf, sizeof(buf) - 1, ipc);
		tmp = strchr(buf, '\n');

		if (tmp)
			*tmp = 0;

		if (strncmp(buf, "Chain ", 6) == 0) {
			//if (conf_format && aclp) fprintf(f, "!\n");
			trimcolumn(buf+6);
			strncpy(acl, buf + 6, 100);
			acl[100] = 0;
			aclp = 0;
		} else if (strncmp(buf, " pkts", 5) != 0) {
			if ((strlen(buf)) && ((xacl == NULL) || (strcmp(xacl, acl) == 0))) {
				arglist *args;
				char *p;

				p = buf;
				while ((*p) && (*p == ' '))
					p++;
				args = librouter_make_args(p);
				if (args->argc < 9) {
					librouter_destroy_args(args);
					continue;
				}

				type = args->argv[2];
				prot = args->argv[3];
				input = args->argv[5];
				output = args->argv[6];
				source = args->argv[7];
				dest = args->argv[8];
				sports = strstr(buf, "spts:");

				if (sports) {
					sports += 5;
				} else {
					sports = strstr(buf, "spt:");
					if (sports)
						sports += 4;
				}

				dports = strstr(buf, "dpts:");

				if (dports) {
					dports += 5;
				} else {
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
				                || (strcmp(type, "DNAT") == 0) || (strcmp(type,
				                "SNAT") == 0)) {

					/* filter CHAINs */
					if (strcmp(acl, "INPUT") != 0 && strcmp(acl, "PREROUTING")
					                != 0 && strcmp(acl, "OUTPUT") != 0
					                && strcmp(acl, "POSTROUTING") != 0) {

						if ((!aclp) && (!conf_format)) {
							fprintf(f, "NAT rule %s\n", acl);
						}

						aclp = 1;
						mcount = buf;

						if (!conf_format) {
							while (*mcount == ' ')
								++mcount;
						}

						_print_nat_rule(type, prot, source, dest, sports,
						                dports, acl, f, conf_format, atoi(
						                                mcount), to,
						                masq_ports);
					}

				} else {

					if (!conf_format) {
						/* PRE|POST ROUTING */
						if (strstr(acl, "ROUTING")) {
							if (strcmp(input, "*"))
								fprintf(
								                f,
								                "interface %s in nat-rule %s\n",
								                input, type);

							if (strcmp(output, "*"))
								fprintf(
								                f,
								                "interface %s out nat-rule %s\n",
								                output, type);
						}
					}
				}
				librouter_destroy_args(args);
			}
		}
	}

	pclose(ipc);
}

void librouter_nat_dump_helper(FILE *f, struct router_config *cfg)
{
	if (cfg->nat_helper_ftp_ports[0]) {
		fprintf(f, "ip nat helper ftp ports %s\n", cfg->nat_helper_ftp_ports);
	} else {
		fprintf(f, "no ip nat helper ftp\n");
	}

	if (cfg->nat_helper_irc_ports[0]) {
		fprintf(f, "ip nat helper irc ports %s\n", cfg->nat_helper_irc_ports);
	} else {
		fprintf(f, "no ip nat helper irc\n");
	}

	if (cfg->nat_helper_tftp_ports[0]) {
		fprintf(f, "ip nat helper tftp ports %s\n", cfg->nat_helper_tftp_ports);
	} else {
		fprintf(f, "no ip nat helper tftp\n");
	}

	fprintf(f, "!\n");
}

int librouter_nat_get_iface_rules(char *iface, char *in_acl, char *out_acl)
{
	typedef enum {
		chain_in, chain_out, chain_other
	} acl_chain;

	FILE *f;
	FILE *procfile;

	acl_chain chain = chain_other;

	char *iface_in_, *iface_out_, *target;
	char *acl_in = NULL, *acl_out = NULL;
	char buf[256];

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose(procfile);

	f = popen("/bin/iptables -t nat -L -nv", "r");

	if (!f) {
		fprintf(stderr, "%% NAT subsystem not found\n");
		return 0;
	}

	while (!feof(f)) {
		buf[0] = 0;
		fgets(buf, 255, f);
		buf[255] = 0;

		librouter_str_striplf(buf);

		if (strncmp(buf, "Chain ", 6) == 0) {
			if (strncmp(buf + 6, "PREROUTING", 10) == 0)
				chain = chain_in;
			else if (strncmp(buf + 6, "POSTROUTING", 11) == 0)
				chain = chain_out;
			else
				chain = chain_other;
		} else if ((strncmp(buf, " pkts", 5) != 0) && (strlen(buf) > 40)) {
			arglist *args;
			char *p;

			p = buf;

			while ((*p) && (*p == ' '))
				p++;

			args = librouter_make_args(p);

			if (args->argc < 7) {
				librouter_destroy_args(args);
				continue;
			}

			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];

			if ((chain == chain_in) && (strcmp(iface, iface_in_) == 0)) {
				acl_in = target;
				strncpy(in_acl, acl_in, 100);
				in_acl[100] = 0;
			}

			if ((chain == chain_out) && (strcmp(iface, iface_out_) == 0)) {
				acl_out = target;
				strncpy(out_acl, acl_out, 100);
				out_acl[100] = 0;
			}

			if (acl_in && acl_out)
				break;

			librouter_destroy_args(args);
		}
	}
	pclose(f);
	return 0;
}
#endif /* OPTION_NAT */
