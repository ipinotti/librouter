#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "options.h"
#include "defines.h"
#include "device.h"
#include "exec.h"
#include "ipsec.h"
#include "str.h"
#include "nv.h"
#include "error.h"
#include "quagga.h"
#include "process.h"
#include "ppp.h"
#include "dhcp.h"

#ifdef OPTION_IPSEC
int get_starter_pid(void)
{
	int i;
	FILE *f=fopen(FILE_STARTER_PID, "r");

	if (f)
	{
		fscanf(f, "%u\n", &i);
		fclose(f);
		return i;
	}
	return 0;
}
	   
int get_ipsec(void)
{
	struct stat st;

	if ((stat(FILE_STARTER_PID, &st) == 0)
		&& (stat(FILE_PLUTO_PID, &st) == 0)) return 1; else return 0;
}

int set_ipsec(int opt)
{
	int i, pid, ret=0;

	pid=get_starter_pid();
	switch(opt)
	{
		case START:
			ret=init_program(1, PROG_STARTER);
			break;
		case STOP:
			ret=init_program(0, PROG_STARTER);
			for (i=0; i < 5; i++)
			{
				if (!get_ipsec()) break;
				sleep(1); /* Wait... */
			}
			break;
		case RESTART:
			if (pid) ret=kill(pid, SIGHUP);
			break;
		case RELOAD:
			if (pid) ret=kill(pid, SIGUSR1);
			break;
	}
	return ret;
}

int get_l2tp_pid(void)
{
	int i;
	FILE *f=fopen(FILE_L2TPD_PID, "r");

	if (f)
	{
		fscanf(f, "%u\n", &i);
		fclose(f);
		return i;
	}
	return 0;
}

int get_l2tp(void)
{
	struct stat st;

	if (stat(FILE_L2TPD_PID, &st) == 0) return 1;
		else return 0;
}

int set_l2tp(int opt)
{
	int pid;
	int ret=0;

	pid=get_l2tp_pid();
	switch(opt)
	{
		case START:
			if (!pid) ret=init_program(1, PROG_L2TPD);
			break;
		case STOP:
			if (pid) ret=init_program(0, PROG_L2TPD);
			break;
		case RESTART:
			if (pid) ret=kill(pid, SIGHUP);
			break;
#if 0
		case RELOAD:
			if (pid) ret=kill(pid, SIGUSR1);
			break;
#endif
	}
	return ret;
}

int create_ipsec_conf(void)
{
	int fd;
	char buf[]="config setup\n"
		"\tklipsdebug= none\n"
		"\tplutodebug= none\n"
		"\tuniqueids= yes\n"
		"\tnat_traversal= no\n"
		"\toverridemtu= 0\n"
		"\tauto_reload= 180\n"
		"conn %default\n"
		"\tkeyingtries=0\n";

	if ((fd=open(FILE_IPSEC_CONF, O_RDWR|O_CREAT, 0600)) < 0) return -1;
	write(fd, buf, strlen(buf));
	lseek(fd, 0, SEEK_SET);
	return fd;
}

int set_ipsec_connection(int add_del, char *key, int fd)
{
	int ret=0;
	struct stat st;
	char *p, *buf;

	if (fstat(fd, &st) < 0)
	{
		pr_error(1, "fstat");
		return -1;
	}
	if (!(buf=malloc(st.st_size+1)))
	{
		pr_error(1, "malloc");
		return -1;
	}
	read(fd, buf, st.st_size); /* read all data */
	*(buf+st.st_size)=0;
	if ((p=strstr(buf, key)) != NULL)
	{
		if (!add_del)
		{
			lseek(fd, 0, SEEK_SET); /* rewind! */
			write(fd, buf, p-buf);
			write(fd, p+strlen(key), st.st_size-(p+strlen(key)-buf)); /* remove key */
			if (ftruncate(fd, lseek(fd, 0, SEEK_CUR)) < 0) /* clean file */
			{
				pr_error(1, "ftruncate");
				ret=-1;
			}
		}
	}
	else
	{
		if (add_del) write(fd, key, strlen(key)); /* add key */
	}
	free(buf);
	return ret;
}

#if 0 /* !!! */
		sprintf(filename, "/bin/echo %s >> %s", buf, FILE_IPSEC_CONF);
		if(!(file=popen(filename, "r")))
		{
			printf("%% Not possible to execute line '%s'!\n", filename);
			pclose(file);
			return -1;
		}
		pclose(file);
#endif

int create_rsakey(int keysize)
{
	FILE *f;
	int ret;
	long size;
	char *buf, line[128];
	struct stat st;

	#if 1
	sprintf(line, "/lib/ipsec/rsasigkey --random /dev/urandom %d > %s", keysize, FILE_TMP_RSAKEYS);
	#else
	sprintf(line, "/lib/ipsec/rsasigkey %d > %s", keysize, FILE_TMP_RSAKEYS);
	#endif
	system(line);

	if (stat(FILE_TMP_RSAKEYS, &st) != 0) return -1;
	if (!(f=fopen(FILE_TMP_RSAKEYS, "rt"))) return -1;
	fseek(f, 0, SEEK_END);
	size=ftell(f);
	fseek(f, 0, SEEK_SET); 
	if ((buf=malloc(size+1)) == NULL)
	{
		pr_error(1, "unable to allocate memory");
		fclose(f);
		return -1;
	}
	fread(buf, size, 1, f);
	buf[size]=0;
	fclose(f);
	unlink(FILE_TMP_RSAKEYS);
	if (set_stored_secret(buf) < 0)
	{
		ret=-1;
		pr_error(1, "unable to save key");
	}
		else ret=0;
	free(buf);
	return ret;
}

int get_ipsec_auth(char *ipsec_conn, char *buf)
{
	int ret;
	char *p, filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	ret=find_string_in_file_nl(filename, STRING_IPSEC_AUTHBY, buf, MAX_CMD_LINE);
	if (ret < 0) return ret;
	if ((p=strstr(buf, "rsasig"))) return RSA;
	else
	{
		if ((p=strstr(buf, "secret"))) return SECRET;
	}
	return 0;
}

char *get_rsakeys_config(const char *file_name)
{
	FILE *file;
	int len=0, tp;
	char *p, *buf=NULL, line[1024], last_line_flag[]="Coefficient: ";
	
	line[1023] = '\0';
	file = fopen(file_name, "r");
	if(file)
	{
		while(!feof(file))
		{
			fgets(line, 1023, file);
			if(strncmp(line, "rsa_keys", 8) == 0)
			{
				len = 0;
				while(!feof(file))
				{
					if(strstr(line, last_line_flag))
					{
						while(!feof(file))
						{
							if((tp=strlen(line)) < 1023)
							{
								len += tp;
								goto next_step;
							}
							len += tp;
							fgets(line, 1023, file);
						}
						goto next_step;
					}
					else	len += 1023;
					fgets(line, 1023, file);
				}
			}
		}
next_step:
		if(len>0)
		{
			fseek(file, 0, SEEK_SET);
			if((buf=malloc(len+1)) != NULL)
			{
				memset(buf, '\0', len+1);
				while(!feof(file))
				{
					fgets(line, 1023, file);
					if(strncmp(line, "rsa_keys", 8) == 0)
					{
						p = strstr(line, "rsa_keys") + 9;
						strcat(buf, p);
						while(!feof(file))
						{
							fgets(line, 1023, file);
							strcat(buf, line);
							if(strchr(line, '\0'))	goto go_out;
						}
					}
				}
			}
		}
go_out:
		fclose(file);
		if (buf && strlen(buf) > 0) return buf;
	}
		else printf("%% Could not find file: %s\n", file_name);
	return NULL;
}

int set_ipsec_rsakey(char *ipsec_conn, char *token, char *rsakey)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	return replace_string_in_file_nl(filename, token, rsakey);
}

/*
 *  Type:
 *     - 0,  secret
 *     - 1,  rsa
 */
int create_secrets_conn_file(char *name, int type, char *shared)
{
	int fd;
	char *rsa, filename[60], buf[MAX_KEY_SIZE];

	sprintf(filename, FILE_CONN_SECRETS, name);
	if(!(fd=open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0600)))
	{
		fprintf(stderr, "Could not create secret file\n");
		return -1;
	}
	if (type)
	{
		char token1[]=": RSA	{\n";
		char token2[]="	}\n";

		if ((rsa=get_rsakeys_from_nv()) == NULL)
		{
			fprintf(stderr, "%% ERROR: You must create RSA keys first (key generate rsa 1024).\n");
			close(fd);
			return -1;
		}
		write(fd, token1, strlen(token1));
		write(fd, rsa, strlen(rsa));
		write(fd, token2, strlen(token2));
		write(fd, '\0', 1);
		close(fd);
		free(rsa);
		/* copia a chave publica para o arquivo de configuracao da conexao */
		if (find_string_in_file_nl(filename, "#pubkey", buf, MAX_KEY_SIZE) < 0) return -1;
		if (set_ipsec_rsakey(name, STRING_IPSEC_L_RSAKEY, buf) < 0) return -1;
	}
	else
	{
		char token1[]=": PSK \"";
		char token2[]="\"\n";

		write(fd, token1, strlen(token1));
		write(fd, shared, strlen(shared));
		write(fd, token2, strlen(token2));
		write(fd, '\0', 1);
		close(fd);
	}
	return 0;
}

int set_ipsec_auth(char *ipsec_conn, int opt)
{
	char buf[MAX_LINE], filename[60];
	
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (opt == RSA)	sprintf(buf, "rsasig");
		else sprintf(buf, "secret");
	return replace_string_in_file_nl(filename, STRING_IPSEC_AUTHBY, buf);
}

int get_ipsec_link(char *ipsec_conn)
{
	int ret;
	struct stat st;
	char opt[5], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (stat(filename, &st) != 0)
	{
		pr_error(0, "Could not get ipsec link");
		return -1;
	}
	if ((ret=find_string_in_file_nl(filename, STRING_IPSEC_LINK, opt, 5)) < 0) return ret;
	if (strstr(opt, "yes")) return 1;
		else return 0;
}

int set_ipsec_ike_authproto(char *ipsec_conn, int opt)
{
	char buf[MAX_LINE], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (opt == AH)	sprintf(buf, "ah");
		else sprintf(buf, "esp");
	return replace_string_in_file_nl(filename, STRING_IPSEC_AUTHPROTO, buf);
}

int set_ipsec_esp(char *ipsec_conn, char *cypher, char *hash)
{
	char buf[MAX_LINE], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	*buf=0;
	if (cypher)
	{
		if (hash) sprintf(buf, "%s-%s", cypher, hash);
			else sprintf(buf, "%s", cypher);
	}
	return replace_string_in_file_nl(filename, STRING_IPSEC_ESP, buf);
}

int set_ipsec_local_id(char *ipsec_conn, char *id)
{
	char filename[60];
	
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	return replace_string_in_file_nl(filename, STRING_IPSEC_L_ID, id);
}

int set_ipsec_remote_id(char *ipsec_conn, char *id)
{
	char filename[60];
	
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	return replace_string_in_file_nl(filename, STRING_IPSEC_R_ID, id);
}

int set_ipsec_local_addr(char *ipsec_conn, char *addr)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	return replace_string_in_file_nl(filename, STRING_IPSEC_L_ADDR, addr);
}

int set_ipsec_remote_addr(char *ipsec_conn, char *addr)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	return replace_string_in_file_nl(filename, STRING_IPSEC_R_ADDR, addr);
}

int set_ipsec_nexthop_inf(int position, char *ipsec_conn, char *nexthop)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	switch(position)
	{
		case LOCAL:
			return replace_string_in_file_nl(filename, STRING_IPSEC_L_NEXTHOP, nexthop);
		case REMOTE:
			return replace_string_in_file_nl(filename, STRING_IPSEC_R_NEXTHOP, nexthop);
	}
	return -1;
}

int set_ipsec_subnet_inf(int position, char *ipsec_conn, char *addr, char *mask)
{
	int ret;
	char subnet[MAX_LINE]="", filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (strlen(addr) && strlen(mask))
	{
		if ((ret=classic_to_cidr(addr, mask, subnet)) < 0) return ret;
	}
	switch(position)
	{
		case LOCAL:
			return replace_string_in_file_nl(filename, STRING_IPSEC_L_SUBNET, subnet);
		case REMOTE:
			return replace_string_in_file_nl(filename, STRING_IPSEC_R_SUBNET, subnet);
	}
	return -1;
}

int set_ipsec_protoport(char *ipsec_conn, char *protoport)
{
	char filename[60], protoport_sp1[]="17/0", protoport_sp2[]="17/1701";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (protoport == NULL)
	{
		replace_string_in_file_nl(filename, STRING_IPSEC_L_PROTOPORT, "");
		replace_string_in_file_nl(filename, STRING_IPSEC_R_PROTOPORT, "");
	}
	else if (!strcmp(protoport, "SP1"))
	{
		replace_string_in_file_nl(filename, STRING_IPSEC_L_PROTOPORT, protoport_sp1);
		replace_string_in_file_nl(filename, STRING_IPSEC_R_PROTOPORT, protoport_sp2);
	}
	else if (!strcmp(protoport, "SP2"))
	{
		replace_string_in_file_nl(filename, STRING_IPSEC_L_PROTOPORT, protoport_sp2);
		replace_string_in_file_nl(filename, STRING_IPSEC_R_PROTOPORT, protoport_sp2);
	}
	return 0;
}

int set_ipsec_pfs(char *ipsec_conn, int on)
{
	char buf[MAX_LINE], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (on) sprintf(buf, "yes");
		else sprintf(buf, "no");
	return replace_string_in_file_nl(filename, STRING_IPSEC_PFS, buf);
}

#if 0 /* local address interface [aux|ethernet|serial] */
int get_ipsec_interface(char *iface, int max_len)
{
	char *p, *cish_dev, buf[MAX_LINE];

	if (find_string_in_file_nl(FILE_IPSEC_CONF, "ipsec0", buf, MAX_LINE) < 0) return -1;
	// retira as aspas
	if ((p=strstr(buf, "\""))) memmove(p,"\0", 1);
	cish_dev=convert_os_device(buf, 0);
	if (strlen(cish_dev) < max_len)
	{
		strncpy(iface, cish_dev, max_len);
	}
		else return 0;
	return 1;
}
#endif

int get_ipsec_autoreload(void)
{
	int ret;
	char buf[MAX_LINE];

	if ((ret=find_string_in_file_nl(FILE_IPSEC_CONF, STRING_IPSEC_AUTORELOAD, buf, MAX_LINE)) < 0) return ret;
	if (!strlen(buf)) return -1;
	return atoi(buf);
}

int get_ipsec_nat_traversal(void)
{
	int ret;
	char buf[MAX_LINE];

	if ((ret=find_string_in_file_nl(FILE_IPSEC_CONF, STRING_IPSEC_NAT, buf, MAX_LINE)) < 0) return ret;
	if (!strlen(buf)) return -1;
	if (strcmp(buf, "yes") == 0) return 1;
	return 0;
}

int get_ipsec_overridemtu(void)
{
	int ret;
	char buf[MAX_LINE];

	if ((ret=find_string_in_file_nl(FILE_IPSEC_CONF, STRING_IPSEC_OMTU, buf, MAX_LINE)) < 0) return ret;
	if (!strlen(buf)) return -1;
	return atoi(buf);
}

static int ipsec_file_filter(const struct dirent *file)
{
	char *p1, *p2;

	if ((p1=strstr(file->d_name, "ipsec.")) == NULL) return 0;
	if ((p2=strstr(file->d_name, ".conf")) == NULL) return 0;
	if (p1+6 < p2) return 1; /* ipsec.[conname].conf */
	return 0;
}

int list_all_ipsec_names(char ***rcv_p)
{
	int i, n, count=0;
	struct dirent **namelist;
	char **list, **list_ini;

	n=scandir("/etc/", &namelist, ipsec_file_filter, alphasort);
	if (n < 0)
	{
		pr_error(0, "scandir failed");
		return -1;
	}
	else
	{
		list_ini = list = malloc(sizeof(char *)*MAX_CONN);
		if (list == NULL) return -1;
		for (i=0; i < MAX_CONN; i++, list++) *list = NULL;
		list = list_ini;
		for (i=0; i < n; i++)
		{
			char *p1, *p2;
			p1 = strchr(namelist[i]->d_name, '.')+1;
			p2 = strrchr(namelist[i]->d_name, '.');
			if((count < MAX_CONN) && (p1 < p2))
			{
				*list = malloc((p2-p1)+1);
				if(*list != NULL)
				{
					*p2 = 0;
					strncpy(*list, p1, ((p2-p1)+1));
					list++;
					count++;
				}
			}
			free(namelist[i]);
		}
		free(namelist);
	}
	*rcv_p = list_ini;
	return 1;
}

int get_ipsec_id(int position, char *ipsec_conn, char *buf)
{
	int ret;
	struct stat st;
	char filename[60];
	
	*buf = '\0';
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if(stat(filename, &st)!=0)	return -1;
	if(position==LOCAL)
	{
		if((ret=find_string_in_file_nl(filename, STRING_IPSEC_L_ID, buf, MAX_LINE)) < 0) return ret;
	}
	else
	{
		if((ret=find_string_in_file_nl(filename, STRING_IPSEC_R_ID, buf, MAX_LINE)) < 0) return ret;
	}
	if (strlen(buf)>0) return 1;
		else return 0;
}

int get_ipsec_subnet(int position, char *ipsec_conn, char *buf)
{
	int ret;
	struct stat st;
	char subnet[MAX_LINE]="", filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (stat(filename, &st)!=0)	return -1;
	if (position==LOCAL)
	{
		if ((ret=find_string_in_file_nl(filename, STRING_IPSEC_L_SUBNET, subnet, MAX_LINE)) < 0) return ret;
	}
	else
	{
		if ((ret=find_string_in_file_nl(filename, STRING_IPSEC_R_SUBNET, subnet, MAX_LINE)) < 0) return ret;
	}
	if (strlen(subnet)>0)
	{
		if((ret=cidr_to_classic(subnet, buf))>=0) return 1;
			else return ret;
	}
	*buf=0;
	return 0;
}

static int check_ip(char *str)
{
	struct in_addr ip;
	return inet_aton(str, &(ip));
}

int get_ipsec_local_addr(char *ipsec_conn, char *buf)
{
	int ret;
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if ((ret=find_string_in_file_nl(filename, STRING_IPSEC_L_ADDR, buf, MAX_LINE)) < 0)
	{
		*buf=0;
		return ret;
	}
	if (check_ip(buf)) return ADDR_IP;
	else if (strcmp(buf, STRING_DEFAULTROUTE) == 0)
	{
		*buf=0;
		return ADDR_DEFAULT;
	}
		else if (strlen(buf) && buf[0] == '%') return ADDR_INTERFACE;
	*buf=0;
	return 0;
}

int get_ipsec_remote_addr(char *ipsec_conn, char *buf)
{
	int ret;
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if ((ret=find_string_in_file_nl(filename, STRING_IPSEC_R_ADDR, buf, MAX_LINE)) < 0)
	{
		*buf=0;
		return ret;
	}
	if (check_ip(buf)) return ADDR_IP;
	else
	{
		if (strcmp(buf, STRING_ANY) == 0)
		{
			*buf=0;
			return ADDR_ANY;
		}
			else if (strlen(buf)) return ADDR_FQDN;
	}
	*buf=0;
	return 0;
}

int get_ipsec_nexthop(int position, char *ipsec_conn, char *buf)
{
	int ret;
	struct stat st;
	char filename[60];
	
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if(stat(filename, &st)!=0)	return -1;
	if(position==LOCAL)
	{
		if((ret=find_string_in_file_nl(filename, STRING_IPSEC_L_NEXTHOP, buf, MAX_LINE))<0)	return ret;
	}
	else
	{
		if((ret=find_string_in_file_nl(filename, STRING_IPSEC_R_NEXTHOP, buf, MAX_LINE))<0)	return ret;
	}
	if(ret < 0)
	{
		*buf=0;
		return 0;
	}
	if(strlen(buf))	return 1;
	else		return 0;
}

int get_ipsec_ike_authproto(char *ipsec_conn)
{
	int ret;
	char *p, filename[60], buf[MAX_LINE];
	
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	ret = find_string_in_file_nl(filename, STRING_IPSEC_AUTHPROTO, buf, MAX_LINE);
	if(ret<0)	return ret;
	if((p=strstr(buf, "ah")))		return AH;
	else	if((p=strstr(buf, "esp")))	return ESP;
	return 0;
}

int get_ipsec_esp(char *ipsec_conn, char *buf)
{
	int ret;
	char *p, filename[60];
	
	*buf = '\0';
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	ret = find_string_in_file_nl(filename, STRING_IPSEC_ESP, buf, MAX_LINE);
	if (ret < 0) return ret;
	if (strlen(buf) > 0)
	{
		if((p=strchr(buf, '-'))) *p=' '; /* remove '-' */
		return 1;
	}
	return 0;
}

int get_ipsec_pfs(char *ipsec_conn)
{
	int ret;
	char *p, filename[60], buf[MAX_LINE]="";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if ((ret=find_string_in_file_nl(filename, STRING_IPSEC_PFS, buf, MAX_LINE))<0)	return ret;
	if (strlen(buf)>0)
	{
		if ((p=strstr(buf, "yes"))) return 1;
		if ((p=strstr(buf, "no"))) return 0;
	}
	return 0;
}

int set_ipsec_link(char *ipsec_conn, int on_off)
{
	struct stat st;
	char opt[20], filename[60], buf[200];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (stat(filename, &st) != 0) return -1;

	if (on_off)
	{
		if (get_ipsec_remote_addr(ipsec_conn, buf) == ADDR_ANY) // caso o endereco remoto seja "any", entao o paramentro "auto" dentro de ipsec.[conn name].conf deve ser "add"
			sprintf(opt, "add");
		else
			sprintf(opt, "start");
	}
		else sprintf(opt, "ignore");
	replace_string_in_file_nl(filename, STRING_IPSEC_AUTO, opt); /* auto= start/ignore/add */
	//if (!strcmp(opt, "add") && (get_ipsec_auth(ipsec_conn, buf) == SECRET)) sprintf(opt, "yes"); /* PSK with %any uses aggresive mode */
	//	else 
	sprintf(opt, "no");
	replace_string_in_file_nl(filename, STRING_IPSEC_AGGRMODE, opt); /* aggrmode= yes/no */
	if (on_off) sprintf(opt, "yes");
		else sprintf(opt, "no");
	return replace_string_in_file_nl(filename, STRING_IPSEC_LINK, opt);
}

int get_ipsec_sharedkey(char *ipsec_conn, char **buf)
{
	int fd;
	int i;
	char *p, tmp[60];
	struct stat st;
	
	sprintf(tmp, FILE_CONN_SECRETS, ipsec_conn);
	if((fd = open(tmp, O_RDONLY))<0)
	{
		fprintf(stderr, "could not get sharedkey");
		return -1;
	}
	if(fstat(fd, &st)<0)
	{
		fprintf(stderr, "could not read file propertys");
		return -1;
	}
	if(!(*buf = malloc(st.st_size+1)))
	{
		close(fd);
		fprintf(stderr, "could not alloc memory");
		return -1;
	}
	read(fd, *buf, st.st_size);
	close(fd);
	*(*buf+st.st_size) = '\0';
	if((p=strchr(*buf, '"')))
	{
		for(p++,i=0; *p!='"'; p++,i++)	*(*buf+i) = *p;
		*(*buf+i) = '\0';
		return 1;
	}
	else
	{
		free(*buf);
		return -1;
	}
}

int get_ipsec_rsakey(char *ipsec_conn, char *token, char **buf)
{
	int ret;
	struct stat st;
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (stat(filename, &st) != 0)
	{
		fprintf(stderr, "could not get ipsec rsakey");
		return -1;
	}
	if (!(*buf=malloc(MAX_KEY_SIZE+1)))
	{
		fprintf(stderr, "could not alloc memory");
		return -1;
	}
	if ((ret=find_string_in_file_nl(filename, token, *buf, MAX_KEY_SIZE)) < 0)
	{
		free(*buf);
		*buf = NULL;
		if (ret < 0) return ret;
			else return 0;
	}
	return 1;
}

char *get_ipsec_protoport(char *ipsec_conn)
{
	struct stat st;
	char filename[60], tmp[60];
	char protoport_sp1[]="17/0", protoport_sp2[]="17/1701";
	static char sp1[]="SP1", sp2[]="SP2";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (stat(filename, &st) != 0)
	{
		fprintf(stderr, "could not get ipsec protoport");
		return NULL;
	}
	if (find_string_in_file_nl(filename, STRING_IPSEC_L_PROTOPORT, tmp, 60) < 0)
	{
		return NULL;
	}
	if (!strcmp(tmp, protoport_sp1)) return sp1;
	if (!strcmp(tmp, protoport_sp2)) return sp2;
	return NULL;
}

int get_ipsec_auto(char *ipsec_conn)
{
	int ret;
	char filename[60], buf[MAX_LINE]="";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if ((ret=find_string_in_file_nl(filename, STRING_IPSEC_AUTO, buf, MAX_LINE))<0)	return ret;
	if (strlen(buf)>0)
	{
		if (strncmp(buf, "ignore", 6)==0) return AUTO_IGNORE;
		if (strncmp(buf, "start", 6)==0) return AUTO_START;
		if (strncmp(buf, "add", 3)==0) return AUTO_ADD;
	}
	return 0;
}

int get_mpc180(void)
{
	struct stat st;

	return (stat(PROC_MPC180, &st) == 0 ? 1 : 0);
}

void lconfig_crypto_dump(FILE *out)
{
	int idx, mtu, auto_reload;
	char **list = NULL, **list_ini = NULL;
	ppp_config cfg;
	char buf[256];

	fprintf(out, "crypto\n");
#if 0
	if (get_ipsec_interface(iface, 40) > 0)
	{
		fprintf(out, " interface %s\n", iface);
	}
#endif
	if ((auto_reload = get_ipsec_autoreload()) > 0) {
		fprintf(out, " auto-reload %d\n", auto_reload);
	}
	if (get_ipsec_nat_traversal() > 0) {
		fprintf(out, " nat-traversal\n");
	}
	if ((mtu = get_ipsec_overridemtu()) > 0) {
		fprintf(out, " overridemtu %d\n", mtu);
	}
	if (list_all_ipsec_names(&list) > 0) {
		if (*list != NULL) {
			list_ini = list;
			for (idx = 0; idx < MAX_CONN; idx++, list++) {
				if (*list) { /* process connection name */
					char *pt;

					// name
					fprintf(out, " ipsec connection add %s\n", *list);
					fprintf(out, " ipsec connection %s\n", *list);

					// authby
					switch (get_ipsec_auth(*list, buf)) {
					case RSA:
						fprintf(out, "  authby rsa\n");
						break;
					case SECRET: {
						pt = NULL;
						if (get_ipsec_sharedkey(*list, &pt) > 0) {
							if (pt) {
								fprintf(
								                out,
								                "  authby secret %s\n",
								                pt);
								free(pt);
							}
						}
						break;
					}
					default:
						break;
					}

					// authproto and crypto
					switch (get_ipsec_ike_authproto(*list)) {
					case AH:
						fprintf(out, "  authproto transport\n");
						break;
					case ESP:
						fprintf(out, "  authproto tunnel\n");
						if (get_ipsec_esp(*list, buf) > 0) {
							fprintf(out, "  esp %s\n", buf);
						}
						break;
					}

					// leftid
					buf[0] = '\0';
					if (get_ipsec_id(LOCAL, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  local id %s\n", buf);
					}

					// left address
					buf[0] = '\0';
					switch (get_ipsec_local_addr(*list, buf)) {
					case ADDR_DEFAULT:
						fprintf(out, "  local address default-route\n");
						break;
					case ADDR_INTERFACE:
						if (strlen(buf) > 0)
							fprintf(
							                out,
							                "  local address interface %s\n",
							                convert_os_device(buf + 1,
							                                0)); /* skip % */
						break;
					case ADDR_IP:
						if (strlen(buf) > 0)
							fprintf(out, "  local address ip %s\n", buf);
						break;
#if 0
						case ADDR_FQDN:
						if (strlen(buf) > 0)
						fprintf(out, "  local address fqdn %s\n", buf);
						break;
#endif
					}

					// leftsubnet
					buf[0] = '\0';
					if (get_ipsec_subnet(LOCAL, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  local subnet %s\n", buf);
					}

					// leftnexthop
					buf[0] = '\0';
					if (get_ipsec_nexthop(LOCAL, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  local nexthop %s\n", buf);
					}

					// rightid
					buf[0] = '\0';
					if (get_ipsec_id(REMOTE, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  remote id %s\n", buf);
					}

					// right address
					buf[0] = '\0';
					switch (get_ipsec_remote_addr(*list, buf)) {
					case ADDR_ANY:
						fprintf(out, "  remote address any\n");
						break;
					case ADDR_IP:
						if (strlen(buf) > 0)
							fprintf(out, "  remote address ip %s\n",
							                buf);
						break;
					case ADDR_FQDN:
						if (strlen(buf) > 0)
							fprintf(out, "  remote address fqdn %s\n",
							                buf);
						break;
					}

					// rightsubnet
					buf[0] = '\0';
					if (get_ipsec_subnet(REMOTE, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  remote subnet %s\n", buf);
					}

					// rightnexthop
					buf[0] = '\0';
					if (get_ipsec_nexthop(REMOTE, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  remote nexthop %s\n", buf);
					}

					// rightrsasigkey
					pt = NULL;
					if (get_ipsec_rsakey(*list, STRING_IPSEC_R_RSAKEY, &pt) > 0) {
						if (pt) {
							fprintf(out, "  remote rsakey %s\n", pt);
							free(pt);
						}
					}

					// pfs
					switch (get_ipsec_pfs(*list)) {
					case 0:
						fprintf(out, "  no pfs\n");
						break;
					case 1:
						fprintf(out, "  pfs\n");
						break;
					}
					// l2tp ppp
					{
						char netmask[16];

						l2tp_ppp_get_config(*list, &cfg);
						if (cfg.peer[0]) {
							if (cidr_to_netmask(cfg.peer_mask, netmask)
							                != -1)
								fprintf(out, "  l2tp peer %s %s\n",
								                cfg.peer, netmask);
						}
						if (cfg.auth_user[0])
							fprintf(
							                out,
							                "  l2tp ppp authentication user %s\n",
							                cfg.auth_user);
						if (cfg.auth_pass[0])
							fprintf(
							                out,
							                "  l2tp ppp authentication pass %s\n",
							                cfg.auth_pass);
						if (cfg.ip_unnumbered != -1) { /* exibir ip unnumbered no show running config */
							fprintf(
							                out,
							                "  l2tp ppp ip unnumbered ethernet %d\n",
							                cfg.ip_unnumbered);
						} else {
							if (cfg.ip_addr[0])
								fprintf(
								                out,
								                "  l2tp ppp ip address %s\n",
								                cfg.ip_addr);
							else
								fprintf(out,
								                "  no l2tp ppp ip address\n");
						}
						if (cfg.ip_peer_addr[0])
							fprintf(
							                out,
							                "  l2tp ppp ip peer-address %s\n",
							                cfg.ip_peer_addr);
						else
							fprintf(out,
							                "  l2tp ppp ip peer-address pool\n");
						if (cfg.default_route)
							fprintf(out,
							                "  l2tp ppp ip default-route\n");
						if (cfg.novj)
							fprintf(out, "  no l2tp ppp ip vj\n");
						else
							fprintf(out, "  l2tp ppp ip vj\n");

						if (cfg.echo_interval)
							fprintf(
							                out,
							                "  l2tp ppp keepalive interval %d\n",
							                cfg.echo_interval);
						if (cfg.echo_failure)
							fprintf(
							                out,
							                "  l2tp ppp keepalive timeout %d\n",
							                cfg.echo_failure);
						if (cfg.mtu)
							fprintf(out, "  l2tp ppp mtu %d\n", cfg.mtu);
					}
					// l2tp protoport SP1|SP2
					if ((pt = get_ipsec_protoport(*list)) != NULL) {
						fprintf(out, "  l2tp protoport %s\n", pt);
					}

					// auto
					switch (get_ipsec_auto(*list)) {
					case AUTO_IGNORE:
						fprintf(out, "  shutdown\n");
						break;
					case AUTO_START:
					case AUTO_ADD:
						fprintf(out, "  no shutdown\n");
						break;
					}
					free(*list);
				}
			}
			free(list_ini);
		}
	}
	/* l2tp settings! */
	if (get_dhcp_local() == DHCP_SERVER) {
		if (get_dhcp_server_local(buf) == 0) {
			fprintf(out, " %s\n", buf);
		}
	} else
		fprintf(out, " l2tp pool ethernet 0\n");

	if (get_l2tp() > 0) {
		fprintf(out, " l2tp server\n");
	}
	fprintf(out, "!\n");
}

#endif /* OPTION_IPSEC */

