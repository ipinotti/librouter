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
/* FIXME This function seems pretty useful for being exported */
static int is_ipv6_addr_empty(struct in6_addr *a)
{
	char *buf = (char *) a;
	int size = sizeof(struct in6_addr);

	return (buf[0] == 0) && (!memcmp(buf, buf + 1, size - 1));
}

static int _check_for_valid_config(struct vrrp_group *g)
{
	int ret = 0;

	if (g->ifname[0] != 0 && g->ip[0].s_addr != 0)
		ret = 1;
#if 0 /* VRRP deamon is not yet very stable only with IPv6 Virtual Addresses */
	else if (g->ifname[0] != 0 && !is_ipv6_addr_empty(&g->ip6[0]))
		ret = 1;
#endif

	return ret;
}

static int _is_ip_owner(struct vrrp_group *g)
{
	char *dev = g->ifname;
	char ipstr[16], msk[16];
	struct in_addr ip;

	librouter_ip_interface_get_ip_addr(dev, ipstr, msk);
	inet_pton(AF_INET, ipstr, &ip);

	if (ip.s_addr == g->ip[0].s_addr)
		return 1;

	return 0;
}

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

	while (_check_for_valid_config(groups)) {
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

		fprintf(f, "\tpriority %d%s\n",
		        		_is_ip_owner(groups) ? VRRP_PRIO_OWNER :
		        		(groups->priority != 0) ? groups->priority :
		        		VRRP_PRIO_DFL);

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

		if (groups->ip[0].s_addr != 0 || !is_ipv6_addr_empty(&groups->ip6[0])) {
			fprintf(f, "\tvirtual_ipaddress {\n");
			for (i = 0; groups->ip[i].s_addr != 0 && i < MAX_VRRP_IP; i++) {
				fprintf(f, "\t\t%s\n", inet_ntoa(groups->ip[i]));
			}
			for (i = 0; is_ipv6_addr_empty(&groups->ip6[i]) == 0 && i < MAX_VRRP_IP; i++) {
				char ip6[64];
				inet_ntop(AF_INET6, &groups->ip6[i], ip6, sizeof(ip6));
				fprintf(f, "\t\t%s\n", ip6);
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

static void vrrp_check_daemon(struct vrrp_group *groups)
{
	int found = 0;

	while (groups->ifname[0] != 0) {
		if (_check_for_valid_config(groups)) {
			found = 1;
			break;
		}
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
	struct vrrp_group *groups, *g;

	if ((groups = vrrp_read_bin()) != NULL) {
		g = vrrp_find_group(groups, dev, group);
		g->authentication_type = type;
		if (password != NULL)
			strncpy(g->authentication_password, password, MAX_VRRP_AUTHENTICATION);
		else
			g->authentication_password[0] = 0;
		vrrp_write_bin(groups);
		if (g->ip[0].s_addr != 0) {
			vrrp_write_conf(groups);
			vrrp_check_daemon(groups);
		}
	}
	free(groups);
}

void librouter_vrrp_option_description(char *dev, int group, char *description)
{
	struct vrrp_group *groups, *g;

	if ((groups = vrrp_read_bin()) != NULL) {
		g = vrrp_find_group(groups, dev, group);
		if (description != NULL)
			strncpy(g->description, description, MAX_VRRP_DESCRIPTION);
		else
			g->description[0] = 0;
		vrrp_write_bin(groups);
	}
	free(groups);
}
#define OPTION_VRRP_IPV6
#ifdef OPTION_VRRP_IPV6
static void _vrrp_secondary_ipv6_addr(struct vrrp_group *g, int add, char *ip_string)
{
	struct in6_addr ip;
	int i, match = -1;

	inet_pton(AF_INET6, ip_string, &ip);

	/* Check if wanted IP already exists in configuration */
	for (i = 0; i < MAX_VRRP_IP6; i++) {
		if (memcmp(&g->ip6[i], &ip, sizeof(struct in6_addr)) == 0)
			match = i;
	}

	/* Check for empty slots */
	for (i = 0; i < MAX_VRRP_IP6; i++) {
		if (is_ipv6_addr_empty(&g->ip6[i]))
			break;
	}

	if (add) {
		if (i == MAX_VRRP_IP6 || match != -1)
			return;
		g->ip6[i] = ip;
	} else {
		if (match == -1)
			return;
		if (match < MAX_VRRP_IP6 - 1)
			memmove(&g->ip6[match], &g->ip6[match + 1],
			                ((MAX_VRRP_IP6 - 1) - match) * sizeof(struct in6_addr));
		memset(&g->ip6[MAX_VRRP_IP6 - 1], 0, sizeof(struct in6_addr)); /* clear top! */
	}
}

int librouter_vrrp_option_ipv6(char *dev, int group, int add, char *ip_string, int secondary)
{
	struct vrrp_group *groups, *g;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return 0;

	g = vrrp_find_group(groups, dev, group);
	if (g == NULL) {
		free(groups);
		return 0;
	}

	if (secondary)
		_vrrp_secondary_ipv6_addr(g, add, ip_string);
	else {
		if (ip_string != NULL)
			inet_pton(AF_INET6, ip_string, &g->ip6[0]); /* primary */
		else
			memset(&g->ip6[0], 0, sizeof(struct in6_addr) * MAX_VRRP_IP6); /* clear all! */
	}

	vrrp_write_bin(groups);
	vrrp_write_conf(groups);
	vrrp_check_daemon(groups);

	free(groups);

	return 0;
}
#endif /* OPTION_VRRP_IPV6 */

int librouter_vrrp_option_ip(char *dev, int group, int add, char *ip_string, int secondary)
{
	struct vrrp_group *groups, *g;
	struct in_addr ip;
	int i, match = -1;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return 0;

	g = vrrp_find_group(groups, dev, group);
	if (g == NULL) {
		free(groups);
		return 0;
	}

	if (secondary) {
		inet_aton(ip_string, &ip);
		for (i = 0; i < MAX_VRRP_IP; i++) {
			if (g->ip[i].s_addr == ip.s_addr)
				match = i;
			if (g->ip[i].s_addr == 0)
				break;
		}
		if ((add && (i == MAX_VRRP_IP || match != -1)) || (!add && (match == -1))) {
			free(groups);
			return -1;
		}
		if (add) {
			g->ip[i] = ip;
		} else {
			if (match < MAX_VRRP_IP - 1)
				memmove(&g->ip[match], &g->ip[match + 1],
				                ((MAX_VRRP_IP - 1) - match) * sizeof(struct in_addr));
			g->ip[MAX_VRRP_IP - 1].s_addr = 0; /* clear top! */
		}
	} else {
		if (ip_string != NULL)
			inet_aton(ip_string, &g->ip[0]); /* primary */
		else
			memset(&g->ip[0], 0, sizeof(struct in_addr) * MAX_VRRP_IP); /* clear all! */
	}

	vrrp_write_bin(groups);
	vrrp_write_conf(groups);
	vrrp_check_daemon(groups);

	free(groups);

	return 0;
}

void librouter_vrrp_option_preempt(char *dev, int group, int preempt, int delay)
{
	struct vrrp_group *groups, *g;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	g = vrrp_find_group(groups, dev, group);
	if (g) {
		g->preempt = preempt;
		g->preempt_delay = delay;
	}

	vrrp_write_bin(groups);

	if (g->ip[0].s_addr != 0) {
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_option_priority(char *dev, int group, int priority)
{
	struct vrrp_group *groups, *g;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	g = vrrp_find_group(groups, dev, group);
	if (g)
		g->priority = priority;

	vrrp_write_bin(groups);

	if (g->ip[0].s_addr != 0) {
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_option_advertise_delay(char *dev, int group, int advertise_delay)
{
	struct vrrp_group *groups, *g;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	g = vrrp_find_group(groups, dev, group);
	if (g)
		g->advertise_delay = advertise_delay;

	vrrp_write_bin(groups);

	if (g->ip[0].s_addr != 0) {
		vrrp_write_conf(groups);
		vrrp_check_daemon(groups);
	}

	free(groups);
}

void librouter_vrrp_option_track_interface(char *dev, int group, char *track_iface)
{
	struct vrrp_group *groups, *g;

	groups = vrrp_read_bin();
	if (groups == NULL)
		return;

	g = vrrp_find_group(groups, dev, group);
	if (g) {
		if (track_iface)
			strncpy(g->track_interface, track_iface, sizeof(g->track_interface));
		else
			memset(g->track_interface, 0, sizeof(g->track_interface));
	}

	vrrp_write_bin(groups);

	if (g->ip[0].s_addr != 0) {
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

			for (i = 0; is_ipv6_addr_empty(&g->ip6[i]) == 0 && i < MAX_VRRP_IP; i++) {
				char buf[64];
				inet_ntop(AF_INET6, &g->ip6[i], buf, sizeof(buf));
				fprintf(out, " vrrp %d ipv6 %s%s\n", g->group, buf, i ? " secondary" : "");
			}

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

			if (g->track_interface[0]) {
				fprintf(out, " vrrp %d track-interface %s\n", g->group,
				                librouter_device_linux_to_cli(g->track_interface, 0));
			}
		}
		g++;
	}

	free(groups);
}

static void _write_group_config(FILE *f, struct vrrp_group *g)
{
	int i;
	char buf[64];

	fprintf(f, "\t%s - Group %u\n", librouter_device_linux_to_cli(g->ifname, 1), g->group);

	/* Virtual IPv4 Addresses */
	if (g->ip[0].s_addr != 0)
		fprintf(f, "\t\tVirtual IP address is %s\n", inet_ntoa(g->ip[0]));

	for (i = 1; g->ip[i].s_addr != 0 && i < MAX_VRRP_IP; i++)
		fprintf(f, "\t\t\tSecundary Virtual IP address is %s\n", inet_ntoa(g->ip[i]));

	/* Virtual IPv6 Addresses */
	if (!is_ipv6_addr_empty(&g->ip6[0])) {
		inet_ntop(AF_INET6, &g->ip6[0], buf, sizeof(buf));
		fprintf(f, "\t\tVirtual IPv6 address is %s\n", buf);
	}

	for (i = 1; (is_ipv6_addr_empty(&g->ip6[i]) == 0) && (i < MAX_VRRP_IP6); i++) {
		inet_ntop(AF_INET6, &g->ip6[i], buf, sizeof(buf));
		fprintf(f, "\t\t\tSecundary Virtual IP address is %s\n", buf);
	}

	/* Virtual MAC : 0000.5e00.01xx, where xx equals the VRID */
	fprintf(f, "\t\tVirtual MAC address is 0000.5e00.01%02x\n", g->group);
	fprintf(f, "\t\tAdvertisement interval is %u.000 sec\n",
			(g->advertise_delay) ? g->advertise_delay : 1);

	fprintf(f, "\t\tPreemption %s", g->preempt ? "enabled" : "disabled");

	/* Verifica se ha delay na preempcao*/
	if (g->preempt_delay)
		fprintf(f, ", delay min %u secs", g->preempt_delay);

	fprintf(f, "\n");

	if (_is_ip_owner(g))
		fprintf(f, "\t\tPriority is %d (IP Address Owner)\n", VRRP_PRIO_OWNER);
	else if (g->priority == 0)
		fprintf(f, "\t\tPriority is %d (Default backup priority)\n", VRRP_PRIO_DFL);
	else
		fprintf(f, "\t\tPriority is %d \n", g->priority);


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

