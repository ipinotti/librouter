#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "options.h"
#include "args.h"
#include "defines.h"
#include "error.h"
#include "exec.h"
#include "ntp.h"
#include "nv.h"
#include "ppp.h"

#ifdef OPTION_NTPD
void ntp_hup(void)
{
	FILE *f;
	char buf[32];
	pid_t pid;

	if (!(f=fopen(NTP_PID, "r"))) return;
	fgets(buf, 31, f);
	fclose(f);
	pid=(pid_t)atoi(buf);
	if (pid > 1) kill(pid, SIGHUP);
}

#ifdef OPTION_NTPD_authenticate
/*
 *  Returns 1 if NTP authentication is used and 0 if not
 */
int is_ntp_auth_used(void)
{
	FILE *f;
	arglist *args;
	char *p, line[200];
	int go_out, used=0;

	if(!(f = fopen(FILE_NTP_CONF, "r"))) return used;
	for(go_out=0; !go_out && fgets(line, 200, f); )
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		args = libconfig_make_args(line);
		if(args->argc > 1)
		{
			if(!strcmp(args->argv[0], "#authenticate"))
			{
				if(!strcmp(args->argv[1], "yes"))	used++;
				go_out++;
			}
		}
		libconfig_destroy_args(args);
	}
	fclose(f);
	return used;
}

int do_ntp_authenticate(int used)
{
	int fd;
	FILE *f;
	arglist *args;
	struct stat st;
	char *p, *local, line[200];

	if(used == is_ntp_auth_used())	return 0;
	if((fd = open(FILE_NTP_CONF, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128)))
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0]='\0';
	close(fd);

	if(!(f = fopen(FILE_NTP_CONF, "r")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		free(local);
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args = libconfig_make_args(line);
			if(args->argc >= 4)
			{
				if(!strcmp(args->argv[0], "server"))
				{
					if(strstr(args->argv[3], "key"))
					{
						strcat(local, "server ");
						strcat(local, args->argv[1]);
						strcat(local, " iburst ");
						if(used)	strcat(local, "key ");
						else		strcat(local, "#key ");
						if(args->argc >= 5)	strcat(local, args->argv[4]);
						else			strcat(local, "1");
						strcat(local, "\n");
					}
				}
				else
				{
					strcat(local, line);
					strcat(local, "\n");
				}
			}
			else
			{
				if(args->argc > 1)
				{
					if(!strcmp(args->argv[0], "#authenticate"))
					{
						if(used)	strcat(local, "#authenticate yes\n");
						else		strcat(local, "#authenticate no\n");
					}
					else
					{
						strcat(local, line);
						strcat(local, "\n");
					}
				}
				else
				{
					strcat(local, line);
					strcat(local, "\n");
				}
			}
			libconfig_destroy_args(args);
		}
	}
	fclose(f);
	if(!(f = fopen(FILE_NTP_CONF, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	fclose(f);
	free(local);
	ntp_hup();
	return 0;
}
#endif

int do_ntp_restrict(char *server, char *mask)
{
	FILE *f;
	arglist *args;
	struct stat st;
	int fd, found=0;
	char *p, *local, line[200];

	if((fd = open(FILE_NTP_CONF, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128)))
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0]='\0';
	close(fd);

	if(!(f=fopen(FILE_NTP_CONF, "r+")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args = libconfig_make_args(line);
			if(args->argc >= 4) /* restrict <ipaddr> mask <netmask> nomodify noserve (noquery) */
			{
				if(!found && !strcmp(args->argv[0], "restrict") && !strcmp(args->argv[1], server))
				{
					found=1;
					strcat(local, "restrict ");
					strcat(local, server);
					strcat(local, " mask ");
					strcat(local, mask);
					strcat(local, " nomodify noserve"); /* disable server */
				}
					else strcat(local, line);
				strcat(local, "\n");
			}
			else
			{
				strcat(local, line);
				strcat(local, "\n");
			}			
			libconfig_destroy_args(args);
		}
	}
	fclose(f);
	if(!(f = fopen(FILE_NTP_CONF, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	free(local);
	if (!found)
	{
		sprintf(line, "restrict %s mask %s nomodify noserve\n", server, mask);
		fwrite(line, 1, strlen(line), f);
	}
	fclose(f);
	ntp_hup();
	return 0;
}

int do_ntp_server(char *server, char *key_num)
{
	FILE *f;
	arglist *args;
	struct stat st;
	int fd, found=0;
	char *p, *local, line[200];
#ifdef OPTION_NTPD_authenticate
	int auth=is_ntp_auth_used();
#endif

	if((fd=open(FILE_NTP_CONF, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128)))
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0] = '\0';
	close(fd);

	if(!(f = fopen(FILE_NTP_CONF, "r+")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args=libconfig_make_args(line);
			if(!found && args->argc >= 2 && !strcmp(args->argv[0], "server") && !strcmp(args->argv[1], server))
			{	/* server <ipaddr> iburst key 1-16 */
				found=1;
				strcat(local, "server ");
				strcat(local, server);
				strcat(local, " iburst");
				if (key_num)
				{
					strcat(local, " key ");
					strcat(local, key_num);
				}
			}
				else strcat(local, line);
			strcat(local, "\n");
			libconfig_destroy_args(args);
		}
	}
	fclose(f);
	if(!(f = fopen(FILE_NTP_CONF, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	free(local);
	if (!found)
	{
#ifdef OPTION_NTPD_authenticate	
		if (auth) sprintf(line, "server %s iburst key ", server);
			else sprintf(line, "server %s iburst #key ", server);
		if (key_num) strcat(line, key_num);
			else strcat(line, "1");
		strcat(line, "\n");
#else
		if (key_num) sprintf(line, "server %s iburst key %s\n", server, key_num);
			else sprintf(line, "server %s iburst\n", server);
#endif
		fwrite(line, 1, strlen(line), f);
	}
	fclose(f);
	if (!is_daemon_running(NTP_DAEMON)) exec_daemon(NTP_DAEMON);
		else ntp_hup();
	return 0;
}

int do_ntp_trust_on_key(char *num)
{
	FILE *f;
	arglist *args;
	struct stat st;
	int i, fd, found=0;
	char *p, *local, line[200];

	if((fd = open(FILE_NTP_CONF, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128)))
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0]='\0';
	close(fd);

	if(!(f = fopen(FILE_NTP_CONF, "r")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		free(local);
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args = libconfig_make_args(line);
			if(args->argc > 0)
			{
				if(!found && !strcmp(args->argv[0], "trustedkey"))
				{
					for(i=1; i < args->argc; i++)	if(!strcmp(args->argv[i], num))	found++;
					strcat(local, "trustedkey");
					for(i=1; i < args->argc; i++)
					{
						strcat(local, " ");
						strcat(local, args->argv[i]);
					}
					if(!found)
					{
						found=1;
						strcat(local, " ");
						strcat(local, num);
					}
				}
				else	strcat(local, line);
				strcat(local, "\n");
			}
			else
			{
				strcat(local, line);
				strcat(local, "\n");
			}
			libconfig_destroy_args(args);
		}
	}
	if (!found)
	{
		strcat(local, "trustedkey");
		strcat(local, " ");
		strcat(local, num);
		strcat(local, "\n");
	}
	fclose(f);
	if(!(f = fopen(FILE_NTP_CONF, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	fclose(f);
	free(local);
	ntp_hup();
	return 0;
}

int do_exclude_ntp_restrict(char *addr)
{
	int fd;
	FILE *f;
	arglist *args;
	struct stat st;
	char *p, *local, line[200];

	if((fd = open(FILE_NTP_CONF, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128)))
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0] = '\0';
	close(fd);

	if(!(f = fopen(FILE_NTP_CONF, "r")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		free(local);
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args = libconfig_make_args(line);
			if(args->argc >= 4) /* restrict <ipaddr> mask <netmask> nomodify noserve (noquery) */
			{
				if(!strcmp(args->argv[0], "restrict"))
				{
					if(addr)
					{
						if(strcmp(args->argv[1], addr))
						{
							strcat(local, line);
							strcat(local, "\n");
						}
						/* else delete line! */
					}
					/* else delete line! */
				}
				else
				{
					strcat(local, line);
					strcat(local, "\n");
				}
			}
			else
			{
				strcat(local, line);
				strcat(local, "\n");
			}
			libconfig_destroy_args(args);
		}
	}
	fclose(f);
	if(!(f=fopen(FILE_NTP_CONF, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	fclose(f);
	free(local);
	ntp_hup();
	return 0;
}

int do_exclude_ntp_server(char *addr)
{
	int fd, servers=0;
	FILE *f;
	arglist *args;
	struct stat st;
	char *p, *local, line[200];

	if((fd = open(FILE_NTP_CONF, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128)))
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0] = '\0';
	close(fd);

	if(!(f = fopen(FILE_NTP_CONF, "r")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		free(local);
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args = libconfig_make_args(line);
			if(args->argc >= 3) /* server <ipaddress> iburst [key <1-16>] */
			{
				if(!strcmp(args->argv[0], "server"))
				{
					if(addr)
					{
						if(strcmp(args->argv[1], addr))
						{
							strcat(local, line);
							strcat(local, "\n");
							servers++;
						}
						/* else delete line! */
					}
					/* else delete line! */
				}
				else
				{
					strcat(local, line);
					strcat(local, "\n");
				}
			}
			else
			{
				strcat(local, line);
				strcat(local, "\n");
			}
			libconfig_destroy_args(args);
		}
	}
	fclose(f);
	if(!(f = fopen(FILE_NTP_CONF, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	fclose(f);
	free(local);
	if (!servers && is_daemon_running(NTP_DAEMON)) kill_daemon(NTP_DAEMON);
		else ntp_hup();
	return 0;
}

int do_exclude_ntp_trustedkeys(char *num)
{
	FILE *f;
	int i, fd;
	arglist *args;
	struct stat st;
	char *p, *local, line[200];

	if((fd = open(FILE_NTP_CONF, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128)))
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0] = '\0';
	close(fd);

	if(!(f = fopen(FILE_NTP_CONF, "r")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		free(local);
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args = libconfig_make_args(line);
			if(args->argc > 1)
			{
				if(!strcmp(args->argv[0], "trustedkey"))
				{
					if(num)
					{
						strcat(local, args->argv[0]);
						for(i=1; i < args->argc; i++)
						{
							if(strcmp(args->argv[i], num))
							{
								strcat(local, " ");
								strcat(local, args->argv[i]);
							}
						}
						strcat(local, "\n");
					}
				}
				else
				{
					strcat(local, line);
					strcat(local, "\n");
				}
			}
			else
			{
				strcat(local, line);
				strcat(local, "\n");
			}
			libconfig_destroy_args(args);
		}
	}
	fclose(f);
	if(!(f = fopen(FILE_NTP_CONF, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	fclose(f);
	free(local);
	ntp_hup();
	return 0;
}

int do_ntp_key_set(char *key_num, char *value)
{
	int fd, found=0;
	FILE *f;
	arglist *args;
	struct stat st;
	char *p, *local, line[200];

	if((fd=open(FILE_NTPD_KEYS, O_RDONLY)) < 0)
	{
		libconfig_pr_error(0, "Could not read NTP configuration (open)");
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		close(fd);
		libconfig_pr_error(0, "Could not read NTP configuration (fstat)");
		return -1;
	}
	if(!(local=malloc(st.st_size+128+strlen(value))))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (malloc)");
		return -1;
	}
	local[0]='\0';
	close(fd);

	if(!(f=fopen(FILE_NTPD_KEYS, "r")))
	{
		libconfig_pr_error(0, "Could not read NTP configuration (fopen)");
		free(local);
		return -1;
	}
	while(fgets(line, 200, f))
	{
		if((p = strchr(line, '\n')))	*p = '\0';
		if(strlen(line))
		{
			args = libconfig_make_args(line);
			if(args->argc > 2)
			{
				if(!found && !strcmp(args->argv[0], key_num) && !strcmp(args->argv[1], "MD5"))
				{
					found=1;
					strcat(local, key_num);
					strcat(local, " MD5 ");
					strcat(local, value);
					strcat(local, "   # MD5 key\n");
				}
				else
				{
					strcat(local, line);
					strcat(local, "\n");
				}
			}
			else
			{
				strcat(local, line);
				strcat(local, "\n");
			}
			libconfig_destroy_args(args);
		}
	}
	if (!found)
	{
		strcat(local, key_num);
		strcat(local, " MD5 ");
		strcat(local, value);
		strcat(local, "   # MD5 key\n");
	}
	fclose(f);
	if(!(f = fopen(FILE_NTPD_KEYS, "w")))
	{
		libconfig_pr_error(0, "Could not write NTP configuration (fopen)");
		free(local);
		return -1;
	}
	fwrite(local, 1, strlen(local), f);
	fclose(f);
	free(local);
	ntp_hup();
	save_ntp_secret(NTP_KEY_FILE); /* save keys on flash! */
	return 0;
}

#else

int ntp_set(int timeout, char *ip)
{
	FILE *f;

	if (ip)
	{
		f=fopen(NTP_CFG_FILE, "w");
		if (!f) return -1;
		fprintf(f, "%d %s\n", timeout, ip);
		fclose(f);
	}
		else unlink(NTP_CFG_FILE);
	return notify_systtyd();
}

int ntp_get(int *timeout, char *ip)
{
	arglist *args;
	char buf[32];
	FILE *f;

	f=fopen(NTP_CFG_FILE, "r");
	if (!f) return -1;
	fgets(buf, 32, f);
	buf[31]=0;
	fclose(f);
	args=libconfig_make_args(buf);
	if (args->argc < 2)
	{
		libconfig_destroy_args(args);
		return -1;
	}
	*timeout=atoi(args->argv[0]);
	strncpy(ip, args->argv[1], 16); ip[15]=0;
	libconfig_destroy_args(args);
	return 0;
}
#endif

void lconfig_ntp_dump(FILE *out)
{
	int i, printed_something = 0;
	FILE *f;
	arglist *args;
	char *p, line[200];

#ifdef OPTION_NTPD_authenticate
	if (is_ntp_auth_used()) fprintf(out, "ntp authenticate\n");
	else fprintf(out, "no ntp authenticate\n");
#endif

	if ((f = fopen(FILE_NTP_CONF, "r"))) {
		while (fgets(line, 200, f)) {
			if ((p = strchr(line, '\n')))
				*p = '\0';
			if (strlen(line)) {
				args = libconfig_make_args(line);
				if (!strcmp(args->argv[0], "restrict")) /* restrict <ipaddr> mask <mask> */
				{
					if (args->argc >= 4) {
						printed_something = 1;
						fprintf(
						                out,
						                "ntp restrict %s %s\n",
						                args->argv[1],
						                args->argv[3]);
					}
				}
				libconfig_destroy_args(args);
			}
		}
		fseek(f, 0, SEEK_SET);
		while (fgets(line, 200, f)) {
			if ((p = strchr(line, '\n')))
				*p = '\0';
			if (strlen(line)) {
				args = libconfig_make_args(line);
				if (!strcmp(args->argv[0], "trustedkey")) {
					printed_something = 1;
					if (args->argc > 1) {
						for (i = 1; i < args->argc; i++)
							fprintf(
							                out,
							                "ntp trusted-key %s\n",
							                args->argv[i]);
					} else
						fprintf(out,
						                "no ntp trusted-key\n");
				}
				libconfig_destroy_args(args);
			}
		}

		fseek(f, 0, SEEK_SET);
		while (fgets(line, 200, f)) {
			if ((p = strchr(line, '\n')))
				*p = '\0';
			if (strlen(line)) {
				args = libconfig_make_args(line);
				if (args->argc >= 2 && !strcmp(args->argv[0],
				                "server")) /* server <ipaddr> iburst [key 1-16] */
				{
					printed_something = 1;
					fprintf(out, "ntp server %s",
					                args->argv[1]);
					if (args->argc >= 5 && !strcmp(
					                args->argv[3], "key"))
						fprintf(out, " key %s\n",
						                args->argv[4]);
					else
						fprintf(out, "\n");
				}
				libconfig_destroy_args(args);
			}
		}
		fclose(f);
		if (printed_something)
			fprintf(out, "!\n");
	}
}
