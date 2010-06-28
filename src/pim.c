/*
 * pim.h
 *
 *  Created on: Jun 24, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "options.h"
#include "args.h"
#include "defines.h"
#include "device.h"
#include "error.h"
#include "exec.h"
#include "pim.h"
#include "str.h"

#ifdef OPTION_PIMD
static void libconfig_pim_dense_hup(void)
{
	FILE *f;
	pid_t pid;
	char buf[32];

	if (!(f = fopen(PIM_DENSE_PID, "r")))
		return;

	fgets(buf, 31, f);
	fclose(f);

	pid = (pid_t) atoi(buf);

	if (pid > 1)
		kill(pid, SIGHUP);
}

static void libconfig_pim_sparse_hup(void)
{
	FILE *f;
	pid_t pid;
	char buf[32];

	if (!(f = fopen(PIM_SPARSE_PID, "r")))
		return;

	fgets(buf, 31, f);
	fclose(f);

	pid = (pid_t) atoi(buf);

	if (pid > 1)
		kill(pid, SIGHUP);
}

int libconfig_pim_dense_phyint(int add, char *dev)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, intf = 0, found = 0, lines = 0;
	char line[200];

	if ((f = fopen(PIMD_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES) {
			libconfig_str_striplf(line);
			if (strlen(line))
				args[lines++] = libconfig_make_args(line);
		}
		fclose(f);
	}

	if ((f = fopen(PIMD_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (!strcmp(args[i]->argv[0], "phyint")) {

				if (!strcmp(args[i]->argv[1], dev)) {

					found = 1;

					if (!add) {
						libconfig_destroy_args(args[i]);
						/* skip line (delete) */
						continue;
					}
				}

				intf++;

			} else {
				/* add after phyint declarations */
				if (add && !found) {
					found = 1;
					fprintf(f, "phyint %s enable preference 101 metric 1024\n", dev);
					intf++;
				}
			}

			for (j = 0; j < args[i]->argc; j++) {
				if (j < args[i]->argc - 1)
					fprintf(f, "%s ", args[i]->argv[j]);
				else
					fprintf(f, "%s\n", args[i]->argv[j]);
			}

			libconfig_destroy_args(args[i]);
		}

		fclose(f);
	}

	libconfig_pim_dense_hup();

	return intf;
}

int libconfig_pim_sparse_phyint(int add, char *dev)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, intf = 0, found = 0, lines = 0;
	char line[200];

	if ((f = fopen(PIMS_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES) {
			libconfig_str_striplf(line);
			if (strlen(line))
				args[lines++] = libconfig_make_args(line);
		}
		fclose(f);
	}

	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (!strcmp(args[i]->argv[0], "phyint")) {

				if (!strcmp(args[i]->argv[1], dev)) {

					found = 1;

					if (!add) {
						libconfig_destroy_args(args[i]);
						/* skip line (delete) */
						continue;
					}
				}

				intf++;

			} else {
				/* add after phyint declarations */
				if (add && !found) {
					found = 1;
					fprintf(f, "phyint %s enable preference 101 metric 1024\n", dev);
					intf++;
				}
			}

			for (j = 0; j < args[i]->argc; j++) {
				if (j < args[i]->argc - 1)
					fprintf(f, "%s ", args[i]->argv[j]);
				else
					fprintf(f, "%s\n", args[i]->argv[j]);
			}

			libconfig_destroy_args(args[i]);
		}

		fclose(f);
	}
	libconfig_pim_sparse_hup();
	return intf;
}

void libconfig_pim_sparse_bsr_candidate(int add,
                                        char *dev,
                                        char *major,
                                        char *priority)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, found = 0, lines = 0;
	char line[200];

	if ((f = fopen(PIMS_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES) {
			libconfig_str_striplf(line);
			if (strlen(line)) {
				args[lines] = libconfig_make_args(line);
				if (!strcmp(args[lines]->argv[0], "cand_bootstrap_router"))
					found = 1;
				lines++;
			}
		}
		fclose(f);
	}

	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (strcmp(args[i]->argv[0], "phyint")) {

				if (add && !found) {
					found = 1;
					if (priority == NULL)
						fprintf(f, "cand_bootstrap_router %s%s\n", dev, major);
					else
						fprintf(f, "cand_bootstrap_router %s%s priority %s\n",
								dev, major, priority);
				}

				if (!strcmp(args[i]->argv[0], "cand_bootstrap_router")) {

					if (add) {
						if (priority == NULL)
							fprintf(f, "cand_bootstrap_router %s%s\n",
							                dev, major);
						else
							fprintf(f, "cand_bootstrap_router %s%s priority %s\n",
							                dev, major, priority);
					}

					found = 1;

					libconfig_destroy_args(args[i]);
					/* skip line (delete) */
					continue;
				}
			}

			for (j = 0; j < args[i]->argc; j++) {
				if (j < args[i]->argc - 1)
					fprintf(f, "%s ", args[i]->argv[j]);
				else
					fprintf(f, "%s\n", args[i]->argv[j]);
			}

			libconfig_destroy_args(args[i]);
		}

		fclose(f);
	}

	libconfig_pim_sparse_hup();
}

void libconfig_pim_sparse_rp_address(int add, char *rp)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, found = 0, lines = 0;
	char line[200];

	if ((f = fopen(PIMS_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES) {
			libconfig_str_striplf(line);
			if (strlen(line)) {
				args[lines] = libconfig_make_args(line);
				if (!strcmp(args[lines]->argv[0], "rp_address"))
					found = 1;
				lines++;
			}
		}
		fclose(f);
	}

	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (strcmp(args[i]->argv[0], "phyint")) {

				if (add && !found) {
					found = 1;
					fprintf(f, "rp_address %s\n", rp);
				}

				if (!strcmp(args[i]->argv[0], "rp_address")) {
					if (add) {
						fprintf(f, "rp_address %s\n", rp);
					}

					found = 1;
					libconfig_destroy_args(args[i]);
					/* skip line (delete) */
					continue;
				}
			}

			for (j = 0; j < args[i]->argc; j++) {
				if (j < args[i]->argc - 1)
					fprintf(f, "%s ", args[i]->argv[j]);
				else
					fprintf(f, "%s\n", args[i]->argv[j]);
			}

			libconfig_destroy_args(args[i]);
		}

		fclose(f);
	}

	libconfig_pim_sparse_hup();
}

void libconfig_pim_sparse_rp_candidate(int add,
                        char *dev,
                        char *major,
                        char *priority,
                        char *interval)
{
	FILE *f;
	arglist *args[MAX_LINES];
	int i, j, found = 0, lines = 0;
	char line[200];

	if ((f = fopen(PIMS_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES) {
			libconfig_str_striplf(line);
			if (strlen(line)) {
				args[lines] = libconfig_make_args(line);
				if (!strcmp(args[lines]->argv[0], "cand_rp"))
					found = 1;
				lines++;
			}
		}
		fclose(f);
	}

	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (strcmp(args[i]->argv[0], "phyint")) {

				if (add && !found) {
					found = 1;

					if (priority == NULL)
						fprintf(f, "cand_rp %s%s\n", dev, major);
					else if (interval == NULL)
						fprintf(f, "cand_rp %s%s priority %s\n",
						                dev, major, priority);
					else
						fprintf(f, "cand_rp %s%s priority %s time %s\n",
								dev, major, priority, interval);
				}

				if (!strcmp(args[i]->argv[0], "cand_rp")) {

					if (add) {
						if (priority == NULL)
							fprintf(f, "cand_rp %s%s\n", dev, major);
						else if (interval == NULL)
							fprintf(f, "cand_rp %s%s priority %s\n",
							                dev, major, priority);
						else
							fprintf(f, "cand_rp %s%s priority %s time %s\n",
							                dev, major, priority, interval);
					}

					found = 1;
					libconfig_destroy_args(args[i]);

					/* skip line (delete) */
					continue;
				}
			}

			for (j = 0; j < args[i]->argc; j++) {
				if (j < args[i]->argc - 1)
					fprintf(f, "%s ", args[i]->argv[j]);
				else
					fprintf(f, "%s\n", args[i]->argv[j]);
			}

			libconfig_destroy_args(args[i]);
		}

		fclose(f);
	}

	libconfig_pim_sparse_hup();
}

void libconfig_pim_dump_interface(FILE *out, char *ifname)
{
	FILE *f;
	arglist *args;
	int found = 0;
	char line[200];

	if ((f = fopen(PIMD_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && !found) {

			libconfig_str_striplf(line);

			if (strlen(line)) {

				args = libconfig_make_args(line);

				if (!strcmp(args->argv[0], "phyint") &&
						!strcmp(args->argv[1], ifname)) {
					found = 1;
					fprintf(out, " ip pim dense-mode\n");
				}

				libconfig_destroy_args(args);
			}
		}

		fclose(f);
	}

	if ((f = fopen(PIMS_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && !found) {

			libconfig_str_striplf(line);

			if (strlen(line)) {

				args = libconfig_make_args(line);

				if (!strcmp(args->argv[0], "phyint") &&
						!strcmp(args->argv[1], ifname)) {
					found = 1;
					fprintf(out, " ip pim sparse-mode\n");
				}

				libconfig_destroy_args(args);
			}
		}

		fclose(f);
	}
}

void libconfig_pim_dump(FILE *out)
{
	FILE *f;
	arglist *args;
	char line[200];

	if ((f = fopen(PIMS_CFG_FILE, "r")) != NULL) {

		while (fgets(line, 200, f)) {
			libconfig_str_striplf(line);

			if (strlen(line)) {
				args = libconfig_make_args(line);

				if (!strcmp(args->argv[0], "cand_bootstrap_router")) {
					if (args->argc == 2)
						fprintf(out, "ip pim bsr-candidate %s\n",
						                libconfig_device_convert_os(args->argv[1], 0));
					else if (args->argc == 4)
						fprintf(out, "ip pim bsr-candidate %s priority %s\n",
								libconfig_device_convert_os(args->argv[1], 0), args->argv[3]);

					/* while */
					break;
				}

				libconfig_destroy_args(args);
			}

		}

		/* Returns to beggining of file */
		rewind(f);

		while (fgets(line, 200, f)) {

			libconfig_str_striplf(line);

			if (strlen(line)) {
				args = libconfig_make_args(line);

				if (!strcmp(args->argv[0], "rp_address")) {
					if (args->argc == 2)
						fprintf(out, "ip pim rp-address %s\n", args->argv[1]);

					break;
				}

				libconfig_destroy_args(args);
			}
		}

		rewind(f);

		while (fgets(line, 200, f)) {
			libconfig_str_striplf(line);

			if (strlen(line)) {
				args = libconfig_make_args(line);

				if (!strcmp(args->argv[0], "cand_rp")) {
					if (args->argc == 2)
						fprintf(out,"ip pim rp-candidate %s\n",
								libconfig_device_convert_os(args->argv[1], 0));
					else if (args->argc == 4)
						fprintf(out, "ip pim rp-candidate %s priority %s\n",
						                libconfig_device_convert_os(args->argv[1], 0), args->argv[3]);
					else if (args->argc == 6)
						fprintf(out, "ip pim rp-candidate %s priority %s interval %s\n",
						                libconfig_device_convert_os(args->argv[1], 0),
						                args->argv[3],
						                args->argv[5]);

					break;
				}

				libconfig_destroy_args(args);
			}
		}

		fclose(f);
	}
}
#endif /* OPTION_PIMD */

