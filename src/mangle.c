/*
 * mangle.c
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
#include "ip.h"
#include "qos.h"

static void print_mangle (const char *action,
                          const char *dscp,
                          const char *mark,
                          const char *proto,
                          const char *src,
                          const char *dst,
                          const char *sports,
                          const char *dports,
                          char *mangle,
                          FILE *out,
                          int conf_format,
                          int mc,
                          char *flags,
                          char *tos,
                          char *dscp_match,
                          char *state,
                          char *icmp_type,
                          char *icmp_type_code)
{
	char src_ports[32];
	char dst_ports[32];
	char *_src;
	char *_dst;
	const char *src_netmask;
	const char *dst_netmask;
	const char *dscp_class;

	_src = strdup (src);
	_dst = strdup (dst);
	src_ports[0] = 0;
	dst_ports[0] = 0;
	src_netmask = extract_mask (_src);
	dst_netmask = extract_mask (_dst);
	libconfig_acl_set_ports (sports, src_ports);
	libconfig_acl_set_ports (dports, dst_ports);
	if (conf_format)
		fprintf (out, "mark-rule ");
	if (conf_format)
		fprintf (out, "%s ", mangle);
	else
		fprintf (out, "    ");
	if (strcmp (action, "DSCP") == 0 && dscp) {
		dscp_class = dscp_to_name (strtol (dscp, NULL, 16));
		if (dscp_class)
			fprintf (out, "dscp class %s ", dscp_class);
		else
			fprintf (out, "dscp %ld ", strtol (dscp, NULL, 16));
	} else if (strcmp (action, "MARK") == 0) {
		fprintf (out, "mark ");
		if (mark)
			fprintf (out, "%ld ", strtol (mark, NULL, 16));
	} else
		fprintf (out, "???? ");
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
				libconfig_acl_print_flags (out, t);
				fprintf (out, "/");
				t = strtok (NULL, "/");
				libconfig_acl_print_flags (out, t);
				fprintf (out, " ");
			}
		} else
			fprintf (out, "flags syn ");
	}
	if (tos)
		fprintf (out, "tos %ld ", strtol (tos, NULL, 16));
	if (dscp_match) {
		dscp_class = dscp_to_name (strtol (dscp_match, NULL, 16));
		if (dscp_class)
			fprintf (out, "dscp class %s ", dscp_class);
		else
			fprintf (out, "dscp %ld ", strtol (dscp_match, NULL, 16));
	}
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

/**
 * 	# iptables -n -L -v -t mangle
 * 	Chain PREROUTING (policy ACCEPT 0 packets, 0 bytes)
 * 	pkts bytes target     prot opt in     out     source               destination
 * 	0     0 MARK       all  --  *      *       0.0.0.0/0            0.0.0.0/0           MARK set 0x1
 * 	0     0 DSCP       all  --  *      *       0.0.0.0/0            0.0.0.0/0           DSCP set 0x01
 * 	0     0 MARK       all  --  *      *       0.0.0.0/0            0.0.0.0/0           DSCP match 0x01 MARK set 0x2
 */
#define trimcolumn(x) tmp=strchr(x, ' '); if (tmp != NULL) *tmp=0;
void lconfig_mangle_dump(char *xmangle, FILE *F, int conf_format)
{
	FILE *ipc;
	char *tmp;
	char mangle[101];
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
	char *mark = NULL;
	char *dscp = NULL;
	char *dscp_match = NULL;
	char *state = NULL;
	char *icmp_type = NULL;
	char *icmp_type_code = NULL;
	char *mcount; /* pkts */
	int manglep = 1;
	FILE *procfile;
	char buf[512];

	procfile = fopen ("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return;
	fclose (procfile);

	mangle[0] = 0;

	ipc = popen ("/bin/iptables -t mangle -L -nv", "r");

	if (!ipc) {
		fprintf (stderr, "%% ACL subsystem not found\n");
		return;
	}

	while (!feof (ipc)) {
		buf[0] = 0;
		fgets (buf, sizeof(buf) - 1, ipc);
		tmp = strchr (buf, '\n');
		if (tmp)
			*tmp = 0;

		if (strncmp (buf, "Chain ", 6) == 0) {
			//if ((conf_format) && (manglep)) pfprintf(F, "!\n");
			trimcolumn(buf+6);
			strncpy (mangle, buf + 6, 100);
			mangle[100] = 0;
			manglep = 0;
		} else if (strncmp (buf, " pkts", 5) != 0) {
			if ((strlen (buf)) && ((xmangle == NULL) || (strcmp (
			                xmangle, mangle) == 0))) {
				arglist *args;
				char *p;

				p = buf;
				while ((*p) && (*p == ' '))
					p++;
				args = make_args (p);
				if (args->argc < 9) {
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
				if ((mark = strstr (buf, "MARK xset 0x")))
					mark += 12;
				if ((dscp = strstr (buf, "DSCP set 0x")))
					dscp += 11;
				if ((dscp_match = strstr (buf, "DSCP match 0x")))
					dscp_match += 13;
				if ((state = strstr (buf, "state ")))
					state += 6;
				if ((icmp_type = strstr (buf, "icmp type "))) {
					icmp_type += 10;
					if ((icmp_type_code = strstr (buf,
					                "code ")))
						icmp_type_code += 5;
				}
				if (flags)
					trimcolumn(flags);
				if (sports)
					trimcolumn(sports);
				if (dports)
					trimcolumn(dports);
				if (tos)
					trimcolumn(tos);
				if (mark)
					trimcolumn(mark);
				if (dscp)
					trimcolumn(dscp);
				if (dscp_match)
					trimcolumn(dscp_match);
				if (state)
					trimcolumn(state);
				if (icmp_type)
					trimcolumn(icmp_type);
				if (icmp_type_code)
					trimcolumn(icmp_type_code);

				if ((strcmp (type, "DSCP") == 0) || (strcmp (
				                type, "MARK") == 0)) {
					if (strcmp (mangle, "INPUT") != 0
					                && strcmp (mangle,
					                                "PREROUTING")
					                                != 0
					                && strcmp (mangle,
					                                "OUTPUT")
					                                != 0
					                && strcmp (mangle,
					                                "POSTROUTING")
					                                != 0) /* filter CHAINs */
					{
						if ((!manglep)
						                && (!conf_format)) {
							fprintf (
							                F,
							                "MARK rule %s\n",
							                mangle);
						}
						manglep = 1;
						mcount = buf;
						if (!conf_format) {
							while (*mcount == ' ')
								++mcount; /* pkts */
						}
						print_mangle (type, dscp, mark,
						                prot, source,
						                dest, sports,
						                dports, mangle,
						                F, conf_format,
						                atoi (mcount),
						                flags, tos,
						                dscp_match,
						                state,
						                icmp_type,
						                icmp_type_code);
					}
				} else {
					if (!conf_format) {
						if (strstr (mangle, "ROUTING")) /* PRE|POST ROUTING */
						{
							if (strcmp (input, "*"))
								fprintf (
								                F,
								                "interface %s in mark-rule %s\n",
								                input,
								                type);
							if (strcmp (output, "*"))
								fprintf (
								                F,
								                "interface %s out mark-rule %s\n",
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
	if (conf_format)
		fprintf (F, "!\n"); /* ! for next: router rip */
}

int lconfig_mangle_get_iface_rules (char *iface, char *in_mangle, char *out_mangle)
{
	typedef enum {
		chain_in, chain_out, chain_other
	} mangle_chain;
	FILE *F;
	char buf[256];
	mangle_chain chain = chain_in;
	char *iface_in_, *iface_out_, *target;
	char *mangle_in = NULL, *mangle_out = NULL;
	FILE *procfile;

	procfile = fopen ("/proc/net/ip_tables_names", "r");
	if (!procfile)
		return 0;
	fclose (procfile);

	F = popen ("/bin/iptables -t mangle -L -nv", "r");

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
			if (strncmp (buf + 6, "INPUT", 5) == 0)
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
			if ((chain == chain_in) && (strcmp (iface, iface_in_)
			                == 0)) {
				mangle_in = target;
				strncpy (in_mangle, mangle_in, 100);
				in_mangle[100] = 0;
			}
			if ((chain == chain_out) && (strcmp (iface, iface_out_)
			                == 0)) {
				mangle_out = target;
				strncpy (out_mangle, mangle_out, 100);
				out_mangle[100] = 0;
			}
			if (mangle_in && mangle_out)
				break;
			destroy_args (args);
		}
	}
	pclose (F);
	return 0;
}
