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

//#define DEBUG_CMD(x) printf("cmd = %s\n", x)
//#define DEBUG_CMD(x) syslog(LOG_DEBUG, "%s\n", x)
#define DEBUG_CMD(x)

void acl_create_new(char *name)
{
	char cmd[64];

	sprintf (cmd, "/bin/iptables -N %s", name);
	system (cmd);
}

/**
 * acl_apply: Apply rules from an acl_config structure
 * \param acl: structure with acl configuration
 * \return void
 */
void acl_apply(struct acl_config *acl)
{
	char cmd[256];
	FILE *f;
	int ruleexists = 0;
	unsigned char buf[512];

	memset(&cmd, 0, sizeof(cmd));

	sprintf (cmd, "/bin/iptables ");

	switch (acl->mode) {
	case insert_acl:
		strcat (cmd, "-I ");
		break;
	case remove_acl:
		strcat (cmd, "-D ");
		break;
	default:
		strcat (cmd, "-A ");
		break;
	}

	strcat (cmd, acl->name);
	strcat (cmd, " ");

	switch (acl->protocol) {
	case tcp:
		strcat (cmd, "-p tcp ");
		break;
	case udp:
		strcat (cmd, "-p udp ");
		break;
	case icmp:
		strcat (cmd, "-p icmp ");
		if (acl->icmp_type) {
			if (acl->icmp_type_code)
				sprintf (cmd + strlen (cmd), "--icmp-type %s ",
						acl->icmp_type_code);
			else
				sprintf (cmd + strlen (cmd), "--icmp-type %s ",
						acl->icmp_type);
		}
		break;
	default:
		sprintf (cmd + strlen (cmd), "-p %d ", acl->protocol);
		break;
	}
	/* Go through addresses */
	if (strcmp (acl->src_address, "0.0.0.0/0")) {
		sprintf (cmd + strlen (cmd), "-s %s ", acl->src_address);
	}
	if (strlen (acl->src_portrange)) {
		sprintf (cmd + strlen (cmd), "--sport %s ", acl->src_portrange);
	}
	if (strcmp (acl->dst_address, "0.0.0.0/0")) {
		sprintf (cmd + strlen (cmd), "-d %s ", acl->dst_address);
	}
	if (strlen (acl->dst_portrange)) {
		sprintf (cmd + strlen (cmd), "--dport %s ", acl->dst_portrange);
	}

	if (acl->flags) {
		if (strcmp (acl->flags, "syn") == 0)
			strcat (cmd, "--syn ");
		else {
			char *tmp;

			tmp = strchr (acl->flags, '/');
			if (tmp != NULL)
				*tmp = ' ';
			sprintf (cmd + strlen (cmd), "--tcp-flags %s ", acl->flags);
		}
	}

	if (acl->tos) {
		sprintf (cmd + strlen (cmd), "-m tos --tos %s ", acl->tos);
	}

	if (acl->state) {
		int comma = 0;
		strcat (cmd, "-m state --state ");
		if (acl->state & st_established) {
			strcat (cmd, "ESTABLISHED");
			comma = 1;
		}

		if (acl->state & st_new) {
			if (comma)
				strcat (cmd, ",");
			else
				comma = 1;
			strcat (cmd, "NEW");
		}
		if (acl->state & st_related) {
			if (comma)
				strcat (cmd, ",");
			strcat (cmd, "RELATED");
		}
		strcat (cmd, " ");
	}

	switch (acl->action) {
	case acl_accept:
		strcat (cmd, "-j ACCEPT");
		break;
	case acl_drop:
		strcat (cmd, "-j DROP");
		break;
	case acl_reject:
		strcat (cmd, "-j REJECT");
		break;
	case acl_log:
		strcat (cmd, "-j LOG");
		break;
	case acl_tcpmss:
		strcat (cmd, "-j TCPMSS ");
		if (strcmp (acl->tcpmss, "pmtu") == 0)
			strcat (cmd, "--clamp-mss-to-pmtu");
		else
			sprintf (cmd + strlen (cmd), "--set-mss %s", acl->tcpmss);
		break;
	}

	/* Check if we already have this rule */
	if ((f = fopen (TMP_CFG_FILE, "w+"))) {
		lconfig_acl_dump (0, f, 1);
		fseek (f, 0, SEEK_SET);

		/* Parse through all existing rules */
		while (fgets ((char *) buf, 511, f)) {
			if (!strcmp((const char *)buf, cmd)) {
				ruleexists = 1;
				break;
			}
		}
		fclose (f);
	}

	if (ruleexists)
		printf ("%% Rule already exists\n");
	else {
		DEBUG_CMD(cmd);
		system (cmd); /* Do it ! */
	}
}

void acl_set_ports (const char *ports, char *str)
{
	char *p;

	if (ports) {
		p = strchr (ports, ':');
		if (!p) {
			if (ports[0] == '!')
				sprintf (str, "neq %s", ports + 1);
			else
				sprintf (str, "eq %s", ports);
		} else {
			int from, to;

			from = atoi (ports);
			to = atoi (p + 1);

			if (from == 0)
				sprintf (str, "lt %d", to);
			else if (to == 65535)
				sprintf (str, "gt %d", from);
			else
				sprintf (str, "range %d %d", from, to);
		}
	}
}

void acl_print_flags (FILE *out, char *flags)
{
	char flags_ascii[6][4] = { "FIN", "SYN", "RST", "PSH", "ACK", "URG" };
	int i, print;
	long int flag, flag_bit;

	flag = strtol (flags, NULL, 16);
	if (flag == 0x3f)
		fprintf (out, "ALL");
	else {
		for (print = 0, i = 0, flag_bit = 0x01; i < 6; i++, flag_bit
		                <<= 1)
			if (flag & flag_bit) {
				fprintf (out, "%s%s", print ? "," : "",
				                flags_ascii[i]);
				print = 1;
			}
	}
}

static void acl_print_rule (const char *action,
                        const char *proto,
                        const char *src,
                        const char *dst,
                        const char *sports,
                        const char *dports,
                        char *acl,
                        FILE *out,
                        int conf_format,
                        int mc,
                        char *flags,
                        char *tos,
                        char *state,
                        char *icmp_type,
                        char *icmp_type_code,
                        char *tcpmss,
                        char *mac)
{
	char src_ports[32];
	char dst_ports[32];
	char *_src;
	char *_dst;
	const char *src_netmask;
	const char *dst_netmask;

	_src = strdup (src);
	_dst = strdup (dst);
	src_ports[0] = 0;
	dst_ports[0] = 0;
	src_netmask = extract_mask (_src);
	dst_netmask = extract_mask (_dst);
	acl_set_ports (sports, src_ports);
	acl_set_ports (dports, dst_ports);

	if (conf_format)
		fprintf (out, "access-list ");
	if (conf_format)
		fprintf (out, "%s ", acl);
	else
		fprintf (out, "    ");
	if (strcmp (action, "ACCEPT") == 0)
		fprintf (out, "accept ");
	else if (strcmp (action, "DROP") == 0)
		fprintf (out, "drop ");
	else if (strcmp (action, "REJECT") == 0)
		fprintf (out, "reject ");
	else if (strcmp (action, "LOG") == 0)
		fprintf (out, "log ");
	else if (strcmp (action, "TCPMSS") == 0 && tcpmss) {
		if (strcmp (tcpmss, "PMTU") == 0)
			fprintf (out, "tcpmss pmtu ");
		else
			fprintf (out, "tcpmss %s ", tcpmss);
	} else
		fprintf (out, "???? ");
	if (mac) {
		fprintf (out, "mac %s ", mac);
		if (!conf_format)
			fprintf (out, " (%i matches)", mc);
		fprintf (out, "\n");
		return;
	}
	if (strcmp (proto, "all") == 0)
		fprintf (out, "ip ");
	else
		fprintf (out, "%s ", proto);
	if (icmp_type) {
		if (icmp_type_code)
			fprintf (out, "type %s %s ", icmp_type, icmp_type_code);
		else
			fprintf (out, "type %s ", icmp_type);
	}
	if (strcasecmp (src, "0.0.0.0/0") == 0)
		fprintf (out, "any ");
	else if (strcmp (src_netmask, "255.255.255.255") == 0)
		fprintf (out, "host %s ", _src);
	else
		fprintf (out, "%s %s ", _src, ciscomask (src_netmask));
	if (*src_ports)
		fprintf (out, "%s ", src_ports);
	if (strcasecmp (dst, "0.0.0.0/0") == 0)
		fprintf (out, "any ");
	else if (strcmp (dst_netmask, "255.255.255.255") == 0)
		fprintf (out, "host %s ", _dst);
	else
		fprintf (out, "%s %s ", _dst, ciscomask (dst_netmask));
	if (*dst_ports)
		fprintf (out, "%s ", dst_ports);
	if (flags) {
		if (strncmp (flags, "0x16/0x02", 9)) {
			char *t;

			t = strtok (flags, "/");
			if (t != NULL) {
				fprintf (out, "flags ");
				acl_print_flags (out, t);
				fprintf (out, "/");
				t = strtok (NULL, "/");
				acl_print_flags (out, t);
				fprintf (out, " ");
			}
		} else
			fprintf (out, "flags syn ");
	}
	if (tos)
		fprintf (out, "tos %d ", (int)strtol (tos, NULL, 16));
	if (state) {
		if (strstr (state, "ESTABLISHED"))
			fprintf (out, "established ");
		if (strstr (state, "NEW"))
			fprintf (out, "new ");
		if (strstr (state, "RELATED"))
			fprintf (out, "related ");
	}
	if (!conf_format)
		fprintf (out, " (%i matches)", mc);
	fprintf (out, "\n");
}

void lconfig_acl_dump (char *xacl, FILE *F, int conf_format)
{
	FILE *ipc;
	char buf[512];
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
	char *flags = NULL;
	char *tos = NULL;
	char *state = NULL;
	char *icmp_type = NULL;
	char *icmp_type_code = NULL;
	char *tcpmss = NULL;
	char *mac = NULL;
	char *mcount;
	int aclp = 1;
	FILE *procfile;

	procfile = fopen ("/proc/net/ip_tables_names", "r");
	if (!procfile) {
		if (conf_format)
			fprintf (F, "!\n"); /* ! for next: router rip */
		return;
	}
	fclose (procfile);

	acl[0] = 0;

	ipc = popen ("/bin/iptables -L -nv", "r");

	if (!ipc) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return;
	}

	while (!feof (ipc)) {
		buf[0] = 0;
		fgets (buf, sizeof(buf), ipc);
		tmp = strchr (buf, '\n');
		if (tmp)
			*tmp = 0;

		if (strncmp (buf, "Chain ", 6) == 0) {
			//if ((conf_format)&&(aclp)) fprintf(F, "!\n");
			trimcolumn(buf+6);
			strncpy (acl, buf + 6, 100);
			acl[100] = 0;
			aclp = 0;
		} else if (strncmp (buf, " pkts", 5) != 0) /*  pkts bytes target     prot opt in     out     source               destination */
		{
			if (strlen (buf) && ((xacl == NULL) || (strcmp (xacl,
			                acl) == 0))) {
				arglist *args;
				char *p;

				p = buf;
				while ((*p) && (*p == ' '))
					p++;
				args = make_args (p);
				if (args->argc < 9) /*     0     0 DROP       tcp  --  *      *       0.0.0.0/0            0.0.0.0/0          tcp flags:0x16/0x02 */
				{
					destroy_args (args);
					continue;
				}
				type = args->argv[2];
				prot = args->argv[3];
				input = args->argv[5];
				output = args->argv[6];
				source = args->argv[7];
				dest = args->argv[8];
				sports = strstr (buf, "spts:");
				if (sports)
					sports += 5;
				else {
					sports = strstr (buf, "spt:");
					if (sports)
						sports += 4;
				}
				dports = strstr (buf, "dpts:");
				if (dports)
					dports += 5;
				else {
					dports = strstr (buf, "dpt:");
					if (dports)
						dports += 4;
				}
				if ((flags = strstr (buf, "tcp flags:")))
					flags += 10;
				if ((tos = strstr (buf, "TOS match 0x")))
					tos += 12;
				if ((state = strstr (buf, "state ")))
					state += 6;
				if ((icmp_type = strstr (buf, "icmp type "))) {
					icmp_type += 10;
					if ((icmp_type_code = strstr (buf,
					                "code ")))
						icmp_type_code += 5;
				}
				if ((tcpmss = strstr (buf, "TCPMSS clamp to ")))
					tcpmss += 16;
				else if ((tcpmss = strstr (buf, "TCPMSS set ")))
					tcpmss += 11;
				if ((mac = strstr (buf, "MAC ")))
					mac += 4;
				if (flags)
					trimcolumn(flags);
				if (sports)
					trimcolumn(sports);
				if (dports)
					trimcolumn(dports);
				if (tos)
					trimcolumn(tos);
				if (state)
					trimcolumn(state);
				if (icmp_type)
					trimcolumn(icmp_type);
				if (icmp_type_code)
					trimcolumn(icmp_type_code);
				if (tcpmss)
					trimcolumn(tcpmss);
				if (mac)
					trimcolumn(mac);

				if ((strcmp (type, "ACCEPT") == 0) || (strcmp (
				                type, "DROP") == 0) || (strcmp (
				                type, "REJECT") == 0)
				                || (strcmp (type, "LOG") == 0)
				                || (strcmp (type, "TCPMSS")
				                                == 0)) {
					if (strcmp (acl, "INPUT") != 0
					                && strcmp (acl,
					                                "OUTPUT")
					                                != 0
					                && strcmp (acl,
					                                "FORWARD")
					                                != 0) /* filter CHAINs */
					{
						if ((!aclp) && (!conf_format)) {
							fprintf (
							                F,
							                "IP access list %s\n",
							                acl);
						}
						aclp = 1;
						mcount = buf;
						if (!conf_format) {
							while (*mcount == ' ')
								++mcount;
						}
						acl_print_rule (type, prot, source,
						                dest, sports,
						                dports, acl, F,
						                conf_format,
						                atoi (mcount),
						                flags, tos,
						                state,
						                icmp_type,
						                icmp_type_code,
						                tcpmss, mac);
					}
				} else {
					if (!conf_format) {
						if (strcmp (acl, "FORWARD")) /* INTPUT || OUTPUT */
						{
							if (strcmp (input, "*")
							                && !strstr (
							                                input,
							                                "ipsec"))
								fprintf (
								                F,
								                "interface %s in access-list %s\n",
								                input,
								                type);
							if (strcmp (output, "*")
							                && !strstr (
							                                output,
							                                "ipsec"))
								fprintf (
								                F,
								                "interface %s out access-list %s\n",
								                output,
								                type);
						}
					}
				}
				destroy_args (args);
			}
		}
	}
	pclose (ipc);
}

void lconfig_acl_dump_policy (FILE *F)
{
	FILE *ipc;
	char buf[512];
	char *p;
	FILE *procfile;

	procfile = fopen ("/proc/net/ip_tables_names", "r");
	if (!procfile) {
		fprintf (F, "access-policy accept\n");
		return;
	}
	fclose (procfile);

	ipc = popen ("/bin/iptables -L -nv", "r");

	if (!ipc) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return;
	}

	while (!feof (ipc)) {
		buf[0] = 0;
		fgets (buf, sizeof(buf), ipc);
		p = strstr (buf, "policy");
		if (p) {
			if (strncasecmp (p + 7, "accept", 6) == 0) {
				fprintf (F, "access-policy accept\n");
				break;
			} else if (strncasecmp (p + 7, "drop", 4) == 0) {
				fprintf (F, "access-policy drop\n");
				break;
			}
		}
	}
	pclose (ipc);
}

int acl_exists (char *acl)
{
	FILE *f;
	char *tmp, buf[256];
	int acl_exists = 0;

	f = popen ("/bin/iptables -L -n", "r");

	if (!f) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return 0;
	}

	while (!feof (f)) {
		buf[0] = 0;
		fgets (buf, 255, f);
		buf[255] = 0;
		striplf (buf);
		if (strncmp (buf, "Chain ", 6) == 0) {
			tmp = strchr (buf + 6, ' ');
			if (tmp) {
				*tmp = 0;
				if (strcmp (buf + 6, acl) == 0) {
					acl_exists = 1;
					break;
				}
			}
		}
	}
	pclose (f);
	return acl_exists;
}

int acl_matched_exists (char *acl, char *iface_in, char *iface_out, char *chain)
{
	FILE *F;
	char *tmp, buf[256];
	int acl_exists = 0;
	int in_chain = 0;
	char *iface_in_, *iface_out_, *target;

	F = popen ("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return 0;
	}

	DEBUG_CMD("acl_matched_exists\n");

	while (!feof (F)) {
		buf[0] = 0;
		fgets (buf, 255, F);
		buf[255] = 0;
		striplf (buf);
		if (strncmp (buf, "Chain ", 6) == 0) {
			if (in_chain)
				break; // chegou `a proxima chain sem encontrar - finaliza
			tmp = strchr (buf + 6, ' ');
			if (tmp) {
				*tmp = 0;
				if (strcmp (buf + 6, chain) == 0)
					in_chain = 1;
			}
		} else if ((in_chain) && (strncmp (buf, " pkts", 5) != 0)
		                && (strlen (buf) > 40)) {
			arglist *args;
			char *p;

			p = buf;
			while ((*p) && (*p == ' '))
				p++;
			args = make_args (p);
			if (args->argc < 7) {
				destroy_args (args);
				continue;
			}
			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];
			if (((iface_in == NULL)
			                || (strcmp (iface_in_, iface_in) == 0))
			                && ((iface_out == NULL) || (strcmp (
			                                iface_out_, iface_out)
			                                == 0))
			                && ((acl == NULL) || (strcmp (target,
			                                acl) == 0))) {
				acl_exists = 1;
				destroy_args (args);
				break;
			}
			destroy_args (args);
		}
	}
	pclose (F);
	return acl_exists;
}

int lconfig_acl_get_iface_rules (char *iface, char *in_acl, char *out_acl)
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

	procfile = fopen ("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose (procfile);

	F = popen ("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return 0;
	}

	while (!feof (F)) {
		buf[0] = 0;
		fgets (buf, 255, F);
		buf[255] = 0;
		striplf (buf);
		if (strncmp (buf, "Chain ", 6) == 0) {
			if (strncmp (buf + 6, "FORWARD", 7) == 0)
				chain = chain_fwd;
			else if (strncmp (buf + 6, "INPUT", 5) == 0)
				chain = chain_in;
			else if (strncmp (buf + 6, "OUTPUT", 6) == 0)
				chain = chain_out;
			else
				chain = chain_other;
		} else if ((strncmp (buf, " pkts", 5) != 0) && (strlen (buf)
		                > 40)) {
			arglist *args;
			char *p;

			p = buf;
			while ((*p) && (*p == ' '))
				p++;
			args = make_args (p);
			if (args->argc < 7) {
				destroy_args (args);
				continue;
			}
			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];
			if ((chain == chain_fwd || chain == chain_in)
			                && (strcmp (iface, iface_in_) == 0)) {
				acl_in = target;
				strncpy (in_acl, acl_in, 100);
				in_acl[100] = 0;
			}
			if ((chain == chain_fwd || chain == chain_out)
			                && (strcmp (iface, iface_out_) == 0)) {
				acl_out = target;
				strncpy (out_acl, acl_out, 100);
				out_acl[100] = 0;
			}
			if (acl_in && acl_out)
				break;
			destroy_args (args);
		}
	}
	pclose (F);
	return 0;
}

int acl_get_refcount (char *acl)
{
	FILE *F;
	char *tmp;
	char buf[256];

	F = popen ("/bin/iptables -L -n", "r");

	if (!F) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return 0;
	}

	while (!feof (F)) {
		buf[0] = 0;
		fgets (buf, 255, F);
		buf[255] = 0;
		striplf (buf);
		if (strncmp (buf, "Chain ", 6) == 0) {
			tmp = strchr (buf + 6, ' ');
			if (tmp) {
				*tmp = 0;
				if (strcmp (buf + 6, acl) == 0) {
					tmp = strchr (tmp + 1, '(');
					if (!tmp)
						return 0;
					tmp++;
					return atoi (tmp);
				}
			}
		}
	}
	pclose (F);
	return 0;
}

int acl_clean_iface_acls (char *iface)
{
	FILE *F;
	char buf[256];
	char cmd[256];
	char chain[16];
	char *p, *iface_in_, *iface_out_, *target;
	FILE *procfile;

	procfile = fopen ("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose (procfile);

	F = popen ("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return -1;
	}

	DEBUG_CMD("clean_iface_acls\n");

	chain[0] = 0;
	while (!feof (F)) {
		buf[0] = 0;
		fgets (buf, 255, F);
		buf[255] = 0;
		striplf (buf);
		if (strncmp (buf, "Chain ", 6) == 0) {
			p = strchr (buf + 6, ' ');
			if (p) {
				*p = 0;
				strncpy (chain, buf + 6, 16);
				chain[15] = 0;
			}
		} else if ((strncmp (buf, " pkts", 5) != 0) && (strlen (buf)
		                > 40)) {
			arglist *args;

			p = buf;
			while ((*p) && (*p == ' '))
				p++;
			args = make_args (p);
			if (args->argc < 7) {
				destroy_args (args);
				continue;
			}
			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];
			if (strncmp (iface, iface_in_, strlen (iface)) == 0) {
				sprintf (
				                cmd,
				                "/bin/iptables -D %s -i %s -j %s",
				                chain, iface_in_, target);

				DEBUG_CMD(cmd);

				system (cmd);
			}

			if (strncmp (iface, iface_out_, strlen (iface)) == 0) {
				sprintf (
				                cmd,
				                "/bin/iptables -D %s -o %s -j %s",
				                chain, iface_out_, target);
				DEBUG_CMD(cmd);

				system (cmd);
			}
			destroy_args (args);
		}
	}
	pclose (F);
	return 0;
}

/* #ifdef OPTION_IPSEC Starter deve fazer uso de #ifdef */
int acl_copy_iface_acls (char *src, char *trg) /* starter/interfaces.c */
{
	FILE *F;
	char buf[256];
	char cmd[256];
	char chain[16];
	char *p, *iface_in_, *iface_out_, *target;
	FILE *procfile;

	procfile = fopen ("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose (procfile);

	F = popen ("/bin/iptables -L -nv", "r");

	if (!F) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return -1;
	}

	DEBUG_CMD("copy_iface_acls\n");

	chain[0] = 0;
	while (!feof (F)) {
		buf[0] = 0;
		fgets (buf, 255, F);
		buf[255] = 0;
		striplf (buf);
		if (strncmp (buf, "Chain ", 6) == 0) {
			p = strchr (buf + 6, ' ');
			if (p) {
				*p = 0;
				strncpy (chain, buf + 6, 16);
				chain[15] = 0;
			}
		} else if ((strncmp (buf, " pkts", 5) != 0) && (strlen (buf)
		                > 40)) {
			arglist *args;

			p = buf;
			while ((*p) && (*p == ' '))
				p++;
			args = make_args (p);
			if (args->argc < 7) {
				destroy_args (args);
				continue;
			}
			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];

			if (strncmp (src, iface_in_, strlen (src)) == 0) {
				sprintf (
				                cmd,
				                "/bin/iptables -A %s -i %s -j %s",
				                chain, trg, target);

				DEBUG_CMD(cmd);

				system (cmd);
			}
			if (strncmp (src, iface_out_, strlen (src)) == 0) {
				sprintf (
				                cmd,
				                "/bin/iptables -A %s -o %s -j %s",
				                chain, trg, target);

				DEBUG_CMD(cmd);

				system (cmd);
			}

			destroy_args (args);
		}
	}
	pclose (F);
	return 0;
}

#ifdef OPTION_IPSEC
int acl_interface_ipsec(int add_del, int chain, char *dev, char *listno)
{
	int i;
	char filename[32], ipsec[16], iface[16], buf[256];
	FILE *f;

	for (i=0; i < N_IPSEC_IF; i++)
	{
		sprintf(ipsec, "ipsec%d", i);
		strcpy(filename, "/var/run/");
		strcat(filename, ipsec);
		if ((f=fopen(filename, "r")) != NULL)
		{
			fgets(iface, 16, f);
			fclose(f);
			striplf(iface);
			if (strcmp(iface, dev) == 0) /* found ipsec attach to dev */
			{
				if (add_del)
				{
					if (chain == chain_in)
					{
						sprintf(buf, "/bin/iptables -A INPUT -i %s -j %s", ipsec, listno);
						DEBUG_CMD(buf);
						system(buf);

						sprintf(buf, "/bin/iptables -A FORWARD -i %s -j %s", ipsec, listno);
						DEBUG_CMD(buf);
						system(buf);
					}
					else
					{
						sprintf(buf, "/bin/iptables -A OUTPUT -o %s -j %s", ipsec, listno);
						DEBUG_CMD(buf);
						system(buf);

						sprintf(buf, "/bin/iptables -A FORWARD -o %s -j %s", ipsec, listno);
						DEBUG_CMD(buf);
						system(buf);
					}
				}
				else
				{
					if ((chain == chain_in) || (chain == chain_both))
					{
						if (acl_matched_exists(listno, ipsec, 0, "INPUT"))
						{
							snprintf(buf, 256, "/bin/iptables -D INPUT -i %s -j %s", ipsec, listno);
							DEBUG_CMD(buf);
							system(buf);
						}
						if (acl_matched_exists(listno, ipsec, 0, "FORWARD"))
						{
							snprintf(buf, 256, "/bin/iptables -D FORWARD -i %s -j %s", ipsec, listno);
							DEBUG_CMD(buf);
							system(buf);
						}
					}
					if ((chain == chain_out) || (chain == chain_both))
					{
						if (acl_matched_exists(listno, 0, ipsec, "OUTPUT"))
						{
							snprintf(buf, 256, "/bin/iptables -D OUTPUT -o %s -j %s", ipsec, listno);
							DEBUG_CMD(buf);
							system(buf);
						}
						if (acl_matched_exists(listno, 0, ipsec, "FORWARD"))
						{
							snprintf(buf, 256, "/bin/iptables -D FORWARD -o %s -j %s", ipsec, listno);
							DEBUG_CMD(buf);
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
#endif

int delete_module (const char *name); /* libbb ? */

void acl_cleanup_modules (void)
{
	delete_module (NULL); /* clean unused modules! */
}

