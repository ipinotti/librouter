#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <syslog.h>
#include <getopt.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libconfig/getpid.h>

/* List of processes. */
static PROC plist[MAX_NUM_PROCESSES];
static int num_processes;

/*
 * Esta funcao zera 'plist_uptodate', de forma a sinalizar que a tabela
 * 'plist' nao eh mais valida. Ela deve ser chamada sempre que houve
 * a criacao/destruicao de algum processo.
 */
void invalidate_plist(void)
{
//	plist_uptodate = 0; // ver comentario no final do arquivo
}

/*
 * Esta funcao varre o diretorio /proc e preenche a variavel global
 * 'plist' com informacoes sobre os processos que estao rodando e 
 * 'num_processes' com o numero de processos.
 */
int read_proc(void)
{
	DIR *dir;
	struct dirent *d;
	FILE *fp;
	int pid, f;
	char *s, *q;
	char path[256];
	char buf[256];
	PROC *p;
	int c;
	int n_proc;
	
	p = plist;
	n_proc = 0;
	
	/* Open the /proc directory. */
	if ((dir = opendir("/proc")) == NULL) 
	{
		return -1;
	}
	/* Walk through the directory. */
	while ((d = readdir(dir)) != NULL) 
	{
		/* See if this is a process */
		if ((pid = atoi(d->d_name)) == 0) continue;
		p->pid = pid;
		
		/* Open the statistics file. */
		sprintf(path, "/proc/%s/stat", d->d_name);
		
		/* Read SID & statname from it. */
		if ((fp = fopen(path, "r")) != NULL) 
		{
			buf[0] = 0;
			fgets(buf, 256, fp);
			
			/* See if name starts with '(' */
			s = buf;
			while(*s!=' ') s++;
			s++;
			if (*s == '(') 
			{
				/* Read program name. */
				q = strrchr(buf, ')');
				if (q == NULL) 
				{
					//printf("can't get program name from %s\n", path);
					continue;
				}
				*q = 0;
				s++;
			} 
			fclose(fp);
		} 
		else 
		{
			/* Process disappeared.. */
			continue;
		}
		strncpy(p->basename, s, MAX_PROC_BASENAME);
		p->basename[MAX_PROC_BASENAME-1] = 0;   
		
		/* Now read arguments */
		sprintf(path, "/proc/%s/cmdline", d->d_name);
		if ((fp = fopen(path, "r")) != NULL) 
		{
			f = 0;
			while (f < (MAX_PROC_CMDLINE-1) && (c = fgetc(fp)) != EOF ) 
			{
				p->cmdline[f++] = c ? c : ' ';
			}
			p->cmdline[f++] = 0;
			fclose(fp);
		} 
		else 
		{
			/* Process disappeared.. */
			continue;
		}
		p++;
		n_proc++;
		if (n_proc==MAX_NUM_PROCESSES) break;
	}
	closedir(dir);
	num_processes = n_proc;
	//plist_uptodate = 1; // ver comentario no final do arquivo
	/* Done. */
	return 1;
}

/*
 *  Esta funcao retorna o PID de um processo.
 */
int get_pid(char *name)
{
	int i, ret;
	int pid=0;
	
	//if (!plist_uptodate) // ver comentario no final do arquivo
	if ((ret=read_proc()) < 1) return ret;
	for (i=0; i < num_processes; i++)
	{
		if(strcmp(name, plist[i].basename)==0)
		{
			pid = plist[i].pid;
			break;
		}
	}
	return pid;
}

