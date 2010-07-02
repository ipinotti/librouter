/*
 * qos.c
 *
 *  Created on: Jun 24, 2010
 */

#include <linux/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <syslog.h>
#include <linux/hdlc.h>
#include <linux/mii.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "args.h"
#include "defines.h"
#include "device.h"
#include "exec.h"
#include "dev.h"
#include "lan.h"
#include "ppp.h"
#include "qos.h"
#include "ip.h"

static int _asort(const void *a, const void *b)
{
	return (-strcmp((*((struct dirent **) a))->d_name,
	                (*((struct dirent **) b))->d_name));
}

static struct ds_class
{
	const char *name;
	unsigned int dscp;
} ds_classes[] = 
{
	{ "CS1",  0x08 },
	{ "CS2",  0x10 },
	{ "CS3",  0x18 },
	{ "CS4",  0x20 },
	{ "CS5",  0x28 },
	{ "CS1",  0x08 },
	{ "CS2",  0x10 },
	{ "CS3",  0x18 },
	{ "CS6",  0x30 },
	{ "CS7",  0x38 },
	{ "BE",   0x00 },
	{ "AF11", 0x0a },
	{ "AF12", 0x0c },
	{ "AF13", 0x0e },
	{ "AF21", 0x12 },
	{ "AF22", 0x14 },
	{ "AF23", 0x16 },
	{ "AF31", 0x1a },
	{ "AF32", 0x1c },
	{ "AF33", 0x1e },
	{ "AF41", 0x22 },
	{ "AF42", 0x24 },
	{ "AF43", 0x26 },
	{ "EF",   0x2e }
};

unsigned int librouter_qos_class_to_dscp(const char *name)
{
	int i;

	for (i = 0; i < sizeof(ds_classes) / sizeof(struct ds_class); i++) {
		if (!strncasecmp(name, ds_classes[i].name, strlen(ds_classes[i].name)))
			return ds_classes[i].dscp;
	}
	return 0;
}

const char *librouter_qos_dscp_to_name(unsigned int dscp)
{
	int i;

	for (i = 0; i < sizeof(ds_classes) / sizeof(struct ds_class); i++) {
		if (dscp == ds_classes[i].dscp)
			return ds_classes[i].name;
	}

	return NULL;
}
/*******************************************************/
/* Show functions */
/*******************************************************/
void librouter_qos_dump_config(FILE *out)
{
	struct dirent **namelist;
	int n, i;

	n = scandir("/var/run", &namelist, 0, _asort);
	if (n < 0)
		return;
	while (n--) {
		char *name = namelist[n]->d_name;

		if (strstr(name, ".policy")) {
			int len;

			len = strlen(name);
			if (len > strlen(".policy")) {
				pmap_cfg_t *pmap;

				name[len - strlen(".policy")] = 0;
				if (librouter_qos_get_policymap(name, &pmap) > 0) {
					fprintf(out, "policy-map %s\n", name);

					if (pmap->description[0])
						fprintf(out, " description %s\n", pmap->description);

					for (i = 0; i < pmap->n_mark; i++) {
						fprintf(out, " mark %d\n", pmap->pmark[i].mark);

						/* bandwidth */
						if (pmap->pmark[i].bw_remain_perc)
							fprintf(out, "  bandwidth remaining percent %d\n", pmap->pmark[i].bw_remain_perc);
						else if (pmap->pmark[i].bw_perc)
							fprintf(out, "  bandwidth percent %d\n", pmap->pmark[i].bw_perc);
						else if (pmap->pmark[i].bw)
							fprintf(out, "  bandwidth %dkbps\n", pmap->pmark[i].bw / 1024);
						else
							fprintf(out, "  no bandwidth\n");

						/* ceil */
						if (pmap->pmark[i].ceil_perc)
							fprintf(out, "  ceil percent %d\n", pmap->pmark[i].ceil_perc);
						else if (pmap->pmark[i].ceil)
							fprintf(out, "  ceil %dkbps\n", pmap->pmark[i].ceil / 1024);
						else
							fprintf(out, "  no ceil\n");

						/* queue */
						if (pmap->pmark[i].queue == SFQ) {
							if (pmap->pmark[i].sfq_perturb)
								fprintf(out, "  queue sfq %d\n", pmap->pmark[i].sfq_perturb);
							else
								fprintf(out, "  queue sfq\n");
						} else if (pmap->pmark[i].queue == RED) {
							fprintf(out, "  queue red %d %d %s\n",
							                pmap->pmark[i].red_latency,
							                pmap->pmark[i].red_probability,
							                pmap->pmark[i].red_ecn ? "ecn" : "");
						} else if (pmap->pmark[i].queue == WFQ) {
							fprintf(out, "  queue wfq %d\n", pmap->pmark[i].wfq_hold_queue);
						} else {
							if (pmap->pmark[i].fifo_limit)
								fprintf(out, "  queue fifo %d\n", pmap->pmark[i].fifo_limit);
							else
								fprintf(out, "  queue fifo\n");
						}

						/* real-time*/
						if (pmap->pmark[i].realtime == BOOL_TRUE)
							fprintf(out, "  real-time %d %d\n",
							                pmap->pmark[i].rt_max_delay,
							                pmap->pmark[i].rt_max_unit);
						else
							fprintf(out, "  no real-time\n");
					}
					fprintf(out, "!\n"); /* At the end of each policy-map */
				}
			}
		}
		free(namelist[n]);
	}
	free(namelist);
}

static void _qos_dump_interface(char *dev_name)
{
	int n, idx, in = 0, first = 1;
	int raw_bw = 0, available_bw = 0, remain_bw = 0;
	long mark;
	intf_qos_cfg_t *intf_cfg;
	pmap_cfg_t *pmap;
	pmark_cfg_t *pmark;
	char buf[256], pkts[32], bytes[32], rate[32], dropped[32];
	char *search_str = "class hfsc ";
	FILE *f;

	/* Get config */
	if (librouter_qos_get_interface_config(dev_name, &intf_cfg) <= 0)
		return;

	/* No policy-map applied */
	if (intf_cfg->pname[0] == 0) {
		librouter_qos_release_config(intf_cfg);
		return;
	}

	if (librouter_qos_get_policymap(intf_cfg->pname, &pmap) <= 0) {
		librouter_qos_release_config(intf_cfg);
		return;
	}

	/* No mark configured */
	if (!pmap->n_mark) {
		librouter_qos_free_policymap(pmap);
		librouter_qos_release_config(intf_cfg);
		return;
	}

	pmark = pmap->pmark;
	n = pmap->n_mark;

	/* Need to know how to distribute remaining bandwidth */
	available_bw = (intf_cfg->bw / 100) * intf_cfg->max_reserved_bw;
	for (idx = 0; idx < n; idx++) {
		if (pmark[idx].bw_perc)
			raw_bw += pmark[idx].bw_perc * (intf_cfg->bw / 100);
		else if (pmark[idx].bw)
			raw_bw += pmark[idx].bw;
	}
	remain_bw = available_bw - raw_bw;

	/* Run tc */
	sprintf(buf, "/bin/tc -s class ls dev %s 2> /dev/null", dev_name);
	f = popen(buf, "r");
	if (!f) {
		printf("ERROR: Could not get tc configuration\n");
		librouter_qos_free_policymap(pmap);
		librouter_qos_release_config(intf_cfg);
		return;
	}

	pkts[0] = bytes[0] = rate[0] = dropped[0] = 0;

	while (1) {
		fgets(buf, 256, f);
		buf[255] = 0;

		if (feof(f))
			break;

		if (in) {
			arglist *args;
			char *p;

			if ((strncmp(buf, " Sent", 5) == 0)) {

				p = buf;
				while ((*p) && (*p == ' '))
					p++;

				args = librouter_make_args(p);
				if (args->argc >= 7) {
					strncpy(pkts, args->argv[3], 32);
					pkts[31] = 0;
					strncpy(bytes, args->argv[1], 32);
					bytes[31] = 0;
					sprintf(dropped, "%d", atoi(
					                args->argv[6]));
				}

				librouter_destroy_args(args);
			} else if ((strncmp(buf, " rate", 5) == 0)) {

				p = buf;
				while ((*p) && (*p == ' '))
					p++;

				args = librouter_make_args(p);
				if (args->argc >= 2) {
					unsigned int rate_per_sec = atoi(args->argv[1]);
					if (strstr(args->argv[1], "Mbit"))
						sprintf(rate, "%dkbit", rate_per_sec * 1024);
					else if (strstr(args->argv[1], "Kbit"))
						sprintf(rate, "%dkbit", rate_per_sec);
					else
						sprintf(rate, "%dbit", rate_per_sec);
				}
				librouter_destroy_args(args);
			} else {
				printf(" \t\tSent %s bytes %s pkts, dropped %s pkts", bytes, pkts, dropped);

				if (rate[0])
					printf(", rate %s", rate);

				printf("\n\n");
				in = 0;
				pkts[0] = bytes[0] = rate[0] = dropped[0] = 0;
			}
		} else {
			if (strncmp(buf, search_str, strlen(search_str)) == 0) {
				char *p1, *p2;
				int n1, n2;

				p1 = buf + strlen(search_str);
				p2 = strchr(p1, ':');
				if (p2) {
					n1 = atoi(p1);
					n2 = atoi(p2 + 1);
					mark = n2 - 10;

					for (idx = 0; idx < n; idx++)
						if (pmark[idx].mark == mark)
							break;
					if (idx < n) {
						int bw, ceil;

						if (pmark[idx].bw_remain_perc)
							bw = pmark[idx].bw_remain_perc * (remain_bw / 100);
						else if (pmark[idx].bw_perc)
							bw = pmark[idx].bw_perc * (intf_cfg->bw / 100);
						else
							bw = pmark[idx].bw;

						if (pmark[idx].ceil_perc)
							ceil = pmark[idx].ceil_perc * (intf_cfg->bw / 100);
						else
							ceil = pmark[idx].ceil;

						if (first) {
							printf("\nQoS statistics for interface %s\n\n", dev_name);
							printf("Policy-map %s\n", intf_cfg->pname);
							first = 0;
						}

						printf("\tMark %ld:\n", mark);
						printf("\t\tBandwidth %dkbps ", bw / 1024);

						if (pmark[idx].bw_perc)
							printf("(%d%% of total bandwidth)\n", pmark[idx].bw_perc);
						else if (pmark[idx].bw_remain_perc)
							printf("(%d%% of remaining bandwidth)\n", pmark[idx].bw_remain_perc);
						else
							printf("\n");

						printf("\t\tCeil %dkbps ", (ceil ? ceil / 1024 : bw / 1024));

						if (pmark[idx].ceil_perc)
							printf("(%d%% of total bandwidth)\n", pmark[idx].ceil_perc);
						else
							printf("\n");

						printf("\t\tReal-time : %s\n", (pmark[idx].realtime == BOOL_TRUE) ? "YES" : "NO");

						if (pmark[idx].realtime == BOOL_TRUE)
							printf("\t\tMaximum packet size %d bytes, Maximum delay %d miliseconds\n",
							                pmark[idx].rt_max_unit,
							                pmark[idx].rt_max_delay);

						in = 1;
					}
				}
			}
		}
	}

	pclose(f);

	librouter_qos_free_policymap(pmap);
	librouter_qos_release_config(intf_cfg);
}

void librouter_qos_dump_interfaces(void)
{
	struct dirent **namelist;
	int n;

	n = scandir("/var/run", &namelist, 0, _asort);

	if (n < 0)
		return;

	while (n--) {
		char *name = namelist[n]->d_name;

		if (strstr(name, ".service_policy")) {
			int len = strlen(name);
			if (len > 15) {
				name[len - 15] = 0;
				_qos_dump_interface(name);
			}
		}
		free(namelist[n]);
	}
	free(namelist);
}

/********************************************************************************/
/* tc related functions */
/********************************************************************************/
int librouter_qos_tc_insert_all(char *dev_name)
{
	FILE *f;
	char filename[64];
	intf_qos_cfg_t *intf_cfg;
	pmark_cfg_t *cfg;
	pmap_cfg_t *pmap;
	int n, i;
	dev_family *fam;
	int major;
	int run_tc_now = 0;
	int raw_bw = 0, available_bw = 0, remain_bw = 0;

	librouter_qos_tc_remove_all(dev_name); /* clear rules... */

	sprintf(filename, FILE_QOS_SCRIPT, dev_name);
	f = fopen(filename, "wt");
	if (!f)
		return -1;

	/* Get status from interfaces */
	if ((fam = librouter_device_get_family(dev_name)) == NULL)
		return -1;

	major = atoi(dev_name + strlen(fam->cish_string));

	//	if (fam->type == mobile ) {  --> situação anterior a implementação do 3G
	if (fam->type == ppp) {
		if (librouter_ppp_get_state(major))
			run_tc_now = 1; /* wait for ppp */
	} else if (fam->type == eth) {
		if (librouter_dev_get_link(dev_name))
			run_tc_now = 1;
	}

	/* Check if qos config exists */
	if (librouter_qos_get_interface_config(dev_name, &intf_cfg) > 0) {
		if (intf_cfg->pname[0] != 0) {
			if (librouter_qos_get_policymap(intf_cfg->pname, &pmap) <= 0) {
				printf("ERROR: Could not get policy-map %s configuration\n",
					intf_cfg->pname);
				fclose(f);
				return -1;
			}
			n = pmap->n_mark;
			cfg = pmap->pmark;

			fprintf(f, "qdisc add dev %s root handle 2: hfsc default 5\n", dev_name);

			fprintf(f, "class add dev %s parent 2: classid 2:1 hfsc sc rate %dkbit ul rate %dkbit\n",
			                dev_name,
			                ((intf_cfg->bw / 100) * intf_cfg->max_reserved_bw) / 1024,
			                ((intf_cfg->bw / 100) * intf_cfg->max_reserved_bw) / 1024);

			/* HFSC does not work well without a default class. Create one only for unmarked packets */
			fprintf(f, "class add dev %s parent 2:1 classid 2:5 hfsc ls rate 1kbit ul rate %dkbit\n",
			                dev_name,
			                ((intf_cfg->bw / 100) * intf_cfg->max_reserved_bw) / 1024);

			/* Need to know remaining bandwidth */
			available_bw = (intf_cfg->bw / 100) * intf_cfg->max_reserved_bw;

			for (i = 0; i < n; i++) {
				if (cfg[i].bw_perc)
					raw_bw += cfg[i].bw_perc * (intf_cfg->bw / 100);
				else if (cfg[i].bw)
					raw_bw += cfg[i].bw;
			}

			remain_bw = available_bw - raw_bw;

			for (i = 0; i < n; i++) {
				int bw, ceil;

				if (cfg[i].bw_remain_perc)
					bw = cfg[i].bw_remain_perc * (remain_bw / 100);
				else if (cfg[i].bw_perc)
					bw = cfg[i].bw_perc * (intf_cfg->bw / 100);
				else
					bw = cfg[i].bw;

				if (cfg[i].ceil_perc)
					ceil = cfg[i].ceil_perc * (intf_cfg->bw / 100);
				else
					ceil = cfg[i].ceil;

				fprintf(f, "class add dev %s estimator 1 4 parent 2:1 classid 2:%d hfsc ",
				                dev_name, 10 + cfg[i].mark);

				/*	http://www.spinics.net/lists/lartc/msg12089.html
				 b(t) =
				 m1 * t   				t <= d
				 m1 * d + m2 * (t - d)	t > d
				 b_rt(t) <= b_ls(t) <= b_ul(t) for all t >= 0 */

				if (cfg[i].realtime == BOOL_TRUE) {
					fprintf(f, "sc umax %db dmax %dms rate %dkbit ul rate %dkbit\n",
					                cfg[i].rt_max_unit,
					                cfg[i].rt_max_delay,
					                bw / 1024,
					                (ceil ? ceil / 1024 : bw / 1024));
				} else {
					fprintf(f, " ls rate %dkbit ul rate %dkbit\n",
					                bw / 1024,
					                (ceil ? ceil / 1024 : bw / 1024));
				}

				/* Queue configuration */
				if (cfg[i].queue == SFQ) {
					fprintf(f, "qdisc add dev %s parent 2:%d sfq", dev_name, 10 + cfg[i].mark);
					if (cfg[i].sfq_perturb)
						fprintf(f, " perturb %d\n", cfg[i].sfq_perturb);
					else
						fprintf(f, "\n");
				} else if (cfg[i].queue == WFQ) {
					fprintf(f, "qdisc add dev %s parent 2:%d wfq", dev_name, 10 + cfg[i].mark);
					if (cfg[i].wfq_hold_queue)
						fprintf(f, " hold-queue %d\n", cfg[i].wfq_hold_queue);
					else
						fprintf(f, "\n");
				} else if (cfg[i].queue == RED) {
					/* http://www.opalsoft.net/qos/DS-26.htm */
					int max = (bw / 8) * cfg[i].red_latency / 1000;
					int burst = ((max * 2 / 3 + max) * 2 + (3 * 1000)) / ((3 * 1000) * 2); /* +1/2 (2A+1B)/(2B)) */
					fprintf(f, "qdisc add dev %s parent 2:%d red limit %d min %d max %d avpkt 1000 "
							"burst %d probability %f bandwidth %d %s\n",
					                dev_name,
					                10 + cfg[i].mark,
					                8 * max,
					                ((max / 3 > 1000) ? max / 3 : 1000),
					                ((max > 1000) ? max : 1000),
					                ((burst) ? burst : 1),
					                (float) cfg[i].red_probability / 100,
					                bw / 1000,
					                cfg[i].red_ecn ? "ecn" : "");
				} else {
					fprintf(f, "qdisc add dev %s parent 2:%d pfifo", dev_name, 10 + cfg[i].mark);
					if (cfg[i].fifo_limit)
						fprintf(f, " limit %d\n", cfg[i].fifo_limit);
					else
						fprintf(f, "\n");
				}

				/* Associate marks with classes */
				fprintf(f, "filter add dev %s parent 2: protocol ip prio 1 handle 0x%x fw flowid 2:%d\n",
				                dev_name, cfg[i].mark, 10 + cfg[i].mark);
			}
			librouter_qos_free_policymap(pmap);
		}
		librouter_qos_release_config(intf_cfg);
	}

	fclose(f);

	if (run_tc_now)
		return librouter_exec_prog(1, "/bin/tc", "-batch", filename, NULL);

	return 0;
}

int librouter_qos_tc_remove_all(char *dev_name)
{
	FILE *f;

	f = fopen(FILE_TMP_QOS_DOWN, "wt");

	if (!f)
		return -1;

	fprintf(f, "qdisc del dev %s root\n", dev_name);
#ifdef OPTION_INGRESS
	fprintf(f, "qdisc del dev %s ingress\n", dev_name);
#endif

	fclose(f);

	return librouter_exec_prog(1, "/bin/tc", "-batch", FILE_TMP_QOS_DOWN, NULL);
}
/**************************************************************************/
/* Policy-map functions */
/**************************************************************************/

int librouter_qos_delete_policy_mark(char *name, int mark)
{
	FILE *in, *out;
	char filename_in[64], filename_out[64];
	pmark_cfg_t cfg;
	int mark_deleted = 0;

	sprintf(filename_in, POLICY_CONFIG_FILE, name);

	if (mark == -1) {
		unlink(filename_in);
		return 0;
	}

	sprintf(filename_out, POLICY_CONFIG_FILE".tmp", name);

	in = fopen(filename_in, "rb");
	if (!in)
		return (-1);

	out = fopen(filename_out, "wb");
	if (!out) {
		fclose(in);
		return -1;
	}

	while (!feof(in)) {
		if (fread(&cfg, sizeof(pmark_cfg_t), 1, in) < 1)
			break;

		if (cfg.mark != mark)
			fwrite(&cfg, sizeof(pmark_cfg_t), 1, out);
		else
			mark_deleted = 1;
	}

	if (!mark_deleted)
		printf("%% Mark %d not configured in this policy-map\n", mark);

	fclose(in);
	fclose(out);
	unlink(filename_in);
	rename(filename_out, filename_in);

	return 0;
}

int librouter_qos_add_policy_mark(char *name, int mark)
{
	FILE *f;
	char file_path[64];
	pmark_cfg_t cfg;
	int mark_exists = 0;

	sprintf(file_path, POLICY_CONFIG_FILE, name);

	f = fopen(file_path, "rb");
	if (!f)
		return -1;

	while (!feof(f)) {
		if (fread(&cfg, sizeof(pmark_cfg_t), 1, f) < 1)
			break;

		if (cfg.mark == mark)
			mark_exists = 1;
	}

	/* Mark does not exist, it must be added */
	if (!mark_exists) {
		pmark_cfg_t new_mark;

		/* Set initial values */
		memset(&new_mark, 0, sizeof(pmark_cfg_t));
		new_mark.mark = mark;
		new_mark.queue = FIFO;
		new_mark.realtime = BOOL_FALSE;

		/* Append to the end of file*/
		f = fopen(file_path, "ab");
		if (f) {
			fwrite(&new_mark, sizeof(pmark_cfg_t), 1, f);
			fclose(f);
		}
	}

	return 0;
}
int librouter_qos_create_policymap(char *policy)
{
	int fd;
	char file_path[64];

	sprintf(file_path, POLICY_CONFIG_FILE, policy);
	fd = open(file_path, O_RDWR);

	/* Does not exist. Create a new one... */
	if (fd < 0) {
		fd = creat(file_path, S_IRUSR | S_IWUSR); /* Read and write permission */

		if (fd < 0) {
			printf("%% ERROR: Could not create configuration file: %s\n", file_path);
			return -1;
		}
		close(fd);
	} else {
		printf("Policy-map %s already exists.\n", policy);
		return -1;
	}

	return 0;
}

int librouter_qos_get_policymap(char *policy, pmap_cfg_t **pmap)
{
	int fd;
	char file_path[64];
	struct stat st;
	pmark_cfg_t *pmark = NULL;

	sprintf(file_path, POLICY_CONFIG_FILE, policy);

	fd = open(file_path, O_RDWR);
	if (fd != -1) {

		if (fstat(fd, &st))
			return -1;

		*pmap = (pmap_cfg_t *) malloc(sizeof(pmap_cfg_t));
		pmark = (pmark_cfg_t *) mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

		if (pmark == ((pmark_cfg_t *) MAP_FAILED)) {
			printf("(%s) error : %s\n", __FUNCTION__, strerror(errno));
			return -1;
		}

		(*pmap)->pmark = pmark;
		close(fd);

		/* Indicates how many marks configured for this policymap */
		(*pmap)->n_mark = (st.st_size) / sizeof(pmark_cfg_t);
		(*pmap)->description[0] = 0; /* clean description */

		librouter_qos_get_policymap_desc(policy, *pmap);

		return 1;
	}

	return 0;
}

void librouter_qos_destroy_policymap(char *policy)
{
	char file_path[64];

	sprintf(file_path, POLICY_CONFIG_FILE, policy);

	if (unlink(file_path) < -1)
		printf("%% Policy %s does not exist", policy);
}

int librouter_qos_get_mark_index(int mark, pmap_cfg_t *pmap)
{
	int i;

	if (mark == 0) {
		printf("%% mark value cannot be zero\n");
		return -1;
	}

	for (i = 0; i < (pmap->n_mark); i++) {
		if (mark == (pmap->pmark[i]).mark)
			break;
	}

	/* Returns n_mark if mark not found */
	return i;
}

void librouter_qos_free_policymap(pmap_cfg_t *pmap)
{
	if (pmap && (pmap != (pmap_cfg_t *) -1)) {
		if (pmap->pmark && (pmap->pmark != (pmark_cfg_t *) -1))
			munmap(pmap->pmark, (pmap->n_mark)* sizeof(pmark_cfg_t));

		free(pmap);
		pmap = NULL;
	}
}

int librouter_qos_save_policymap_desc(char *name, pmap_cfg_t *pmap)
{
	char file_name[128];
	FILE *f;

	sprintf(file_name, POLICY_DESC_FILE, name);

	f = fopen(file_name, "w+");
	if (!f)
		return -1;

	fwrite(pmap->description, 255, 1, f);
	fclose(f);

	return 0;
}

int librouter_qos_get_policymap_desc(char *name, pmap_cfg_t *pmap)
{
	char file_name[128];
	FILE *f;

	sprintf(file_name, POLICY_DESC_FILE, name);

	f = fopen(file_name, "r");
	if (!f)
		return -1;

	fread(pmap->description, 255, 1, f);
	fclose(f);

	return 0;
}

void librouter_qos_destroy_policymap_desc(char *name)
{
	char file_name[128];

	sprintf(file_name, POLICY_DESC_FILE, name);
	unlink(file_name);
}
/*********************************************************************/
/* Service-policy functions */
/*********************************************************************/

void librouter_qos_clean_config(char *dev_name)
{
	int n;
	char filename[64], cleaname[64];
	struct dirent **namelist;

	sprintf(filename, SERVICE_POLICY_CONFIG_FILE, dev_name);
	unlink(filename);

	n = scandir("/var/run", &namelist, 0, _asort);
	if (n < 0)
		return;

	while (n--) {
		int len;
		char *name = namelist[n]->d_name;

		sprintf(filename, "qos.%s", dev_name);
		if (strncmp(name, filename, strlen(filename)) == 0) {
			len = strlen(name);
			if (len > strlen(filename)) {
				sprintf(cleaname, "/var/run/%s", name);
				if (strncmp(name + len - 3, ".up", 3) == 0)
					unlink(cleaname);
			}
		}

		/* Clean frts files too... */
		sprintf(filename, "frts.%s", dev_name);
		if (strncmp(name, filename, strlen(filename)) == 0) {
			len = strlen(name);
			if (len > strlen(filename)) {
				sprintf(cleaname, "/var/run/%s", name);
				if (strncmp(name + len - 4, ".cfg", 4) == 0)
					unlink(cleaname);
			}
		}

		free(namelist[n]);
	}

	free(namelist);
}

int librouter_qos_create_interface_config(char *dev)
{
	char file_path[64];
	int fd;

	sprintf(file_path, SERVICE_POLICY_CONFIG_FILE, dev);
	fd = open(file_path, O_RDWR);

	/* Does not exist. Create a new one... */
	if (fd < 0) {
		intf_qos_cfg_t default_cfg;

		/* Default values */

		if (strstr(dev, "aux")) {
			/* 9600 bps */
			default_cfg.bw = 9600;
		} else if (strstr(dev, "serial")) {
			/* 2 Mbit */
			default_cfg.bw = 2048000;
		} else if (strstr(dev, "ethernet")) {
			/* 100 Mbit */
			default_cfg.bw = 100000000;
		} else {
			/* Not a valid interface */
			return -1;
		}

		default_cfg.max_reserved_bw = 75; /* 75% */
		default_cfg.pname[0] = 0; /* DEFAULT: No policy map configured */

		fd = creat(file_path, S_IRUSR | S_IWUSR); /* Read and write permission */

		if (fd < 0) {
			printf("%% ERROR: Could not create configuration file: %s\n", file_path);
			return -1;
		}

		write(fd, &default_cfg, sizeof(intf_qos_cfg_t));
		close(fd);
	}

	return 0;
}

void librouter_qos_release_config(intf_qos_cfg_t *intf_cfg)
{
	if (intf_cfg && (intf_cfg != (intf_qos_cfg_t *) -1))
		munmap(intf_cfg, sizeof(intf_qos_cfg_t));
}

/**
 * Checks if a policy-map is active on an interface
 *
 * @param policy
 * @return
 */
char *librouter_qos_check_active_policy(char *policy)
{
	struct dirent **namelist;
	int n;
	intf_qos_cfg_t *intf_cfg;
	char *dev = NULL;

	n = scandir("/var/run", &namelist, 0, _asort);

	if (n < 0)
		return NULL;

	while (n--) {
		char *name = namelist[n]->d_name;

		if (strstr(name, ".service_policy")) {
			int len = strlen(name);

			if (len > 15) {
				name[len - 15] = 0;
				if (librouter_qos_get_interface_config(name, &intf_cfg) > 0) {
					if (!strcmp(policy, intf_cfg->pname)) {
						dev = (char *) malloc(strlen(name + 1));
						strcpy(dev, name);
					}
				}
			}
		}
		free(namelist[n]);
	}

	free(namelist);

	return dev;
}

/**
 * Get configuration in filesystem. Return 0 if file does not exist.
 *
 * @param dev
 * @param intf_cfg
 * @return
 */
int librouter_qos_get_interface_config(char *dev, intf_qos_cfg_t **intf_cfg)
{
	char file_path[64];
	int fd;

	sprintf(file_path, SERVICE_POLICY_CONFIG_FILE, dev);
	fd = open(file_path, O_RDWR);
	if (fd < 0)
		return 0;
	*intf_cfg = (intf_qos_cfg_t *) mmap(NULL, sizeof(intf_qos_cfg_t),
	                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (*intf_cfg == (intf_qos_cfg_t *) -1)
		return -1;
	return 1;
}

/**
 * Check the consistency of the policy-map. If it is OK, calls tc_insert_all
 *
 * @param dev
 * @param policymap
 */
void librouter_qos_apply_policy(char *dev, char *policymap)
{
	int fd;
	intf_qos_cfg_t *intf_cfg = NULL;
	pmap_cfg_t *pmap = NULL;
	pmark_cfg_t *pmark = NULL;
	char file_path[64];
	int available_bw, raw_bw = 0, remain_bw = 0;
	int tmp = 0, i;
	const int log = 1;

	/* Get configuration file. If it does not exist it must be created. */
	if (librouter_qos_get_interface_config(dev, &intf_cfg) <= 0) {
		librouter_qos_create_interface_config(dev);
		if (librouter_qos_get_interface_config(dev, &intf_cfg) <= 0) {
			printf("%% Could not get %s configuration file.\n", dev);
			goto apply_policy_err;
		}
	}

	/* From here, we must perform several tests that will indicate
	 * if the QoS configuration can be applied to this interface:
	 * Step 1: Check if policy-map exists! */
	sprintf(file_path, POLICY_CONFIG_FILE, policymap);
	fd = open(file_path, O_RDONLY);

	if (fd < 0) {
		printf("%% Policy-map %s does not exist.\n", policymap);
		goto apply_policy_err;
	}

	/* Step 2: Get policy-map configuration */
	if (librouter_qos_get_policymap(policymap, &pmap) <= 0) {
		printf("%%Could not get Policy-map configuration\n");
		goto apply_policy_err;
	}

	/* Step 3: Check number of marks configured in the policy map */
	if (pmap->n_mark == 0) {
		printf("%% No marks configured in this policy.\n");

		if (log)
			syslog(LOG_INFO, "disabling QoS on %s; No marks configured in policy-map %s",
			                dev, policymap);
		goto apply_policy_err;
	}

	pmark = pmap->pmark;

	/* Ok, now is the real deal. The whole policy map configuration must be validated in
	 * order to be able to apply this configuration. Here we go... */

	/* REALTIME VERIFICATION */
	/* Check how much we got for this interface */
	available_bw = (intf_cfg->bw / 100) * intf_cfg->max_reserved_bw;

	/* Check if requested bandwidth can be guaranteed */
	for (i = 0; i < pmap->n_mark; i++) {
		if (pmark[i].realtime == BOOL_TRUE)
			/* bits per sec*/
			tmp += (pmark[i].rt_max_unit * 8 * 1000) / pmark[i].rt_max_delay;
	}

	if (tmp > available_bw) {
		printf("%% Not enough bandwidth to guarantee real-time traffic.\n");
		printf("%% Available bandwidth: %d bps\n", available_bw);
		printf("%% Needed : %d bps \n", tmp);

		if (log) {
			syslog(LOG_INFO, "disabling QoS on %s; Not enough bandwidth to guarantee real-time traffic",
			                dev);
			syslog(LOG_INFO, "Available bandwidth: %d bps",
			                available_bw);
			syslog(LOG_INFO, "Needed : %d bps", tmp);
		}

		goto apply_policy_err;
	}

	/*******************************************************************************/
	/* BANDWIDTH VERIFICATION ******************************************************/
	/*******************************************************************************/

	/* Check how much we got for this interface */
	available_bw = (intf_cfg->bw / 100) * intf_cfg->max_reserved_bw;

	/* check raw bandwidth first */
	for (i = 0; i < pmap->n_mark; i++) {
		if (!(pmark[i].bw_perc) && !(pmark[i].bw_remain_perc))
			raw_bw += (pmark[i]).bw;
	}

	/* check percentages */
	for (i = 0; i < pmap->n_mark; i++) {
		if (pmark[i].bw_perc) {
			raw_bw += (intf_cfg->bw / 100) * pmark[i].bw_perc;
		}
	}

	if (raw_bw > available_bw) {
		printf("%% Configured bandwidth (%d bps) higher than "
			"available (%d bps).\n", raw_bw, available_bw);

		if (log) {
			syslog(LOG_INFO, "disabling QoS on %s; Configured bandwidth (%d bps) higher than "
					"available (%d bps).", dev,
			                raw_bw, available_bw);
		}

		goto apply_policy_err;
	}

	/* check remaining bandwidth values */
	for (i = 0; i < pmap->n_mark; i++) {
		if (pmark[i].bw_remain_perc) {
			remain_bw += pmark[i].bw_remain_perc;
		}
	}

	if (remain_bw > 100) {
		printf("%% percentage values for remaining bandwidth exceed 100%%.\n");

		if (log) {
			syslog(LOG_INFO, "disabling QoS on %s; percentage values for "
					"remaining bandwidth exceed 100%%",
			                dev);
		}

		goto apply_policy_err;
	}

	/* CEIL VERIFICATION */
	/* Check how much we got for this interface */
	available_bw = (intf_cfg->bw / 100) * intf_cfg->max_reserved_bw;

	/* Ceil verification is much simpler */
	for (i = 0; i < pmap->n_mark; i++) {
		if (pmark[i].ceil > available_bw) {

			printf("%% Invalid ceil value for mark %d : %dbps\n"
					"  Ceil value cannot exceed the configured bandwidth (%dbps) for this interface\n",
			                pmark[i].mark, pmark[i].ceil,
			                available_bw);

			if (log) {
				syslog(LOG_INFO, "disabling QoS on %s; Invalid ceil value for mark %d : %dbps ",
				                dev, pmark[i].mark,
				                pmark[i].ceil);
				syslog(LOG_INFO, "Ceil value cannot exceed the configured bandwidth (%dbps) for "
					       "this interface",
				                available_bw);
			}

			goto apply_policy_err;

		} else if (pmark[i].ceil_perc) {

			if (pmark[i].ceil_perc > intf_cfg->max_reserved_bw) {
				printf("%% Invalid ceil percentage for mark %d: %d%%\n"
						"  Ceil value cannot exceed max-reserved-bandwidth (%d%%) for this interface\n",
				                pmark[i].mark,
				                pmark[i].ceil_perc,
				                intf_cfg->max_reserved_bw);

				if (log) {
					syslog(LOG_INFO, "disabling QoS on %s; Invalid ceil percentage for mark %d: %d%% ",
					                dev, pmark[i].mark,
					                pmark[i].ceil_perc);
					syslog(LOG_INFO, "Ceil value cannot exceed max-reserved-bandwidth (%d%%) for "
						       "this interface",
					                intf_cfg->max_reserved_bw);
				}

				goto apply_policy_err;
			}
		}
	}

	/* MARK VERIFICATION */
	/* We must respect: real-time rate <= bandwidth <= ceil */

	/* remaining bw */
	available_bw -= raw_bw;

	for (i = 0; i < pmap->n_mark; i++) {
		int tmp_bw = 0, tmp_ceil = 0;

		if (pmark[i].bw_remain_perc)
			tmp_bw = pmark[i].bw_remain_perc * (available_bw / 100);
		else if (pmark[i].bw_perc)
			tmp_bw = pmark[i].bw_perc * (intf_cfg->bw / 100);
		else
			tmp_bw = pmark[i].bw;

		if (pmark[i].ceil_perc)
			tmp_ceil = pmark[i].ceil_perc * (intf_cfg->bw / 100);
		else
			tmp_ceil = pmark[i].ceil;

		if ((tmp_bw > tmp_ceil) && tmp_ceil) {

			printf("%% Mark %d has bandwidth (%d bps) bigger than ceil (%d bps)\n",
			                pmark[i].mark, tmp_bw, tmp_ceil);

			if (log) {
				syslog(LOG_INFO, "Mark %d has bandwidth (%d bps) bigger than ceil (%d bps)",
						pmark[i].mark, tmp_bw, tmp_ceil);
			}

			goto apply_policy_err;
		}
	}

	/* Reaching this point means configuration is consistent */
	strncpy(intf_cfg->pname, policymap, 31);
	librouter_qos_tc_insert_all(dev);

apply_policy_err:

	librouter_qos_free_policymap(pmap);
	librouter_qos_release_config(intf_cfg);

	return;
}

void librouter_qos_config_reserved_bw(char *dev, int reserved_bw)
{
	intf_qos_cfg_t *intf_cfg = NULL;

	if (librouter_qos_get_interface_config(dev, &intf_cfg) <= 0) {
		librouter_qos_create_interface_config(dev);
		if (librouter_qos_get_interface_config(dev, &intf_cfg) <= 0) {
			printf("%% Could not get qos config for %s\n", dev);
			return;
		}
	}

	if (!(intf_cfg->pname[0]))
		intf_cfg->max_reserved_bw = reserved_bw;
	else
		printf("Policy-map %s applied to interface %s. Please disable it before configuring.\n",
		                intf_cfg->pname, dev);

	librouter_qos_release_config(intf_cfg);
}

void librouter_qos_config_interface_bw(char *dev, int bw)
{
	intf_qos_cfg_t *intf_cfg = NULL;

	if (librouter_qos_get_interface_config(dev, &intf_cfg) <= 0) {
		librouter_qos_create_interface_config(dev);
		if (librouter_qos_get_interface_config(dev, &intf_cfg) <= 0) {
			printf("%% Could not get qos config for %s\n", dev);
			return;
		}
	}

	if (!(intf_cfg->pname[0]))
		intf_cfg->bw = bw;
	else
		printf("Policy-map %s applied to interface %s. Please disable it before configuring.\n",
		                intf_cfg->pname, dev);

	librouter_qos_release_config(intf_cfg);
}

int librouter_qos_get_frts_config(char *dev_name, frts_cfg_t **cfg)
{
	int fd;
	frts_cfg_t *p;
	char filename[64];
	struct stat st;

	sprintf(filename, FILE_FRTS_CFG, dev_name);

	if ((fd = open(filename, O_RDWR)) < 0)
		return 0;

	if (fstat(fd, &st))
		return 0;

	p = (frts_cfg_t *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (p == ((frts_cfg_t *) -1))
		return 0;

	close(fd);
	*cfg = p;

	return (st.st_size / sizeof(frts_cfg_t));
}

int librouter_qos_release_frts_config(frts_cfg_t *cfg, int n)
{
	return munmap(cfg, sizeof(frts_cfg_t) * n);
}

int librouter_qos_add_frts_config(char *dev_name, frts_cfg_t *cfg)
{
	FILE *f;
	char filename[64];

	sprintf(filename, FILE_FRTS_CFG, dev_name);

	/* do not append! */
	f = fopen(filename, "wb");
	if (!f)
		return (-1);

	fwrite(cfg, sizeof(frts_cfg_t), 1, f);
	fclose(f);

	return 0;
}

int librouter_qos_del_frts_config(char *dev_name)
{
	char filename[64];

	sprintf(filename, FILE_FRTS_CFG, dev_name);
	unlink(filename);

	return 0;
}

/**
 * Retorna a estatitica de bytes enviados para uma marca de qos.
 * A busca se baseia no numero da marca e na interface a qual estah associada.
 * Se a marca nao existe ou nao estah associada a interface especificada, retorna -1.
 *
 * @param dev_name
 * @param mark
 * @return
 */
int librouter_qos_get_stats_by_devmark(char *dev_name, int mark)
{
	FILE *f;
	char *p, buf[256];
	arg_list argl = NULL;
	int n1, out = 0, ret = -1;

	intf_qos_cfg_t *intf_cfg;
	pmap_cfg_t *pmap;

	/* Get config */
	if (librouter_qos_get_interface_config(dev_name, &intf_cfg) <= 0)
		return ret;

	if (intf_cfg->pname[0] == 0) {
		librouter_qos_release_config(intf_cfg);
		return ret;
	}

	if (librouter_qos_get_policymap(intf_cfg->pname, &pmap) <= 0) {
		librouter_qos_release_config(intf_cfg);
		return ret;
	}

	if (!pmap->n_mark) {
		librouter_qos_free_policymap(pmap);
		librouter_qos_release_config(intf_cfg);
		return ret;
	}

	librouter_qos_free_policymap(pmap);
	librouter_qos_release_config(intf_cfg);

	sprintf(buf, "/bin/tc -s class ls dev %s 2> /dev/null", dev_name);

	if (!(f = popen(buf, "r")))
		return ret;

	for (; out == 0;) {

		fgets(buf, 256, f);
		buf[255] = 0;

		if (feof(f))
			break;

		if (librouter_parse_args_din(buf, &argl) >= 3) {

			if ((strcasecmp(argl[0], "class") == 0) && (strcasecmp(argl[1], "hfsc") == 0)) {

				if ((p = strchr(argl[2], ':'))) {

					*p = 0;
					n1 = atoi(p + 1);

					librouter_destroy_args_din(&argl);

					if ((n1 - 10) == mark) {
						/* Skip Sent 0 bytes 0 pkts (dropped 0, overlimits 0)*/
						fgets(buf, 256, f);
						fgets(buf, 256, f);

						buf[255] = 0;

						if (feof(f))
							break;

						if ((p = strstr(buf, "rate"))) {
							p += 5;
							/* In Kbps */
							ret = (atoi(p) * 8) / 1024;
						} else {
							ret = 0;
						}
					}
				}
			}
		}

		librouter_destroy_args_din(&argl);
	}

	pclose(f);

	return ret;
}

