/*
 * debug.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <syslog.h>

#include "options.h"
#include "defines.h"
#include "dhcp.h"
#include "debug.h"

static debuginfo DEBUG[] = { /* !!! Check cish_defines.h */
	{"acl","kernel: (acl)","Access list service", 0},
	{"bgp","bgpd[","BGP routing service", 0},
	{"bridge","kernel: bridge","Bridge daemon", 0},
	{"chat","chat[","Chat service", 0},
	{"config","config[","Configuration service", 0},
#ifdef OPTION_IPSEC
	{"crypto","pluto[","VPN daemon", 0},
#endif
#ifdef UDHCPD
	{"dhcp","udhcpd[","DHCP service", 0},
#else
	{"dhcp","dhcpd:","DHCP service", 0},
#endif
	{"ethernet0","kernel: eth0:","Ethernet daemon", 0},
	{"efm0","kernel: (EFM:","EFM daemon", 0},
#ifdef OPTION_IPSEC
	{"l2tp","l2tpd","L2TP daemon", 0},
#endif
	{"login","login[","Login daemon", 0},
#ifdef OPTION_NTPD
	{"ntp","ntpd[","NTP daemon", 0},
#endif
#ifdef OPTION_ROUTER
	{"ospf","ospfd[","OSPF routing service", 0},
#endif
#ifdef OPTION_PPP
	{"ppp","pppd[","PPP daemon", 0},
#endif
#ifdef OPTION_ROUTER
	{"rip","ripd[","RIP routing service", 0},
#endif
	{"ssh","sshd","SSH service", 0},
#ifdef OPTION_VRRP
	{"vrrp","Keepalived_vrrp","VRRP service", 0},
#endif
	{"zebra","zebra[","Routing daemon", 0},
	{NULL, NULL, NULL, 0}
};

static int _librouter_find_debug(const char *name)
{
	int i;

	for (i = 0; DEBUG[i].name; i++) {
		if (!strncasecmp(name, DEBUG[i].name, strlen(DEBUG[i].name)))
			return i;
	}
	return -1;
}

char *librouter_debug_find_token(char *logline, char *name, int enabled)
{
	char *p, *l;
	debuginfo *dk;


	for (dk = DEBUG; dk->name; dk++) {
		if ((enabled || dk->enabled) &&	(l = strstr(logline, dk->token))) {

			p = strchr(logline + strlen(dk->token), ' ');

			if (p == NULL)
				return NULL;

			strncpy(name, dk->name, 14); /* renamed token! */
			strcat(name, ": ");

			return (p + 1); /* log info! */

			printf("logline = %s\n", logline);
			printf("l = %s\n", l);
			printf("p = %s\n", p);
			printf("name = %s\n", name);
		}
	}
	return NULL;
}

int librouter_debug_set_token(int on_off, const char *token)
{
	int i, j;

	i = _librouter_find_debug(token);

	if (i >= 0) {
		DEBUG[i].enabled = on_off;
#if 0
		/* debug persistent */
		cfg->debug[i]=on_off;
#endif
		printf("  %s debbuging is %s\n", DEBUG[i].description,
		                on_off ? "on" : "off");
	}

	if (!on_off) {

		/* Verifica se ainda ha algum debugging ligado. Se sim, mantem... */
		for (j = 0; DEBUG[j].name; j++) {
			if (DEBUG[j].enabled)
				return -1;
		}
	}
	return i;
}

void librouter_debug_set_all(int on_off)
{
	int i;

	for (i = 0; DEBUG[i].name; i++) {
		DEBUG[i].enabled = on_off;
#if 0
		cfg->debug[i]=on_off; /* debug persistent */
#endif
	}
	printf("  All debugging %s\n", on_off ? "enabled" : "disabled");
}

void librouter_debug_dump(void)
{
	int i;

	for (i = 0; DEBUG[i].name; i++) {
		if (DEBUG[i].enabled)
			printf("  %s debugging is on\n", DEBUG[i].description);
	}
}

int librouter_debug_get_state(const char *token)
{
	int i;

	i = _librouter_find_debug(token);
	if (i >= 0)
		return DEBUG[i].enabled;
	return i;
}

void librouter_debug_recover_all(void)
{
#if 0
	int i;

	for (i=0; DEBUG[i].name; i++)
	DEBUG[i].enabled=cfg->debug[i]; /* debug persistent */
#endif
}

