#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <cish/options.h>
#include <libconfig/defines.h>
#include <libconfig/acl.h>
#include <libconfig/args.h>
#include <libconfig/str.h>
#include <libconfig/exec.h>
#include <libconfig/system.h>

//#define DEBUG_CMD(x) printf("cmd = %s\n", x)
//#define DEBUG_CMD(x) syslog(LOG_DEBUG, "%s\n", x)
#define DEBUG_CMD(x)

int acl_exists(char *acl)
{
	FILE *F;
	char *tmp, buf[256];
	int acl_exists=0;
#ifdef CHECK_IPTABLES_EXEC
	char iptline[256], randomfile[FILE_TMP_IPT_ERRORS_LEN];
#endif

#ifdef CHECK_IPTABLES_EXEC
	/* Temporary file for errors */
	strcpy(iptline, "/bin/iptables -L -n");
	connect_error_file(iptline, randomfile); /* Temporary file for errors */
	F=popen(iptline, "r");
#else
	F=popen("/bin/iptables -L -n", "r");
#endif
	if (!F)
	{
		fprintf(stderr, "%% ACL subsystem not found\n");
		return 0;
	}
#ifdef CHECK_IPTABLES_EXEC
	if( !is_iptables_exec_valid(randomfile) )
	{
		fprintf(stderr, "%% ACL subsystem not found\n");
		pclose(F);
		return 0;
	}
#endif
	while (!feof(F))
	{
		buf[0]=0;
		fgets(buf, 255, F);
		buf[255]=0;
		striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0)
		{
			tmp=strchr(buf+6, ' ');
			if (tmp) {
				*tmp=0;
				if (strcmp(buf+6, acl) == 0) {
					acl_exists=1;
					break;
				}
			}
		}
	}
	pclose(F);
	return acl_exists;
}

int matched_acl_exists(char *acl, char *iface_in, char *iface_out, char *chain)
{
	FILE *F;
	char *tmp, buf[256];
	int acl_exists=0;
	int in_chain=0;
	char *iface_in_, *iface_out_, *target;
#ifdef CHECK_IPTABLES_EXEC
	char iptline[256], randomfile[FILE_TMP_IPT_ERRORS_LEN];
#endif

#ifdef CHECK_IPTABLES_EXEC
	/* Temporary file for errors */
	strcpy(iptline, "/bin/iptables -L -nv");
	connect_error_file(iptline, randomfile); /* Temporary file for errors */
	F=popen(iptline, "r");
#else
	F=popen("/bin/iptables -L -nv", "r");
#endif
	if (!F)
	{
		fprintf (stderr, "%% ACL subsystem not found\n");
		return 0;
	}
#ifdef CHECK_IPTABLES_EXEC
	if( !is_iptables_exec_valid(randomfile) )
	{
		fprintf(stderr, "%% ACL subsystem not found\n");
		pclose(F);
		return 0;
	}
#endif
	DEBUG_CMD("matched_acl_exists\n");
	while (!feof(F))
	{
		buf[0]=0;
		fgets(buf, 255, F);
		buf[255]=0;
		striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0)
		{
			if (in_chain)
				break; // chegou `a proxima chain sem encontrar - finaliza
			tmp=strchr(buf+6, ' ');
			if (tmp) {
				*tmp=0;
				if (strcmp(buf+6, chain) == 0)
					in_chain=1;
			}
		}
		else if ((in_chain) && (strncmp(buf, " pkts", 5) != 0) && (strlen(buf)>40))
		{
			arglist *args;
			char *p;

			p=buf; while ((*p)&&(*p==' ')) p++;
			args=make_args(p);
			if (args->argc < 7)
			{
				destroy_args(args);
				continue;
			}
			iface_in_=args->argv[5];
			iface_out_=args->argv[6];
			target=args->argv[2];
			if (((iface_in==NULL) ||(strcmp(iface_in_,  iface_in )==0)) &&
			    ((iface_out==NULL)||(strcmp(iface_out_, iface_out)==0)) &&
			    ((acl==NULL)      ||(strcmp(target,     acl      )==0)))
			{
				acl_exists=1;
				destroy_args(args);
				break;
			}
			destroy_args(args);
		}
	}
	pclose(F);
	return acl_exists;
}

int get_iface_acls(char *iface, char *in_acl, char *out_acl)
{
	typedef enum {chain_fwd, chain_in, chain_out, chain_other} acl_chain;
	FILE *F;
	char buf[256];
	acl_chain chain=chain_fwd;
	char *iface_in_, *iface_out_, *target;
	char *acl_in=NULL, *acl_out=NULL;
	FILE *procfile;
#ifdef CHECK_IPTABLES_EXEC
	char iptline[256], randomfile[FILE_TMP_IPT_ERRORS_LEN];
#endif

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile) return 0;
	fclose(procfile);

#ifdef CHECK_IPTABLES_EXEC
	/* Temporary file for errors */
	strcpy(iptline, "/bin/iptables -L -nv");
	connect_error_file(iptline, randomfile); /* Temporary file for errors */
	F=popen(iptline, "r");
#else
	F = popen("/bin/iptables -L -nv", "r");
#endif
	if (!F)
	{
		fprintf (stderr, "%% ACL subsystem not found\n");
		return 0;
	}
#ifdef CHECK_IPTABLES_EXEC
	if( !is_iptables_exec_valid(randomfile) )
	{
		fprintf(stderr, "%% ACL subsystem not found\n");
		pclose(F);
		return 0;
	}
#endif
	while (!feof(F))
	{
		buf[0]=0;
		fgets(buf, 255, F);
		buf[255]=0;
		striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0)
		{
			if (strncmp(buf+6, "FORWARD", 7) == 0) chain=chain_fwd;
				else if (strncmp(buf+6, "INPUT", 5) == 0) chain=chain_in;
					else if (strncmp(buf+6, "OUTPUT", 6) == 0) chain=chain_out;
						else chain=chain_other;
		}
		else if ((strncmp(buf, " pkts", 5)!=0) && (strlen(buf) > 40))
		{
			arglist	*args;
			char *p;

			p=buf; while ((*p)&&(*p==' ')) p++;
			args=make_args(p);
			if (args->argc < 7)
			{
				destroy_args(args);
				continue;
			}
			iface_in_=args->argv[5];
			iface_out_=args->argv[6];
			target=args->argv[2];
			if ((chain == chain_fwd || chain == chain_in) && (strcmp(iface, iface_in_ ) == 0))
			{
				acl_in=target;
				strncpy(in_acl, acl_in, 100);
				in_acl[100]=0;
			}
			if ((chain == chain_fwd || chain == chain_out) && (strcmp(iface, iface_out_ ) == 0))
			{
				acl_out=target;
				strncpy(out_acl, acl_out, 100);
				out_acl[100]=0;
			}
			if (acl_in && acl_out) break;
			destroy_args(args);
		}
	}
	pclose(F);
	return 0;
}

int get_acl_refcount(char *acl)
{
	FILE *F;
	char *tmp;
	char buf[256];
#ifdef CHECK_IPTABLES_EXEC
	char iptline[256], randomfile[FILE_TMP_IPT_ERRORS_LEN];
#endif

#ifdef CHECK_IPTABLES_EXEC
	/* Temporary file for errors */
	strcpy(iptline, "/bin/iptables -L -n");
	connect_error_file(iptline, randomfile); /* Temporary file for errors */
	F=popen(iptline, "r");
#else
	F=popen("/bin/iptables -L -n", "r");
#endif
	if (!F)
	{
		fprintf(stderr, "%% ACL subsystem not found\n");
		return 0;
	}
#ifdef CHECK_IPTABLES_EXEC
	if( !is_iptables_exec_valid(randomfile) )
	{
		fprintf(stderr, "%% ACL subsystem not found\n");
		pclose(F);
		return 0;
	}
#endif
	while (!feof(F))
	{
		buf[0]=0;
		fgets(buf, 255, F);
		buf[255]=0;
		striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0)
		{
			tmp=strchr(buf+6, ' ');
			if (tmp) {
				*tmp=0;
				if (strcmp(buf+6, acl) == 0) {
					tmp=strchr(tmp+1, '(');
					if (!tmp)
						return 0;
					tmp++;
					return atoi(tmp);
				}
			}
		}
	}
	pclose (F);
	return 0;
}

int clean_iface_acls(char *iface)
{
	FILE *F;
	char buf[256];
	char cmd[256];
	char chain[16];
	char *p, *iface_in_, *iface_out_, *target;
	FILE *procfile;
#ifdef CHECK_IPTABLES_EXEC
	char iptline[256], randomfile[FILE_TMP_IPT_ERRORS_LEN];
#endif

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile) return 0;
	fclose(procfile);

#ifdef CHECK_IPTABLES_EXEC
	/* Temporary file for errors */
	strcpy(iptline, "/bin/iptables -L -nv");
	connect_error_file(iptline, randomfile); /* Temporary file for errors */
	F=popen(iptline, "r");
#else
	F=popen("/bin/iptables -L -nv", "r");
#endif
	if (!F)
	{
		fprintf (stderr, "%% ACL subsystem not found\n");
		return -1;
	}
#ifdef CHECK_IPTABLES_EXEC
	if( !is_iptables_exec_valid(randomfile) )
	{
		fprintf(stderr, "%% ACL subsystem not found\n");
		pclose(F);
		return 0;
	}
#endif
	DEBUG_CMD("clean_iface_acls\n");
	chain[0]=0;
	while (!feof(F))
	{
		buf[0]=0;
		fgets(buf, 255, F);
		buf[255]=0;
		striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0)
		{
			p=strchr(buf+6, ' ');
			if (p)
			{
				*p=0;
				strncpy(chain, buf+6, 16);
				chain[15]=0;
			}
		}
		else if ((strncmp(buf, " pkts", 5) != 0)&&(strlen(buf) > 40))
		{
			arglist	*args;

			p=buf; while ((*p)&&(*p==' ')) p++;
			args=make_args(p);
			if (args->argc < 7)
			{
				destroy_args(args);
				continue;
			}
			iface_in_ = args->argv[5];
			iface_out_ = args->argv[6];
			target = args->argv[2];
			if (strncmp(iface, iface_in_, strlen(iface)) == 0)
			{
				sprintf(cmd, "/bin/iptables -D %s -i %s -j %s", chain, iface_in_, target);
#ifdef CHECK_IPTABLES_EXEC
				connect_error_file(cmd, randomfile); /* Temporary file for errors */
#endif
				DEBUG_CMD(cmd);
				system(cmd);
#ifdef CHECK_IPTABLES_EXEC
				if( !is_iptables_exec_valid(randomfile) )
				{
					fprintf(stderr, "%% Not possible to execute action\n");
					destroy_args(args);
					pclose(F);
					return 0;
				}
#endif
			}
			if (strncmp(iface, iface_out_, strlen(iface)) == 0)
			{
				sprintf(cmd, "/bin/iptables -D %s -o %s -j %s", chain, iface_out_, target);
#ifdef CHECK_IPTABLES_EXEC
				connect_error_file(cmd, randomfile); /* Temporary file for errors */
#endif
				DEBUG_CMD(cmd);
				system(cmd);
#ifdef CHECK_IPTABLES_EXEC
				if( !is_iptables_exec_valid(randomfile) )
				{
					fprintf(stderr, "%% Not possible to execute action\n");
					destroy_args(args);
					pclose(F);
					return 0;
				}
#endif
			}
			destroy_args(args);
		}
	}
	pclose(F);
	return 0;
}

/* #ifdef OPTION_IPSEC Starter deve fazer uso de #ifdef */
int copy_iface_acls(char *src, char *trg) /* starter/interfaces.c */
{
	FILE *F;
	char buf[256];
	char cmd[256];
	char chain[16];
	char *p, *iface_in_, *iface_out_, *target;
	FILE *procfile;
#ifdef CHECK_IPTABLES_EXEC
	char iptline[256], randomfile[FILE_TMP_IPT_ERRORS_LEN];
#endif

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile) return 0;
	fclose(procfile);

#ifdef CHECK_IPTABLES_EXEC
	/* Temporary file for errors */
	strcpy(iptline, "/bin/iptables -L -nv");
	connect_error_file(iptline, randomfile); /* Temporary file for errors */
	F=popen(iptline, "r");
#else
	F=popen("/bin/iptables -L -nv", "r");
#endif
	if (!F)
	{
		fprintf (stderr, "%% ACL subsystem not found\n");
		return -1;
	}
#ifdef CHECK_IPTABLES_EXEC
	if( !is_iptables_exec_valid(randomfile) )
	{
		fprintf (stderr, "%% ACL subsystem not found\n");
		pclose(F);
		return -1;
	}
#endif
	DEBUG_CMD("copy_iface_acls\n");
	chain[0]=0;
	while (!feof(F))
	{
		buf[0]=0;
		fgets(buf, 255, F);
		buf[255]=0;
		striplf(buf);
		if (strncmp(buf, "Chain ", 6) == 0)
		{
			p=strchr(buf+6, ' ');
			if (p)
			{
				*p=0;
				strncpy(chain, buf+6, 16);
				chain[15]=0;
			}
		}
		else if ((strncmp(buf, " pkts", 5) != 0)&&(strlen(buf) > 40))
		{
			arglist	*args;

			p=buf; while ((*p)&&(*p==' ')) p++;
			args=make_args(p);
			if (args->argc < 7)
			{
				destroy_args(args);
				continue;
			}
			iface_in_=args->argv[5];
			iface_out_=args->argv[6];
			target=args->argv[2];
#if 1 /* Enable ipsecx acl rules */
			if (strncmp(src, iface_in_, strlen(src)) == 0)
			{
				sprintf(cmd, "/bin/iptables -A %s -i %s -j %s", chain, trg, target);
#ifdef CHECK_IPTABLES_EXEC
				connect_error_file(cmd, randomfile); /* Temporary file for errors */
#endif
				DEBUG_CMD(cmd);
				system(cmd);
#ifdef CHECK_IPTABLES_EXEC
				if( !is_iptables_exec_valid(randomfile) )
				{
					fprintf (stderr, "%% Not possible to execute action\n");
					destroy_args(args);
					pclose(F);
					return -1;
				}
#endif
			}
			if (strncmp(src, iface_out_, strlen(src)) == 0)
			{
				sprintf(cmd, "/bin/iptables -A %s -o %s -j %s", chain, trg, target);
#ifdef CHECK_IPTABLES_EXEC
				connect_error_file(cmd, randomfile); /* Temporary file for errors */
#endif
				DEBUG_CMD(cmd);
				system(cmd);
#ifdef CHECK_IPTABLES_EXEC
				if( !is_iptables_exec_valid(randomfile) )
				{
					fprintf (stderr, "%% Not possible to execute action\n");
					destroy_args(args);
					pclose(F);
					return -1;
				}
#endif
			}
#endif
			destroy_args(args);
		}
	}
	pclose(F);
	return 0;
}

#ifdef OPTION_IPSEC
int interface_ipsec_acl(int add_del, int chain, char *dev, char *listno)
{
	int i;
	char filename[32], ipsec[16], iface[16], buf[256];
	FILE *f;
#ifdef CHECK_IPTABLES_EXEC
	char randomfile[FILE_TMP_IPT_ERRORS_LEN];
#endif

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
#if 1 /* Enable ipsec acl rules */
				if (add_del)
				{
					if (chain == chain_in)
					{
						sprintf(buf, "/bin/iptables -A INPUT -i %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
						connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
						DEBUG_CMD(buf);
						system(buf);
#ifdef CHECK_IPTABLES_EXEC
						if( !is_iptables_exec_valid(randomfile) )
						{
							return -1;
						}
#endif

						sprintf(buf, "/bin/iptables -A FORWARD -i %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
						connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
						DEBUG_CMD(buf);
						system(buf);
#ifdef CHECK_IPTABLES_EXEC
						if( !is_iptables_exec_valid(randomfile) )
						{
							return -1;
						}
#endif
					}
					else
					{
						sprintf(buf, "/bin/iptables -A OUTPUT -o %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
						connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
						DEBUG_CMD(buf);
						system(buf);
#ifdef CHECK_IPTABLES_EXEC
						if( !is_iptables_exec_valid(randomfile) )
						{
							return -1;
						}
#endif

						sprintf(buf, "/bin/iptables -A FORWARD -o %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
						connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
						DEBUG_CMD(buf);
						system(buf);
#ifdef CHECK_IPTABLES_EXEC
						if( !is_iptables_exec_valid(randomfile) )
						{
							return -1;
						}
#endif
					}
				}
				else
				{
					if ((chain == chain_in) || (chain == chain_both))
					{
						if (matched_acl_exists(listno, ipsec, 0, "INPUT"))
						{
							snprintf(buf, 256, "/bin/iptables -D INPUT -i %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
							connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
							DEBUG_CMD(buf);
							system(buf);
#ifdef CHECK_IPTABLES_EXEC
							if( !is_iptables_exec_valid(randomfile) )
							{
								return -1;
							}
#endif
						}
						if (matched_acl_exists(listno, ipsec, 0, "FORWARD"))
						{
							snprintf(buf, 256, "/bin/iptables -D FORWARD -i %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
							connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
							DEBUG_CMD(buf);
							system(buf);
#ifdef CHECK_IPTABLES_EXEC
							if( !is_iptables_exec_valid(randomfile) )
							{
								return -1;
							}
#endif
						}
					}
					if ((chain == chain_out) || (chain == chain_both))
					{
						if (matched_acl_exists(listno, 0, ipsec, "OUTPUT"))
						{
							snprintf(buf, 256, "/bin/iptables -D OUTPUT -o %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
							connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
							DEBUG_CMD(buf);
							system(buf);
#ifdef CHECK_IPTABLES_EXEC
							if( !is_iptables_exec_valid(randomfile) )
							{
								return -1;
							}
#endif
						}
						if (matched_acl_exists(listno, 0, ipsec, "FORWARD"))
						{
							snprintf(buf, 256, "/bin/iptables -D FORWARD -o %s -j %s", ipsec, listno);
#ifdef CHECK_IPTABLES_EXEC
							connect_error_file(buf, randomfile); /* Temporary file for errors */
#endif
							DEBUG_CMD(buf);
							system(buf);
#ifdef CHECK_IPTABLES_EXEC
							if( !is_iptables_exec_valid(randomfile) )
							{
								return -1;
							}
#endif
						}
					}
				}
#endif
				return 0;
			}
		}
	}
	return -1;
}
#endif

int delete_module(const char *name);

void cleanup_modules(void)
{
	delete_module(NULL); /* clean unused modules! */
}

