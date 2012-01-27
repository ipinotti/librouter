#if 0
-n (+ de um grupo na mesma interface)
-i ifname
-v vrid (group)
-s preemption
-p prio
-d delay advertisement (s)
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/sockios.h>

#include "options.h"
#include "error.h"
#include "exec.h"
#include "defines.h"
#include "dev.h"
#include "ip.h"
#include "vrrp.h"

#ifdef OPTION_VRRP

void kick_vrrp(void)
{
	FILE *f;
	char buf[32];

	f = fopen(VRRPD_PID_FILE, "r");
	if (f) {
		fgets(buf, 32, f);
		kill((pid_t) atoi(buf), SIGHUP);
		fclose(f);
	}
}

static struct vrrp_group *vrrp_read_bin(void)
{
	int size;
	FILE *f;
	struct vrrp_group *groups;

	if ((f = fopen(VRRP_BIN_FILE, "rb")) != NULL) {
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if ((groups = (struct vrrp_group *) malloc(size + 2 * sizeof(struct vrrp_group))) != NULL) {
			memset(groups, 0, size + 2 * sizeof(struct vrrp_group));
			fread(groups, size, 1, f);
			fclose(f);
			return groups;
		}
		fclose(f);
		return NULL;
	}
	if ((groups = (struct vrrp_group *) malloc(2 * sizeof(struct vrrp_group))) != NULL) {
		memset(groups, 0, 2 * sizeof(struct vrrp_group));
		return groups;
	}
	return NULL;
}

static void vrrp_write_bin(struct vrrp_group *groups)
{
	FILE *f;

	if ((f = fopen(VRRP_BIN_FILE, "wb")) != NULL) {
		while (groups->ifname[0] != 0) {
			fwrite(groups, sizeof(struct vrrp_group), 1, f);
			groups++;
		}
		fclose(f);
	}
}

static void vrrp_write_conf(struct vrrp_group *groups)
{
	int i;
	FILE *f;

	if ((f = fopen(VRRP_CONF_FILE, "w")) == NULL) {
		printf("Could not open VRRP configuration file\n");
		return;
	}

	while (groups->ifname[0] != 0 && groups->ip[0].s_addr != 0) {
		fprintf(f, "vrrp_instance %s_%d {\n", groups->ifname, groups->group);
		fprintf(f, "\tinterface %s\n", groups->ifname);
		fprintf(f, "\tvirtual_router_id %d\n", groups->group);

		if (groups->authentication_type != VRRP_AUTHENTICATION_NONE) {
			fprintf(f, "\tauthentication {\n");
			fprintf(f, "\t\tauth_type %s\n",
			                groups->authentication_type == VRRP_AUTHENTICATION_AH ? "AH" : "PASS");
			fprintf(f, "\t\tauth_pass %s\n", groups->authentication_password);
			fprintf(f, "\t}\n");
		}

		if (groups->priority)
			fprintf(f, "\tpriority %d\n", groups->priority);

		if (groups->advertise_delay)
			fprintf(f, "\tadvert_int %d\n", groups->advertise_delay);

		if (!groups->preempt)
			fprintf(f, "\tnopreempt\n");
		else if (groups->preempt_delay)
			fprintf(f, "\tpreempt_delay %d\n", groups->preempt_delay);

		if (groups->track_interface[0] != 0) {
			fprintf(f, "\ttrack_interface {\n");
			fprintf(f, "\t\t%s\n", groups->track_interface);
			fprintf(f, "\t}\n");
		}

		if (groups->ip[0].s_addr != 0) {
			fprintf(f, "\tvirtual_ipaddress {\n");
			for (i = 0; groups->ip[i].s_addr != 0 && i < MAX_VRRP_IP; i++) {
				fprintf(f, "\t\t%s\n", inet_ntoa(groups->ip[i]));
			}
			fprintf(f, "\t}\n");
		}
		fprintf(f, "}\n");
		groups++;
	}
	fclose(f);
}

static struct vrrp_group *vrrp_find_group(struct vrrp_group *groups, char *dev, int group)
{
	while (groups->ifname[0] != 0) {
		if (strcmp(groups->ifname, dev) == 0 && groups->group == group)
			return groups;
		groups++;
	}
	strncpy(groups->ifname, dev, MAX_VRRP_IFNAME);
	groups->group = group;
	groups->preempt = 1; /* default! */

	return groups; /* empty position! initialized! */
}

static int vrrp_delete_group(struct vrrp_group *groups, char *dev, int group)
{
	struct vrrp_group *match = NULL;

	while (groups->ifname[0] != 0) {
		if (strcmp(groups->ifname, dev) == 0 && groups->group == group)
			match = groups;
		groups++;
	}
	if (match != NULL) {
		memmove(match, match + 1, (groups - match) * sizeof(struct vrrp_group));
		return 0;
	}
	return -1;
}

#if 0
static struct vrrp_group *vrrp_add_group(struct vrrp_group *groups, struct vrrp_group *new)
{
	int count;
	struct vrrp_group *groups=groups;

	while (groups->ifname[0] != 0)
	groups++;
	count=(groups-groups);
	if ((groups=realloc(groups, (count+2)*sizeof(struct vrrp_group))) != NULL)
	{
		memcpy(groups+count, new, sizeof(struct vrrp_group));
		memset(groups+count+1, 0, sizeof(struct vrrp_group));
		return groups;
	}
	return NULL;
}
#endif

void vrrp_check_daemon(struct vrrp_group *groups)
{
	int found = 0;

	while (groups->ifname[0] != 0) {
		if (groups->ip[0].s_addr != 0)
			found = 1;
		groups++;
	}
	if (found == 0 && librouter_exec_check_daemon(VRRP_DAEMON)) {
		librouter_kill_daemon(VRRP_DAEMON);
		return;
	}
	if (found == 1) {
		if (librouter_exec_check_daemon(VRRP_DAEMON))
			kick_vrrp();
		else
			librouter_exec_daemon(VRRP_DAEMON);
	}
}

/**
 *
 * BEGIN OF EXPORTED FUNCTIONS
 *
 */
void librouter_vrrp_no_group(char *dev, int group)
{
	struct vrrp_group *groups = vrrp_read_bin();

	if (groups == NULL)
		return;

	if (vrrp_delete_group(groups, dev, group) == 0) {
		vrrp_write_bin(groups);
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_option_authenticate(char *dev, int group, int type, char *password)
{
	struct vrrp_group *groups, *find_group;

	if ((groups = vrrp_read_bin()) != NULL) {
		find_group = vrrp_find_group(groups, dev, group);
		find_group->authentication_type = type;
		if (password != NULL)
			strncpy(find_group->authentication_password, password, MAX_VRRP_AUTHENTICATION);
		else
			find_group->authentication_password[0] = 0;
		vrrp_write_bin(groups);
		if (find_group->ip[0].s_addr != 0) {
			vrrp_write_conf(groups);
			vrrp_check_daemon(groups);
		}
	}
	free(groups);
}

void librouter_vrrp_option_description(char *dev, int group, char *description)
{
	struct vrrp_group *groups, *find_group;

	if ((groups = vrrp_read_bin()) != NULL) {
		find_group = vrrp_find_group(groups, dev, group);
		if (description != NULL)
			strncpy(find_group->description, description, MAX_VRRP_DESCRIPTION);
		else
			find_group->description[0] = 0;
		vrrp_write_bin(groups);
	}
	free(groups);
}

int librouter_vrrp_option_ip(char *dev, int group, int add, char *ip_string, int secondary)
{
	struct vrrp_group *groups, *find_group;
	struct in_addr ip;
	int i, match = -1;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return 0;

	find_group = vrrp_find_group(groups, dev, group);
	if (find_group == NULL) {
		free(groups);
		return 0;
	}

	if (secondary) {
		inet_aton(ip_string, &ip);
		for (i = 0; i < MAX_VRRP_IP; i++) {
			if (find_group->ip[i].s_addr == ip.s_addr)
				match = i;
			if (find_group->ip[i].s_addr == 0)
				break;
		}
		if ((add && (i == MAX_VRRP_IP || match != -1)) || (!add && (match == -1))) {
			free(groups);
			return -1;
		}
		if (add) {
			find_group->ip[i] = ip;
		} else {
			if (match < MAX_VRRP_IP - 1)
				memmove(&find_group->ip[match], &find_group->ip[match + 1],
				                ((MAX_VRRP_IP - 1) - match) * sizeof(struct in_addr));
			find_group->ip[MAX_VRRP_IP - 1].s_addr = 0; /* clear top! */
		}
	} else {
		if (ip_string != NULL)
			inet_aton(ip_string, &find_group->ip[0]); /* primary */
		else
			memset(&find_group->ip[0], 0, sizeof(struct in_addr) * MAX_VRRP_IP); /* clear all! */
	}

	vrrp_write_bin(groups);
	vrrp_write_conf(groups);
	vrrp_check_daemon(groups);

	free(groups);

	return 0;
}

void librouter_vrrp_option_preempt(char *dev, int group, int preempt, int delay)
{
	struct vrrp_group *groups, *find_group;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	find_group = vrrp_find_group(groups, dev, group);
	if (find_group) {
		find_group->preempt = preempt;
		find_group->preempt_delay = delay;
	}

	vrrp_write_bin(groups);

	if (find_group->ip[0].s_addr != 0) {
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_option_priority(char *dev, int group, int priority)
{
	struct vrrp_group *groups, *find_group;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	find_group = vrrp_find_group(groups, dev, group);
	if (find_group)
		find_group->priority = priority;

	vrrp_write_bin(groups);

	if (find_group->ip[0].s_addr != 0) {
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_option_advertise_delay(char *dev, int group, int advertise_delay)
{
	struct vrrp_group *groups, *find_group;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	find_group = vrrp_find_group(groups, dev, group);
	if (find_group)
		find_group->advertise_delay = advertise_delay;

	vrrp_write_bin(groups);

	if (find_group->ip[0].s_addr != 0) {
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_option_track_interface(char *dev, int group, char *track_iface)
{
	struct vrrp_group *groups, *find_group;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	find_group = vrrp_find_group(groups, dev, group);
	if (find_group)
		strncpy(track_iface, find_group->track_interface, sizeof(find_group->track_interface));

	vrrp_write_bin(groups);

	if (find_group->ip[0].s_addr != 0) {
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_dump_interface(FILE *out, char *dev)
{
	int i;
	struct vrrp_group *groups, *g;

	groups = vrrp_read_bin();

	if (groups == NULL)
		return;

	g = groups;
	while (g->ifname[0] != 0) {
		if (strcmp(g->ifname, dev) == 0) {
			if (g->authentication_type != VRRP_AUTHENTICATION_NONE)
				fprintf(
				                out,
				                " vrrp %d authentication %s %s\n",
				                g->group,
				                g->authentication_type == VRRP_AUTHENTICATION_AH ? "ah" : "text",
				                g->authentication_password);
			if (g->description[0])
				fprintf(out, " vrrp %d description %s\n", g->group, g->description);
			for (i = 0; g->ip[i].s_addr != 0 && i < MAX_VRRP_IP; i++)
				fprintf(out, " vrrp %d ip %s%s\n", g->group, inet_ntoa(g->ip[i]),
				                i ? " secondary" : "");
			if (g->preempt) {
				fprintf(out, " vrrp %d preempt", g->group);
				if (g->preempt_delay)
					fprintf(out, "delay minimum %d\n", g->preempt_delay);
				else
					fprintf(out, "\n");
			}
			if (g->priority)
				fprintf(out, " vrrp %d priority %d\n", g->group, g->priority);
			if (g->advertise_delay)
				fprintf(out, " vrrp %d timers advertise %d\n", g->group, g->advertise_delay);
		}
		g++;
	}

	free(groups);
}

static void _write_group_config(FILE *f, struct vrrp_group *g)
{
	int i;

	fprintf(f, "\t%s - Group %u\n", librouter_device_linux_to_cli(g->ifname, 1), g->group);
	fprintf(f, "\t\tVirtual IP address is %s\n", inet_ntoa(g->ip[0]));

	/* Se existir IP secundario*/
	for (i = 1; g->ip[i].s_addr != 0 && i < MAX_VRRP_IP; i++)
		fprintf(f, "\t\t\tSecundary Virtual IP address is %s\n", inet_ntoa(g->ip[i]));

	/*MAC virtual sempre eh 0000.5e00.01xx, no qual xx = ID do grupo*/
	fprintf(f, "\t\tVirtual MAC address is 0000.5e00.01%02x\n", g->group);
	fprintf(f, "\t\tAdvertisement interval is %u.000 sec\n", g->advertise_delay);

	fprintf(f, "\t\tPreemption %s", g->preempt ? "enabled" : "disabled");

	/* Verifica se ha delay na preempcao*/
	if (g->preempt_delay)
		fprintf(f, ", delay min %u secs", g->preempt_delay);

	fprintf(f, "\n");

	fprintf(f, "\t\tPriority is %u\n", g->priority);

	/* Verifica se ha autenticacao */
	if (g->authentication_type != VRRP_AUTHENTICATION_NONE)
		fprintf(f, "\t\tAuthentication %s \"%s\"\n",
		                g->authentication_type == VRRP_AUTHENTICATION_AH ? "AH" : "text",
		                g->authentication_password);
}

static void _write_group_status(FILE *f, int vrid)
{
	int fd, n;
	char filename[64];
	struct vrrp_status_t v;
	char prio[16];
	float adv, down;

	sprintf(filename, VRRP_STATUS_FILE, vrid);
	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return;

	n = read(fd, (void *) &v, sizeof(v));

	if (n != sizeof(v))
		return;

	fprintf(
	                f,
	                "\t\tState is %s\n",
	                v.state == VRRP_STATE_INIT ? "Init" : v.state == VRRP_STATE_MAST ? "Master" : "Backup");

	sprintf(prio, "%d", v.master_prio);
	fprintf(f, "\t\tMaster Router is %s%s, priority is %s\n",
	                (v.state == VRRP_STATE_INIT ? "unknown" : v.ipaddr),
	                (v.state == VRRP_STATE_MAST ? "(local)" : ""),
	                (v.state == VRRP_STATE_INIT ? "unknown" : v.master_prio == 0 ? "unknown" : prio));

	fprintf(f, "\t\tMaster Advertisement interval is ");
	if (v.state == VRRP_STATE_INIT) {
		fprintf(f, "unknown\n");
	} else {
		adv = ((float) v.adver_int) / 1000000;
		fprintf(f, "%.3f sec\n", adv);
	}

	fprintf(f, "\t\tMaster Down interval is ");
	if (v.state == VRRP_STATE_INIT) {
		fprintf(f, "unknown\n");
	} else {
		down = ((float) v.down_interval) / 1000000;
		fprintf(f, "%.3f sec\n\n", down);
	}

	close(fd);
}

void librouter_vrrp_dump_status(void)
{
	struct vrrp_group *groups, *g;
	FILE *fshow;

	/* Read VRRP configuration */
	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	/* Open file for dumping status */
	fshow = fopen(VRRP_SHOW_FILE, "w+");
	if (fshow == NULL) {
		free(groups);
		return;
	}

	g = groups;
	while (g->ifname[0] != 0) {
		_write_group_config(fshow, g);
		_write_group_status(fshow, g->group);
		g++;
	}

	free(groups);
	fclose(fshow);
}
#endif

