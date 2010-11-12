#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <asm/param.h>

#include "error.h"
#include "device.h"
#include "defines.h"
#include "libbridge/libbridge.h"

int librouter_br_initbr(void)
{
	int err;

	err = br_init();
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_exists(char *brname)
{
	struct bridge *br;

	if (br_refresh())
		return 0;

	br = br_find_bridge(brname);

	return (br != NULL);
}

static struct bridge *_find_bridge(char *brname)
{
	struct bridge *br;

	if (br_refresh())
		return NULL;

	br = br_find_bridge(brname);
	if (br == NULL)
		librouter_pr_error(0, "bridge %s doesn't exist", brname);
	return br;
}

int librouter_br_addbr(char *brname)
{
	int err;

	if ((err = br_add_bridge(brname)) == 0)
		return 0;

	switch (err) {
	case EEXIST:
		librouter_pr_error(0, "device %s already exists; can't create "
			"bridge with the same name", brname);
		break;

	default:
		librouter_pr_error(1, "br_add_bridge");
		break;
	}
	return (-err);
}

int librouter_br_delbr(char *brname)
{
	int err;

	if ((err = br_del_bridge(brname)) == 0)
		return 0;

	switch (err) {
	case ENXIO:
		librouter_pr_error(0, "bridge %s doesn't exist; can't delete it", brname);
		break;

	case EBUSY:
		librouter_pr_error(0, "bridge %s is still up; can't delete it", brname);
		break;

	default:
		librouter_pr_error(1, "br_del_bridge");
		break;
	}
	return (-err);
}

int librouter_br_addif(char *brname, char *ifname)
{
	int err;
	int ifindex;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		librouter_pr_error(0, "interface %s does not exist!", ifname);
		return (-1);
	}

	if ((err = br_add_interface(br, ifindex)) == 0)
		return 0;

	switch (err) {
	case EBUSY:
		librouter_pr_error(0, "device %s is already a member of a bridge; "
			"can't enslave it to bridge %s", ifname, br->ifname);
		break;

	default:
		librouter_pr_error(1, "br_add_interface");
		break;
	}
	return (-err);
}

int librouter_br_delif(char *brname, char *ifname)
{
	int err;
	int ifindex;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		librouter_pr_error(0, "interface %s does not exist!", ifname);
		return (-1);
	}

	if ((err = br_del_interface(br, ifindex)) == 0)
		return 0;

	switch (err) {
	case EINVAL:
		librouter_pr_error(0, "device %s is not a slave of %s", ifname, br->ifname);
		break;

	default:
		librouter_pr_error(1, "br_del_interface");
		break;
	}
	return (-err);
}

int librouter_br_checkif(char *brname, char *ifname)
{
	struct port *p;
	struct bridge *br;
	char buf[IFNAMSIZ];

	if (br_refresh())
		return (0);

	br = br_find_bridge(brname);
	if (br == NULL)
		return (0);

	p = br->firstport;

	while (p != NULL) {
		if (strcmp(ifname, if_indextoname(p->ifindex, buf)) == 0)
			return 1;
		p = p->next;
	}
	return 0;
}

int librouter_br_hasifs(char *brname)
{
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return 0;
	if (br->firstport == NULL)
		return 0;
	return 1;
}

int librouter_br_setageing(char *brname, int sec)
{
	int err;
	struct timeval tv;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	tv.tv_sec = sec;
	tv.tv_usec = 0;
	err = br_set_ageing_time(br, &tv);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_getageing(char *brname)
{
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);
	return br->info.ageing_time.tv_sec;
}

int librouter_br_setbridgeprio(char *brname, int prio)
{
	int err;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	err = br_set_bridge_priority(br, prio);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_getbridgeprio(char *brname)
{
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);
	return ((br->info.bridge_id.prio[0] << 8) + br->info.bridge_id.prio[1]);
}

int librouter_br_setfd(char *brname, int sec)
{
	int err;
	struct timeval tv;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	tv.tv_sec = sec;
	tv.tv_usec = 0;

	err = br_set_bridge_forward_delay(br, &tv);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_getfd(char *brname)
{
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);
	return br->info.bridge_forward_delay.tv_sec;
}

int librouter_br_setgcint(char *brname, int sec)
{
	int err;
	struct timeval tv;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	tv.tv_sec = sec;
	tv.tv_usec = 0;

	err = br_set_gc_interval(br, &tv);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_sethello(char *brname, int sec)
{
	int err;
	struct timeval tv;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	tv.tv_sec = sec;
	tv.tv_usec = 0;

	err = br_set_bridge_hello_time(br, &tv);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_gethello(char *brname)
{
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);
	return br->info.bridge_hello_time.tv_sec;
}

int librouter_br_setmaxage(char *brname, int sec)
{
	int err;
	struct timeval tv;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	tv.tv_sec = sec;
	tv.tv_usec = 0;

	err = br_set_bridge_max_age(br, &tv);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_getmaxage(char *brname)
{
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);
	return br->info.bridge_max_age.tv_sec;
}

int librouter_br_setpathcost(char *brname, char *portname, int cost)
{
	struct port *p;
	int err;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	if ((p = br_find_port(br, portname)) == NULL) {
		librouter_pr_error(0, "can't find port %s in bridge %s", portname, br->ifname);
		return (-1);
	}

	err = br_set_path_cost(p, cost);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_setportprio(char *brname, char *portname, int cost)
{
	struct port *p;
	int err;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	if ((p = br_find_port(br, portname)) == NULL) {
		librouter_pr_error(0, "can't find port %s in bridge %s", portname, br->ifname);
		return (-1);
	}

	err = br_set_port_priority(p, cost);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_set_stp(char *brname, int on_off)
{
	int err;
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);

	err = br_set_stp_state(br, on_off);
	if (err)
		return (-err);
	else
		return 0;
}

int librouter_br_get_stp(char *brname)
{
	struct bridge *br = _find_bridge(brname);
	if (br == NULL)
		return (-1);
	return br->info.stp_enabled;
}

static void librouter_br_dump_bridge_id(unsigned char *x, FILE *out)
{
	fprintf(out, "%.2x%.2x.%.2x%.2x%.2x%.2x%.2x%.2x", x[0], x[1], x[2], x[3], x[4], x[5], x[6],
	                x[7]);
}

static void librouter_br_show_timer(struct timeval *tv, FILE *out)
{
	fprintf(out, "%4i", (int) tv->tv_sec);
}

static void librouter_br_dump_port_info(struct port *p, FILE *out)
{
	char ifname[IFNAMSIZ];
	struct port_info *pi;
	char *dev;

	pi = &p->info;
	if_indextoname(p->ifindex, ifname);
	dev = librouter_device_convert_os(ifname, 1);
	if (dev == NULL)
		return;

	fprintf(out, "%s (%i)\n", dev, p->index);
	fprintf(out, " port id\t\t%.4x\t\t\t", pi->port_id);
	fprintf(out, "state\t\t\t%s\n", br_get_state_name(pi->state));
	fprintf(out, " designated root\t");
	librouter_br_dump_bridge_id((unsigned char *) &pi->designated_root, out);
	fprintf(out, "\tpath cost\t\t%4i\n", pi->path_cost);

	fprintf(out, " designated bridge\t");
	librouter_br_dump_bridge_id((unsigned char *) &pi->designated_bridge, out);
	fprintf(out, "\tmessage age timer\t");
	librouter_br_show_timer(&pi->message_age_timer_value, out);
	fprintf(out, "\n designated port\t%.4x", pi->designated_port);
	fprintf(out, "\t\t\tforward delay timer\t");
	librouter_br_show_timer(&pi->forward_delay_timer_value, out);
	fprintf(out, "\n designated cost\t%4i", pi->designated_cost);
	fprintf(out, "\t\t\thold timer\t\t");
	librouter_br_show_timer(&pi->hold_timer_value, out);
	fprintf(out, "\n flags\t\t\t");
	if (pi->config_pending)
		fprintf(out, "CONFIG_PENDING ");
	if (pi->top_change_ack)
		fprintf(out, "TOPOLOGY_CHANGE_ACK ");
	fprintf(out, "\n");
	fprintf(out, "\n");
}

int librouter_br_dump_info(char *brname, FILE *out)
{
	struct bridge *br = _find_bridge(brname);
	struct bridge_info *bri;
	struct port *p;

	if (br == NULL)
		return (-1);
	bri = &br->info;

	fprintf(out, "%s\n", br->ifname);
	if (!bri->stp_enabled) {
		fprintf(out, " STP is disabled for this interface\n");
		return 0;
	}

	fprintf(out, " bridge id\t\t");
	librouter_br_dump_bridge_id((unsigned char *) &bri->bridge_id, out);
	fprintf(out, "\n designated root\t");
	librouter_br_dump_bridge_id((unsigned char *) &bri->designated_root, out);
	fprintf(out, "\n root port\t\t%4i\t\t\t", bri->root_port);
	fprintf(out, "path cost\t\t%4i\n", bri->root_path_cost);
	fprintf(out, " max age\t\t");
	librouter_br_show_timer(&bri->max_age, out);
	fprintf(out, "\t\t\tbridge max age\t\t");
	librouter_br_show_timer(&bri->bridge_max_age, out);
	fprintf(out, "\n hello time\t\t");
	librouter_br_show_timer(&bri->hello_time, out);
	fprintf(out, "\t\t\tbridge hello time\t");
	librouter_br_show_timer(&bri->bridge_hello_time, out);
	fprintf(out, "\n forward delay\t\t");
	librouter_br_show_timer(&bri->forward_delay, out);
	fprintf(out, "\t\t\tbridge forward delay\t");
	librouter_br_show_timer(&bri->bridge_forward_delay, out);
	fprintf(out, "\n ageing time\t\t");
	librouter_br_show_timer(&bri->ageing_time, out);
	fprintf(out, "\t\t\tgc interval\t\t");
	librouter_br_show_timer(&bri->gc_interval, out);
	fprintf(out, "\n hello timer\t\t");
	librouter_br_show_timer(&bri->hello_timer_value, out);
	fprintf(out, "\t\t\ttcn timer\t\t");
	librouter_br_show_timer(&bri->tcn_timer_value, out);
	fprintf(out, "\n topology change timer\t");
	librouter_br_show_timer(&bri->topology_change_timer_value, out);
	fprintf(out, "\t\t\tgc timer\t\t");
	librouter_br_show_timer(&bri->gc_timer_value, out);
	fprintf(out, "\n flags\t\t\t");
	if (bri->topology_change)
		fprintf(out, "TOPOLOGY_CHANGE ");
	if (bri->topology_change_detected)
		fprintf(out, "TOPOLOGY_CHANGE_DETECTED ");
	fprintf(out, "\n");
	fprintf(out, "\n");
	fprintf(out, "\n");

	p = br->firstport;
	while (p != NULL) {
		librouter_br_dump_port_info(p, out);
		p = p->next;
	}
	return 0;
}

void librouter_br_dump_bridge(FILE *out)
{
	int i, printed_something = 0;
	char brname[32];

	for (i = 1; i <= MAX_BRIDGE; i++) {
		sprintf(brname, "%s%d", BRIDGE_NAME, i);
		if (!librouter_br_exists(brname))
			continue;
		printed_something = 1;
		fprintf(out, "bridge %d protocol ieee\n", i);
		fprintf(out, "bridge %d aging-time %d\n", i, librouter_br_getageing(brname));
		fprintf(out, "bridge %d forward-time %d\n", i, librouter_br_getfd(brname));
		fprintf(out, "bridge %d hello-time %d\n", i, librouter_br_gethello(brname));
		fprintf(out, "bridge %d max-age %d\n", i, librouter_br_getmaxage(brname));
		fprintf(out, "bridge %d priority %d\n", i, librouter_br_getbridgeprio(brname));
		if (!librouter_br_get_stp(brname))
			fprintf(out, "bridge %d spanning-disabled\n", i);
	}
	if (printed_something)
		fprintf(out, "!\n");
}

