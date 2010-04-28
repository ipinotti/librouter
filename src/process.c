#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>
#include <getopt.h>
#include <stdarg.h>
#include <errno.h>
#include <utmp.h>
#include "process.h"
#include "error.h"

/* Retorna informacoes sobre um processo.
 * progname 	- nome do processo
 * arg      	- se diferente de NULL restringe a busca a processos que 
 *		  contenham a string 'arg' na sua linha de comando
 * proc_cmdline	- buffer onde sera armazenado a linha de comando completa 
 *		  do processo encontrado. Pode ser NULL, mas caso exista
 *		  deve ter pelo menos MAX_PROC_CMDLINE bytes.
 * Retorna o PID do processo, ou 0 caso nao o encontre.
 */
pid_t get_process_info(char *progname, char *arg, char *proc_cmdline)
{
	DIR *dir;
	struct dirent *d;
	FILE *fp;
	int pid=0, found=0, f, c;
	char *s, *q;
	char path[256];
	char buf[256];
	char cmdline[MAX_PROC_CMDLINE];

	/* Open the /proc directory. */
	if ((dir = opendir("/proc")) == NULL)
	{
		pr_error(1, "cannot opendir(/proc)");
		return (0);
	}

	/* Walk through the directory. */
	while (((d = readdir(dir)) != NULL)&&(!found))
	{
		/* See if this is a process */
		if ((pid = atoi(d->d_name)) == 0) continue;

		/* Open the statistics file. */
		sprintf(path, "/proc/%s/stat", d->d_name);

		/* Read SID & statname from it. */
		if ((fp = fopen(path, "r")) != NULL)
		{
			buf[0] = 0;
			fgets(buf, 256, fp);
			fclose(fp);

			/* See if name starts with '(' */
			s = buf;
			while(*s!=' ') s++;
			s++;
			if (*s == '(')
			{
				/* Read program name. */
				q = strrchr(buf, ')');
				if (q == NULL)
					continue;
				*q = 0;
				s++;
				
				if (strcmp(s, progname)==0)
				{
					/* Now read arguments */
					sprintf(path, "/proc/%s/cmdline", d->d_name);
					if ((fp = fopen(path, "r")) != NULL) 
					{
						f = 0;
						while (f < (MAX_PROC_CMDLINE-1) && (c = fgetc(fp)) != EOF ) 
						{
							cmdline[f++] = c ? c : ' ';
						}
						cmdline[f++] = 0;
						fclose(fp);
					} 
					else 
					{
						/* Process disappeared.. */
						continue;
					}
				
					if ((arg==NULL)||(strstr(cmdline, arg)==0))
					{
						found = 1;
						if (proc_cmdline)
						{
							strncpy(proc_cmdline, cmdline, MAX_PROC_CMDLINE-1);
							proc_cmdline[MAX_PROC_CMDLINE-1]=0;
						}
					}
				}	
			}
		}
		else
		{
			/* Process disappeared.. */
			continue;
		}

	}
	closedir(dir);

	return (found ? pid :0);
}

/* Retorna 0 se o processo 'pid' nao existe, diferente de zero caso exista */
int pid_exists(pid_t pid)
{
	return ( !((kill(pid, 0) < 0) && (errno == ESRCH)) );
}

/* Espera que o processo 'pid' finalize por ate 'timeout' segundos.
   Retorna 0 em caso de timeout, o numero de segundos restantes caso contrario.
 */
int wait_for_process(pid_t pid, int timeout)
{
	while ((timeout>0)&&(pid_exists(pid)))
	{
		sleep(1);
		timeout--;
	}
	return (timeout);
}

int get_runlevel(void)
{
	struct utmp *ut;
	//char prev;

	setutent();
	while ((ut=getutent()) != NULL)
	{
		if (ut->ut_type == RUN_LVL)
		{
			//prev=ut->ut_pid / 256;
			//if (prev == 0) prev='N';
			endutent();
			return (ut->ut_pid % 256);
		}
	}
	endutent();
	return(-1);
}

