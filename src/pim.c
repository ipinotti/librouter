#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cish/options.h>
#include <libconfig/args.h>
#include <libconfig/defines.h>
#include <libconfig/device.h>
#include <libconfig/error.h>
#include <libconfig/exec.h>
#include <libconfig/pim.h>
#include <libconfig/str.h>

#ifdef OPTION_PIMD
static void pimdd_hup(void)
{
	FILE *f;
	pid_t pid;
	char buf[32];

	if (!(f=fopen(PIMD_PID, "r"))) return;
	fgets(buf, 31, f);
	fclose(f);
	pid=(pid_t)atoi(buf);
	if (pid > 1) kill(pid, SIGHUP);
}

static void pimsd_hup(void)
{
	FILE *f;
	pid_t pid;
	char buf[32];

	if (!(f=fopen(PIMS_PID, "r"))) return;
	fgets(buf, 31, f);
	fclose(f);
	pid=(pid_t)atoi(buf);
	if (pid > 1) kill(pid, SIGHUP);
}

int pimdd_phyint(int add, char *dev)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, intf=0, found=0, lines=0;
	char line[200];

	if ((f=fopen(PIMD_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f) && lines < MAX_LINES)
		{
			striplf(line);
			if (strlen(line)) args[lines++]=make_args(line);
		}
		fclose(f);
	}
	if ((f=fopen(PIMD_CFG_FILE, "w")) != NULL)
	{
		for (i=0; i < lines; i++)
		{
			if (!strcmp(args[i]->argv[0], "phyint"))
			{
				if (!strcmp(args[i]->argv[1], dev))
				{
					found=1;
					if (!add)
					{
						destroy_args(args[i]);
						continue; /* skip line (delete) */
					}
				}
				intf++;
			}
			else
			{
				if (add && !found) /* add after phyint declarations */
				{
					found=1;
					fprintf(f, "phyint %s enable preference 101 metric 1024\n", dev);
					intf++;
				}
			}
			for (j=0; j < args[i]->argc; j++)
			{
				if (j < args[i]->argc-1) fprintf(f, "%s ", args[i]->argv[j]);
					else fprintf(f, "%s\n", args[i]->argv[j]);
			}
			destroy_args(args[i]);
		}
		fclose(f);
	}
	pimdd_hup();
	return intf;
}

int pimsd_phyint(int add, char *dev)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, intf=0, found=0, lines=0;
	char line[200];

	if ((f=fopen(PIMS_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f) && lines < MAX_LINES)
		{
			striplf(line);
			if (strlen(line)) args[lines++]=make_args(line);
		}
		fclose(f);
	}
	if ((f=fopen(PIMS_CFG_FILE, "w")) != NULL)
	{
		for (i=0; i < lines; i++)
		{
			if (!strcmp(args[i]->argv[0], "phyint"))
			{
				if (!strcmp(args[i]->argv[1], dev))
				{
					found=1;
					if (!add)
					{
						destroy_args(args[i]);
						continue; /* skip line (delete) */
					}
				}
				intf++;
			}
			else
			{
				if (add && !found) /* add after phyint declarations */
				{
					found=1;
					fprintf(f, "phyint %s enable preference 101 metric 1024\n", dev);
					intf++;
				}
			}
			for (j=0; j < args[i]->argc; j++)
			{
				if (j < args[i]->argc-1) fprintf(f, "%s ", args[i]->argv[j]);
					else fprintf(f, "%s\n", args[i]->argv[j]);
			}
			destroy_args(args[i]);
		}
		fclose(f);
	}
	pimsd_hup();
	return intf;
}

void pimsd_bsr_candidate(int add, char *dev, char *major, char *priority)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, found=0, lines=0;
	char line[200];

	if ((f=fopen(PIMS_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f) && lines < MAX_LINES)
		{
			striplf(line);
			if (strlen(line))
			{
				args[lines]=make_args(line);
				if (!strcmp(args[lines]->argv[0], "cand_bootstrap_router")) found=1;
				lines++;
			}
		}
		fclose(f);
	}
	if ((f=fopen(PIMS_CFG_FILE, "w")) != NULL)
	{
		for (i=0; i < lines; i++)
		{
			if (strcmp(args[i]->argv[0], "phyint"))
			{
				if (add && !found)
				{
					found=1;
					if (priority == NULL) fprintf(f, "cand_bootstrap_router %s%s\n", dev, major);
						else fprintf(f, "cand_bootstrap_router %s%s priority %s\n", dev, major, priority);
				}
				if (!strcmp(args[i]->argv[0], "cand_bootstrap_router"))
				{
					if (add)
					{
						if (priority == NULL) fprintf(f, "cand_bootstrap_router %s%s\n", dev, major);
							else fprintf(f, "cand_bootstrap_router %s%s priority %s\n", dev, major, priority);
					}
					found=1;
					destroy_args(args[i]);
					continue; /* skip line (delete) */
				}
			}
			for (j=0; j < args[i]->argc; j++)
			{
				if (j < args[i]->argc-1) fprintf(f, "%s ", args[i]->argv[j]);
					else fprintf(f, "%s\n", args[i]->argv[j]);
			}
			destroy_args(args[i]);
		}
		fclose(f);
	}
	pimsd_hup();
}

void pimsd_rp_address(int add, char *rp)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, found=0, lines=0;
	char line[200];

	if ((f=fopen(PIMS_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f) && lines < MAX_LINES)
		{
			striplf(line);
			if (strlen(line))
			{
				args[lines]=make_args(line);
				if (!strcmp(args[lines]->argv[0], "rp_address")) found=1;
				lines++;
			}
		}
		fclose(f);
	}
	if ((f=fopen(PIMS_CFG_FILE, "w")) != NULL)
	{
		for (i=0; i < lines; i++)
		{
			if (strcmp(args[i]->argv[0], "phyint"))
			{
				if (add && !found)
				{
					found=1;
					fprintf(f, "rp_address %s\n", rp);
				}
				if (!strcmp(args[i]->argv[0], "rp_address"))
				{
					if (add)
					{
						fprintf(f, "rp_address %s\n", rp);
					}
					found=1;
					destroy_args(args[i]);
					continue; /* skip line (delete) */
				}
			}
			for (j=0; j < args[i]->argc; j++)
			{
				if (j < args[i]->argc-1) fprintf(f, "%s ", args[i]->argv[j]);
					else fprintf(f, "%s\n", args[i]->argv[j]);
			}
			destroy_args(args[i]);
		}
		fclose(f);
	}
	pimsd_hup();
}

void pimsd_rp_candidate(int add, char *dev, char *major, char *priority, char *interval)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, found=0, lines=0;
	char line[200];

	if ((f=fopen(PIMS_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f) && lines < MAX_LINES)
		{
			striplf(line);
			if (strlen(line))
			{
				args[lines]=make_args(line);
				if (!strcmp(args[lines]->argv[0], "cand_rp")) found=1;
				lines++;
			}
		}
		fclose(f);
	}
	if ((f=fopen(PIMS_CFG_FILE, "w")) != NULL)
	{
		for (i=0; i < lines; i++)
		{
			if (strcmp(args[i]->argv[0], "phyint"))
			{
				if (add && !found)
				{
					found=1;
					if (priority == NULL) fprintf(f, "cand_rp %s%s\n", dev, major);
						else if (interval == NULL) fprintf(f, "cand_rp %s%s priority %s\n", dev, major, priority);
							else fprintf(f, "cand_rp %s%s priority %s time %s\n", dev, major, priority, interval);
				}
				if (!strcmp(args[i]->argv[0], "cand_rp"))
				{
					if (add)
					{
						if (priority == NULL) fprintf(f, "cand_rp %s%s\n", dev, major);
							else if (interval == NULL) fprintf(f, "cand_rp %s%s priority %s\n", dev, major, priority);
								else fprintf(f, "cand_rp %s%s priority %s time %s\n", dev, major, priority, interval);
					}
					found=1;
					destroy_args(args[i]);
					continue; /* skip line (delete) */
				}
			}
			for (j=0; j < args[i]->argc; j++)
			{
				if (j < args[i]->argc-1) fprintf(f, "%s ", args[i]->argv[j]);
					else fprintf(f, "%s\n", args[i]->argv[j]);
			}
			destroy_args(args[i]);
		}
		fclose(f);
	}
	pimsd_hup();
}

void dump_pim_interface(FILE *out, char *ifname)
{
	FILE *f;
	arglist *args;
	int found=0;
	char line[200];

	if ((f=fopen(PIMD_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f) && !found)
		{
			striplf(line);
			if (strlen(line))
			{
				args=make_args(line);
				if (!strcmp(args->argv[0], "phyint") && !strcmp(args->argv[1], ifname))
				{
					found=1;
					fprintf(out, " ip pim dense-mode\n");
				}
				destroy_args(args);
			}
		}
		fclose(f);
	}
	if ((f=fopen(PIMS_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f) && !found)
		{
			striplf(line);
			if (strlen(line))
			{
				args=make_args(line);
				if (!strcmp(args->argv[0], "phyint") && !strcmp(args->argv[1], ifname))
				{
					found=1;
					fprintf(out, " ip pim sparse-mode\n");
				}
				destroy_args(args);
			}
		}
		fclose(f);
	}
}

void dump_pim(FILE *out, int conf_format)
{
	FILE *f;
	arglist *args;
	char line[200];

	if ((f=fopen(PIMS_CFG_FILE, "r")) != NULL)
	{
		while (fgets(line, 200, f))
		{
			striplf(line);
			if (strlen(line)) {
				args=make_args(line);
				if (!strcmp(args->argv[0], "cand_bootstrap_router")) {
					if (args->argc == 2) 
						fprintf(out, "ip pim bsr-candidate %s\n", convert_os_device(args->argv[1], 0));
					else if (args->argc == 4) 
						fprintf(out, "ip pim bsr-candidate %s priority %s\n", convert_os_device(args->argv[1], 0), args->argv[3]);
					break; /* while */
				}
				destroy_args(args);
			}
		}
		rewind(f); /* Returns to beggining of file */
		while (fgets(line, 200, f)) {
			striplf(line);
			if (strlen(line)) {
				args=make_args(line);
				if (!strcmp(args->argv[0], "rp_address")) {
					if (args->argc == 2) 
						fprintf(out, "ip pim rp-address %s\n", args->argv[1]);
					break;
				}
				destroy_args(args);
			}
		}
		rewind(f);
		while (fgets(line, 200, f)) {
			striplf(line);
			if (strlen(line)) {
				args=make_args(line);
				if (!strcmp(args->argv[0], "cand_rp")) {
					if (args->argc == 2) 
						fprintf(out, "ip pim rp-candidate %s\n", convert_os_device(args->argv[1], 0));
					else if (args->argc == 4) 
						fprintf(out, "ip pim rp-candidate %s priority %s\n", convert_os_device(args->argv[1], 0), args->argv[3]);
					else if (args->argc == 6) 
						fprintf(out, "ip pim rp-candidate %s priority %s interval %s\n", 
						convert_os_device(args->argv[1], 0), args->argv[3], args->argv[5]);
					break;
				}				
				destroy_args(args);
			}
		}
		fclose(f);
	}
}
#endif

