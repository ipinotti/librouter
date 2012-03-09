#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "options.h"
#include "defines.h"
#include "acl.h"
#include "args.h"
#include "str.h"
#include "exec.h"
#include "ip.h"

#ifdef OPTION_FIREWALL
/**
 * librouter_acl_delete_rule:	Delete firewall rule
 * @param name
 * @return 0 if ok, -1 if not
 */
int librouter_acl_delete_rule(char * name)
{
	char cmd[256];

	memset(cmd, 0, sizeof(cmd));

	sprintf(cmd, "/bin/iptables -F %s", name); /* flush */
	if (system(cmd) != 0)
		return -1;

	sprintf(cmd, "/bin/iptables -X %s", name); /* delete */
	if (system(cmd) != 0)
		return -1;

	return 0;
}


/**
 * librouter_acl_apply_access_policy: Apply access-policy based on the given policy_target (Accept/Drop)
 *
 * @param policy_target
 * @return 0 if ok, -1 if not
 */
int librouter_acl_apply_access_policy(char *policy_target)
{
	char cmd[256];
	FILE *procfile;
	char *target;
	int ret=-1;
	memset(cmd, 0, sizeof(cmd));

	procfile = fopen("/proc/net/ip_tables_names", "r");

	if (!strcmp(policy_target, "accept")) {
		target = "ACCEPT";
		if (!procfile)
			goto end;
		/* doesnt need to load modules! */
	}
	else
		if (!strcmp(policy_target, "drop"))
			target = "DROP";
		else
			target = "REJECT";

	if (procfile)
		fclose(procfile);

	sprintf(cmd, "/bin/iptables -P INPUT %s", target);
	ret = system(cmd);

	sprintf(cmd, "/bin/iptables -P OUTPUT %s", target);
	ret = system(cmd);

	sprintf(cmd, "/bin/iptables -P FORWARD %s", target);
	ret = system(cmd);

end:
	return ret;

}


/**
 * librouter_acl_apply_exist_chain_in_intf: Apply existent chain on a given interface based on the direction (IN/OUT)
 *
 * @param dev
 * @param chain
 * @param direction
 * @return 0 if ok, -1 if not
 */
int librouter_acl_apply_exist_chain_in_intf(char *dev, char *chain, int direction)
{
	char cmd[256];

	memset(cmd, 0, sizeof(cmd));

	if (direction){
		sprintf(cmd, "/bin/iptables -A INPUT -i %s -j %s", dev, chain);
		if (system(cmd) != 0)
			return -1;

		sprintf(cmd, "/bin/iptables -A FORWARD -i %s -j %s", dev, chain);
		if (system(cmd) != 0)
			return -1;
	}
	else{
		sprintf(cmd, "/bin/iptables -A OUTPUT -o %s -j %s", dev, chain);
		if (system(cmd) != 0)
			return -1;

		sprintf(cmd, "/bin/iptables -A FORWARD -o %s -j %s", dev, chain);
		if (system(cmd) != 0)
			return -1;
	}

	return 0;
}


/**
 * librouter_acl_create_new: Create a new chain from a given name
 *
 * @param name
 */
void librouter_acl_create_new(char *name)
{
	char cmd[64];

	sprintf(cmd, "/bin/iptables -N %s", name);
	system(cmd);
}

/**
 * acl_apply: Apply rules from an acl_config structure
 * \param acl: structure with acl configuration
 * \return void
 */
void librouter_acl_apply(struct acl_config *acl)
{
	char cmd[256];
	FILE *f;
	int ruleexists = 0;
	unsigned char buf[512];

	memset(&cmd, 0, sizeof(cmd));

	sprintf(cmd, "/bin/iptables ");

	switch (acl->mode) {
	case insert_acl:
		strcat(cmd, "-I ");
		break;
	case remove_acl:
		strcat(cmd, "-D ");
		break;
	default:
		strcat(cmd, "-A ");
		break;
	}

	strcat(cmd, acl->name);
	strcat(cmd, " ");

	switch (acl->protocol) {
	case tcp:
		strcat(cmd, "-p tcp ");
		break;
	case udp:
		strcat(cmd, "-p udp ");
		break;
	case icmp:
		strcat(cmd, "-p icmp ");
		if (acl->icmp_type) {
			if (acl->icmp_type_code)
				sprintf(cmd + strlen(cmd), "--icmp-type %s ",
				                acl->icmp_type_code);
			else
				sprintf(cmd + strlen(cmd), "--icmp-type %s ",
				                acl->icmp_type);
		}
		break;
	default:
		sprintf(cmd + strlen(cmd), "-p %d ", acl->protocol);
		break;
	}
	/* Go through addresses */
	if (strcmp(acl->src_address, "0.0.0.0/0")) {
		sprintf(cmd + strlen(cmd), "-s %s ", acl->src_address);
	}
	if (strlen(acl->src_portrange)) {
		sprintf(cmd + strlen(cmd), "--sport %s ", acl->src_portrange);
	}
	if (strcmp(acl->dst_address, "0.0.0.0/0")) {
		sprintf(cmd + strlen(cmd), "-d %s ", acl->dst_address);
	}
	if (strlen(acl->dst_portrange)) {
		sprintf(cmd + strlen(cmd), "--dport %s ", acl->dst_portrange);
	}

	if (acl->flags) {
		if (strcmp(acl->flags, "syn") == 0)
			strcat(cmd, "--syn ");
		else {
			char *tmp;

			tmp = strchr(acl->flags, '/');
			if (tmp != NULL)
				*tmp = ' ';
			sprintf(cmd + strlen(cmd), "--tcp-flags %s ",
			                acl->flags);
		}
	}

	if (acl->tos) {
		sprintf(cmd + strlen(cmd), "-m tos --tos %s ", acl->tos);
	}

	if (acl->state) {
		int comma = 0;
		strcat(cmd, "-m state --state ");
		if (acl->state & st_established) {
			strcat(cmd, "ESTABLISHED");
			comma = 1;
		}

		if (acl->state & st_new) {
			if (comma)
				strcat(cmd, ",");
			else
				comma = 1;
			strcat(cmd, "NEW");
		}
		if (acl->state & st_related) {
			if (comma)
				strcat(cmd, ",");
			strcat(cmd, "RELATED");
		}
		strcat(cmd, " ");
	}

	switch (acl->action) {
	case acl_accept:
		strcat(cmd, "-j ACCEPT");
		break;
	case acl_drop:
		strcat(cmd, "-j DROP");
		break;
	case acl_reject:
		strcat(cmd, "-j REJECT");
		break;
	case acl_log:
		strcat(cmd, "-j LOG");
		break;
	case acl_tcpmss:
		strcat(cmd, "-j TCPMSS ");
		if (strcmp(acl->tcpmss, "pmtu") == 0)
			strcat(cmd, "--clamp-mss-to-pmtu");
		else
			sprintf(cmd + strlen(cmd), "--set-mss %s", acl->tcpmss);
		break;
	}

	/* Check if we already have this rule */
	if ((f = fopen(TMP_CFG_FILE, "w+"))) {
		librouter_acl_dump(0, f, 1);
		fseek(f, 0, SEEK_SET);

		/* Parse through all existing rules */
		while (fgets((char *) buf, 511, f)) {
			if (!strcmp((const char *) buf, cmd)) {
				ruleexists = 1;
				break;
			}
		}
		fclose(f);
	}

	if (ruleexists)
		printf("%% Rule already exists\n");
	else {
		acl_dbg("Applying rule : %s\n", cmd);
		system(cmd); /* Do it ! */
	}
}

void librouter_acl_set_ports(const char *ports, char *str)
{
	char *p;

	if (ports) {
		p = strchr(ports, ':');
		if (!p) {
			if (ports[0] == '!')
				sprintf(str, "neq %s", ports + 1);
			else
				sprintf(str, "eq %s", ports);
		} else {
			int from, to;

			from = atoi(ports);
			to = atoi(p + 1);

			if (from == 0)
				sprintf(str, "lt %d", to);
			else if (to == 65535)
				sprintf(str, "gt %d", from);
			else
				sprintf(str, "range %d %d", from, to);
		}
	}
}

void librouter_acl_print_flags(FILE *out, char *flags)
{
	char flags_ascii[6][4] = { "FIN", "SYN", "RST", "PSH", "ACK", "URG" };
	int i, print;
	long int flag, flag_bit;

	flag = strtol(flags, NULL, 16);
	if (flag == 0x3f)
		fprintf(out, "ALL");
	else {
		for (print = 0, i = 0, flag_bit = 0x01; i < 6; i++, flag_bit
		                <<= 1)
			if (flag & flag_bit) {
				fprintf(out, "%s%s", print ? "," : "", flags_ascii[i]);
				print = 1;
			}
	}
}

static void _librouter_acl_print_rule(struct acl_dump *a, FILE *out, int conf_format)
{
	char src_ports[32];
	char dst_ports[32];

	//char *_src;
	//char *_dst;

	const char *src_netmask;
	const char *dst_netmask;

	//_src = strdup(src);
	//_dst = strdup(dst);
	memset(src_ports, 0, sizeof(src_ports));
	memset(dst_ports, 0, sizeof(dst_ports));

	src_netmask = librouter_ip_extract_mask(a->src);
	dst_netmask = librouter_ip_extract_mask(a->dst);
	librouter_acl_set_ports(a->sports, src_ports);
	librouter_acl_set_ports(a->dports, dst_ports);

	if (conf_format)
		fprintf(out, "access-list %s ", a->acl);
	else
		fprintf(out, "    ");

	if (strcmp(a->type, "ACCEPT") == 0)
		fprintf(out, "accept ");
	else if (strcmp(a->type, "DROP") == 0)
		fprintf(out, "drop ");
	else if (strcmp(a->type, "REJECT") == 0)
		fprintf(out, "reject ");
	else if (strcmp(a->type, "LOG") == 0)
		fprintf(out, "log ");
	else if (strcmp(a->type, "TCPMSS") == 0 && a->tcpmss) {
		if (strcmp(a->tcpmss, "PMTU") == 0)
			fprintf(out, "tcpmss pmtu ");
		else
			fprintf(out, "tcpmss %s ", a->tcpmss);
	} else
		fprintf(out, "???? ");

	if (a->mac) {
		fprintf(out, "mac %s ", a->mac);
		if (!conf_format)
			fprintf(out, " (%s matches)", a->mcount);
		fprintf(out, "\n");
		return;
	}

	if (strcmp(a->proto, "all") == 0)
		fprintf(out, "ip ");
	else
		fprintf(out, "%s ", a->proto);
	if (a->icmp_type) {
		if (a->icmp_type_code)
			fprintf(out, "type %s %s ", a->icmp_type, a->icmp_type_code);
		else
			fprintf(out, "type %s ", a->icmp_type);
	}

	/* Source */
	if (strcasecmp(a->src, "0.0.0.0/0") == 0)
		fprintf(out, "any ");
	else if (strcmp(src_netmask, "255.255.255.255") == 0)
		fprintf(out, "host %s ", a->src);
	else
		fprintf(out, "%s %s ", a->src, librouter_ip_ciscomask(src_netmask));
	if (*src_ports)
		fprintf(out, "%s ", src_ports);

	/* Destination */
	if (strcasecmp(a->dst, "0.0.0.0/0") == 0)
		fprintf(out, "any ");
	else if (strcmp(dst_netmask, "255.255.255.255") == 0)
		fprintf(out, "host %s ", a->dst);
	else
		fprintf(out, "%s %s ", a->dst, librouter_ip_ciscomask(dst_netmask));
	if (*dst_ports)
		fprintf(out, "%s ", dst_ports);

	if (a->flags) {
		if (strncmp(a->flags, "0x16/0x02", 9)) {
			char *t;

			t = strtok(a->flags, "/");
			if (t != NULL) {
				fprintf(out, "flags ");
				librouter_acl_print_flags(out, t);
				fprintf(out, "/");
				t = strtok(NULL, "/");
				librouter_acl_print_flags(out, t);
				fprintf(out, " ");
			}
		} else
			fprintf(out, "flags syn ");
	}

	if (a->tos)
		fprintf(out, "tos %d ", (int) strtol(a->tos, NULL, 16));
	if (a->state) {
		if (strstr(a->state, "ESTABLISHED"))
			fprintf(out, "established ");
		if (strstr(a->state, "NEW"))
			fprintf(out, "new ");
		if (strstr(a->state, "RELATED"))
			fprintf(out, "related ");
	}

	if (!conf_format)
		fprintf(out, " (%s matches)", a->mcount);
	fprintf(out, "\n");
}

void librouter_acl_dump(char *xacl, FILE *F, int conf_format)
{
	FILE *ipc;
	FILE *procfile;

	char buf[512];
	char *tmp;

	//char acl[256];

	struct acl_dump a;

	int aclp = 1;

	memset(&a, 0, sizeof(a));

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile) {
		if (conf_format)
			fprintf(F, "!\n");
		return;
	}
	fclose(procfile);

	//acl[0] = 0;

	ipc = popen("/bin/iptables -L -nv", "r");

	if (!ipc) {
		fprintf(stderr, "%% ACL subsystem not found\n");
		return;
	}

	while (fgets(buf, sizeof(buf), ipc)) {
		librouter_str_striplf(buf); /* Remove new line */

		if (strncmp(buf, "Chain ", 6) == 0) {
			trimcolumn(buf+6);
			//strncpy(acl, buf + 6, sizeof(acl));
			a.acl = strdup(buf + 6); /* acl name */
			aclp = 0;
		} else if (strncmp(buf, " pkts", 5) != 0) {
			/* pkts bytes target prot opt in out source destination */
			if (strlen(buf) && ((xacl == NULL) || (strcmp(xacl, a.acl) == 0))) {
				arglist *args;
				char *p;

				p = buf;
				while ((*p) && (*p == ' '))
					p++;

				/* 0 0 DROP tcp -- * * 0.0.0.0/0 0.0.0.0/0 tcp flags:0x16/0x02 */
				args = librouter_make_args(p);
				if (args->argc < 9) {
					librouter_destroy_args(args);
					continue;
				}

				a.type = args->argv[2];
				a.proto = args->argv[3];
				a.input = args->argv[5];
				a.output = args->argv[6];
				a.src = args->argv[7];
				a.dst = args->argv[8];

				a.sports = strstr(buf, "spts:");
				if (a.sports) {
					a.sports += 5;
				} else {
					a.sports = strstr(buf, "spt:");
					if (a.sports)
						a.sports += 4;
				}

				a.dports = strstr(buf, "dpts:");
				if (a.dports) {
					a.dports += 5;
				} else {
					a.dports = strstr(buf, "dpt:");
					if (a.dports)
						a.dports += 4;
				}

				if ((a.flags = strstr(buf, "tcp flags:")))
					a.flags += 10;

				if ((a.tos = strstr(buf, "TOS match 0x")))
					a.tos += 12;

				if ((a.state = strstr(buf, "state ")))
					a.state += 6;

				if ((a.icmp_type = strstr(buf, "icmp type "))) {
					a.icmp_type += 10;
					if ((a.icmp_type_code = strstr(buf, "code ")))
						a.icmp_type_code += 5;
				}

				if ((a.tcpmss = strstr(buf, "TCPMSS clamp to "))) {
					a.tcpmss += 16;
				} else if ((a.tcpmss = strstr(buf, "TCPMSS set "))) {
					a.tcpmss += 11;
				}

				if ((a.mac = strstr(buf, "MAC ")))
					a.mac += 4;

				if (a.flags)
					trimcolumn(a.flags);

				if (a.sports)
					trimcolumn(a.sports);

				if (a.dports)
					trimcolumn(a.dports);

				if (a.tos)
					trimcolumn(a.tos);

				if (a.state)
					trimcolumn(a.state);

				if (a.icmp_type)
					trimcolumn(a.icmp_type);

				if (a.icmp_type_code)
					trimcolumn(a.icmp_type_code);

				if (a.tcpmss)
					trimcolumn(a.tcpmss);

				if (a.mac)
					trimcolumn(a.mac);

				if ((strcmp(a.type, "ACCEPT") == 0) ||
					(strcmp(a.type, "DROP") == 0) ||
					(strcmp(a.type, "REJECT") == 0) ||
					(strcmp(a.type, "LOG") == 0) ||
					(strcmp(a.type, "TCPMSS") == 0)) {

					if (strcmp(a.acl, "INPUT") != 0 &&
						strcmp(a.acl, "OUTPUT") != 0 &&
						strcmp(a.acl, "FORWARD") != 0) {
						/* filter CHAINs */

						if ((!aclp) && (!conf_format)) {
							fprintf(F, "IP access list %s\n", a.acl);
						}

						aclp = 1;
						a.mcount = buf;

						if (!conf_format) {
							while (*a.mcount == ' ')
								++a.mcount;
						}

						_librouter_acl_print_rule(&a, F, conf_format);
					}
				} else {
					if (!conf_format) {
						if (strcmp(a.acl, "FORWARD")) {
							/* INTPUT || OUTPUT */

							if (strcmp(a.input, "*") && !strstr(a.input,"ipsec"))
								fprintf(F,
									"interface %s in access-list %s\n",
									librouter_device_linux_to_cli(a.input, 0),
									a.type);

							if (strcmp(a.output, "*") && !strstr(a.output, "ipsec"))
								fprintf(F,
									"interface %s out access-list %s\n",
									librouter_device_linux_to_cli(a.output, 0),
									a.type);
						}
					}
				}
				librouter_destroy_args(args);
			}
		}
	}
	pclose(ipc);
}

void librouter_acl_dump_policy(FILE *F)
{
	FILE *ipc;
	char buf[512];
	char *p;
	FILE *procfile;

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile) {
		fprintf(F, "access-policy accept\n");
		return;
	}

	fclose(procfile);

	ipc = popen("/bin/iptables -L -nv", "r");

	if (!ipc) {
		fprintf(stderr, "%% ACL subsystem not found\n");
		return;
	}

	while (fgets(buf, sizeof(buf), ipc)) {
		syslog(LOG_INFO, "%s\n", buf);
		p = strstr(buf, "policy");
		if (p) {
			if (strncmp(p + 7, "ACCEPT", 6) == 0) {
				syslog(LOG_INFO, "policy is accept\n");
				fprintf(F, "access-policy accept\n");
				break;
			} //else if (strncasecmp(p + 7, "drop", 4) == 0) {
			else {
				fprintf(F, "access-policy drop\n");
				break;
			}
		}
	}

	pclose(ipc);
}

int librouter_acl_exists(char *acl)
{
	FILE *f;
	char *tmp, buf[256];
	int acl_exists = 0;

	f = popen("/bin/iptables -L -n", "r");

	if (!f) {
		fprintf(stderr, "%% ACL subsystem not found\n");
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
				if (strcmp(buf + 6, acl) == 0) {
					acl_exists = 1;
					break;
				}
			}
		}
	}

	pclose(f);

	return acl_exists;
}

int librouter_acl_matched_exists(char *acl,
                                 char *iface_in,
                                 char *iface_out,
                                 char *chain)
{
	FILE *F;
	char *tmp, buf[256];
	int acl_exists = 0;
	int in_chain = 0;
	char *iface_in_, *iface_out_, *target;

	F = popen("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf(stderr, "%% ACL subsystem not found\n");
		return 0;
	}

	while (!feof(F)) {
		buf[0] = 0;
		fgets(buf, 255, F);
		buf[255] = 0;
		librouter_str_striplf(buf);

		if (strncmp(buf, "Chain ", 6) == 0) {

			/* chegou a proxima chain sem encontrar - finaliza */
			if (in_chain)
				break;

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
				&& ((iface_out == NULL) || (strcmp(iface_out_, iface_out) == 0))
				&& ((acl == NULL) || (strcmp(target, acl) == 0))) {

				acl_exists = 1;
				librouter_destroy_args(args);
				break;
			}

			librouter_destroy_args(args);
		}
	}

	pclose(F);

	return acl_exists;
}

int librouter_acl_get_iface_rules(char *iface, char *in_acl, char *out_acl)
{
	typedef enum {
		chain_fwd, chain_in, chain_out, chain_other
	} acl_chain;

	FILE *F;
	char buf[256];
	acl_chain chain = chain_fwd;
	char *iface_in_, *iface_out_, *target;
	char *acl_in = NULL, *acl_out = NULL;
	FILE *procfile;

	procfile = fopen("/proc/net/ip_tables_names", "r");

	if (!procfile)
		return 0;

	fclose(procfile);

	F = popen("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf(stderr, "%% ACL subsystem not found\n");
		return 0;
	}

	while (!feof(F)) {
		buf[0] = 0;
		fgets(buf, 255, F);
		buf[255] = 0;
		librouter_str_striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0) {
			if (strncmp(buf + 6, "FORWARD", 7) == 0)
				chain = chain_fwd;
			else if (strncmp(buf + 6, "INPUT", 5) == 0)
				chain = chain_in;
			else if (strncmp(buf + 6, "OUTPUT", 6) == 0)
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

			if ((chain == chain_fwd || chain == chain_in)
				&& (strcmp(iface, iface_in_) == 0)) {
				acl_in = target;
				strncpy(in_acl, acl_in, 100);
				in_acl[100] = 0;
			}

			if ((chain == chain_fwd || chain == chain_out)
				&& (strcmp(iface, iface_out_) == 0)) {
				acl_out = target;
				strncpy(out_acl, acl_out, 100);
				out_acl[100] = 0;
			}

			if (acl_in && acl_out)
				break;

			librouter_destroy_args(args);
		}
	}

	pclose(F);

	return 0;
}

int librouter_acl_get_refcount(char *acl)
{
	FILE *F;
	char *tmp;
	char buf[256];

	F = popen("/bin/iptables -L -n", "r");

	if (!F) {
		fprintf(stderr, "%% ACL subsystem not found\n");
		return 0;
	}

	while (!feof(F)) {
		buf[0] = 0;
		fgets(buf, 255, F);
		buf[255] = 0;
		librouter_str_striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0) {
			tmp = strchr(buf + 6, ' ');
			if (tmp) {
				*tmp = 0;
				if (strcmp(buf + 6, acl) == 0) {
					tmp = strchr(tmp + 1, '(');
					if (!tmp)
						return 0;

					tmp++;
					return atoi(tmp);
				}
			}
		}
	}
	pclose(F);
	return 0;
}

int librouter_acl_clean_iface_acls(char *iface)
{
	FILE *F;
	char buf[256];
	char cmd[256];
	char chain[16];
	char *p, *iface_in_, *iface_out_, *target;
	FILE *procfile;

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose(procfile);

	F = popen("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf(stderr, "%% ACL subsystem not found\n");
		return -1;
	}

	chain[0] = 0;
	while (!feof(F)) {
		buf[0] = 0;
		fgets(buf, 255, F);
		buf[255] = 0;
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

				sprintf(cmd, "/bin/iptables -D %s -i %s -j %s",
						chain, iface_in_, target);

				system(cmd);
			}

			if (strncmp(iface, iface_out_, strlen(iface)) == 0) {
				sprintf(cmd, "/bin/iptables -D %s -o %s -j %s",
				                chain, iface_out_, target);
				system(cmd);
			}
			librouter_destroy_args(args);
		}
	}

	pclose(F);

	return 0;
}

/* #ifdef OPTION_IPSEC Starter deve fazer uso de #ifdef */
int librouter_acl_copy_iface_acls(char *src, char *trg) /* starter/interfaces.c */
{
	FILE *F;
	char buf[256];
	char cmd[256];
	char chain[16];
	char *p, *iface_in_, *iface_out_, *target;
	FILE *procfile;

	procfile = fopen("/proc/net/ip_tables_names", "r");

	if (!procfile)
		return 0;

	fclose(procfile);

	F = popen("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf(stderr, "%% ACL subsystem not found\n");
		return -1;
	}

	chain[0] = 0;
	while (!feof(F)) {
		buf[0] = 0;
		fgets(buf, 255, F);
		buf[255] = 0;
		librouter_str_striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0) {
			p = strchr(buf + 6, ' ');

			if (p) {
				*p = 0;
				strncpy(chain, buf + 6, 16);
				chain[15] = 0;
			}

		} else if ((strncmp(buf, " pkts", 5) != 0)
		                && (strlen(buf) > 40)) {
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

			if (strncmp(src, iface_in_, strlen(src)) == 0) {
				sprintf(cmd, "/bin/iptables -A %s -i %s -j %s",
				                chain, trg, target);

				system(cmd);
			}

			if (strncmp(src, iface_out_, strlen(src)) == 0) {
				sprintf(cmd, "/bin/iptables -A %s -o %s -j %s",
				                chain, trg, target);

				system(cmd);
			}

			librouter_destroy_args(args);
		}
	}

	pclose(F);

	return 0;
}


/* We're using netkey now, so no more virtual interfaces */
#if 0 //#ifdef OPTION_IPSEC
int librouter_acl_interface_ipsec(int add_del,
                                  int chain,
                                  char *dev,
                                  char *listno)
{
	int i;
	char filename[32], ipsec[16], iface[16], buf[256];
	FILE *f;

	for (i = 0; i < N_IPSEC_IF; i++) {
		sprintf(ipsec, "ipsec%d", i);
		strcpy(filename, "/var/run/");
		strcat(filename, ipsec);
		if ((f = fopen(filename, "r")) != NULL) {
			fgets(iface, 16, f);
			fclose(f);
			librouter_str_striplf(iface);
			if (strcmp(iface, dev) == 0) /* found ipsec attach to dev */
			{
				if (add_del) {
					if (chain == chain_in) {
						sprintf(buf,
							"/bin/iptables -A INPUT -i %s -j %s",
							ipsec, listno);

						acl_dbg("%s\n", buf);
						system(buf);

						sprintf(buf,
						        "/bin/iptables -A FORWARD -i %s -j %s",
						        ipsec, listno);

						acl_dbg("%s\n", buf);
						system(buf);
					} else {
						sprintf(buf,
							"/bin/iptables -A OUTPUT -o %s -j %s",
							ipsec, listno);

						acl_dbg("%s\n", buf);
						system(buf);

						sprintf(buf,
						        "/bin/iptables -A FORWARD -o %s -j %s",
						        ipsec, listno);

						acl_dbg("%s\n", buf);
						system(buf);
					}
				} else {
					if ((chain == chain_in) || (chain == chain_both)) {
						if (librouter_acl_matched_exists(listno, ipsec, 0, "INPUT")) {
							snprintf(buf,
								256,
								"/bin/iptables -D INPUT -i %s -j %s",
								ipsec,
								listno);

							acl_dbg("%s\n", buf);
							system(buf);
						}

						if (librouter_acl_matched_exists(listno, ipsec, 0, "FORWARD")) {
							snprintf(buf,
								256,
								"/bin/iptables -D FORWARD -i %s -j %s",
								ipsec,
								listno);

							acl_dbg("%s\n", buf);
							system(buf);
						}
					}
					if ((chain == chain_out) || (chain
					                == chain_both)) {
						if (librouter_acl_matched_exists(listno, 0, ipsec, "OUTPUT")) {
							snprintf(buf,
								256,
								"/bin/iptables -D OUTPUT -o %s -j %s",
								ipsec,
								listno);

							acl_dbg("%s\n", buf);
							system(buf);
						}

						if (librouter_acl_matched_exists(listno, 0, ipsec, "FORWARD")) {
							snprintf(buf,
								256,
								"/bin/iptables -D FORWARD -o %s -j %s",
								ipsec,
								listno);

							acl_dbg("%s\n", buf);
							system(buf);
						}
					}
				}

				return 0;
			}
		}
	}
	return -1;
}
#endif /* if 0 */

int delete_module(const char *name); /* libbb ? */

void librouter_acl_cleanup_modules(void)
{
	delete_module(NULL); /* clean unused modules! */
}
#endif /* OPTION_FIREWALL */
