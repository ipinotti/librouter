/*
 * ps.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <pwd.h>

#include "options.h"
#include "ps.h"

static struct {
	char *linux_name;
	char *display_name;
} proc_names[] = {
		{ "syslogd", "System Logger" },
		{ "klogd", "Kernel Logger" },
		{ "cish", "Configuration Shell" },
		{ "pppd", "PPP Session" },
		{ "inetd", "Service Multiplexer" },
		{ "systtyd", "Runtime System" },
		{ "backupd", "Conection Manager"},
		{ "wnsd", "HTTP Server" },
		{ "wnsslsd", "HTTPS Server" },
#ifdef OPTION_OPENSSH
                { "sshd", "SSH Server" },
#else
                { "dropbear", "SSH Server"},
#endif
                { "telnetd", "Telnet Server" },
                { "ftpd", "FTP Server" },
                { "snmpd", "SNMP Agent" },
                { "ospfd", "OSPF Server" },
                { "ripd", "RIP Server" },
#ifdef OPTION_BGP
                { "bgpd", "BGP Server" },
#endif
                { "udhcpd", "DHCP Server" },
                { "dhcrelay", "DHCP Relay" },
                { "rfc1356", "RFC1356 Tunnel" },
                { "dnsmasq", "DNS Relay" },
#ifdef OPTION_NTPD
                { "ntpd", "NTP Server" },
#endif
#ifdef OPTION_IPSEC
                { "/lib/ipsec/pluto", "VPN Server" },
                { "l2tpd", "L2TP Server" },
#endif
#ifdef OPTION_PIMD
                { "pimdd", "PIM-DM Server" }, { "pimsd", "PIM-SM Server" },
#endif
#ifdef OPTION_RMON
                { "rmond", "RMON Server" },
#endif
#ifdef OPTION_VRRP
                { "keepalived", "VRRP Server"},
#endif
                { NULL, NULL }
};

static char *_nexttoksep(char **strp, char *sep)
{
	char *p = strsep(strp, sep);
	return (p == 0) ? "" : p;
}
static char *_nexttok(char **strp)
{
	return _nexttoksep(strp, " ");
}

static int _ps_line(struct process_t *ps)
{
	int pid = ps->pid;
	char statline[1024];
	char cmdline[1024];
	char user[32];
	struct stat stats;
	int fd, r;
	char *ptr, *name, *state;
	int ppid, tty;
	unsigned wchan, rss, vss, eip;
	unsigned utime, stime;
	int prio, nice, rtprio, sched;
	struct passwd *pw;
	int i, found = 0;

	sprintf(statline, "/proc/%d", pid);
	stat(statline, &stats);

	sprintf(statline, "/proc/%d/stat", pid);
	sprintf(cmdline, "/proc/%d/cmdline", pid);

	fd = open(cmdline, O_RDONLY);
	if (fd == 0) {
		r = 0;
	} else {
		r = read(fd, cmdline, 1023);
		close(fd);
		if (r < 0)
			r = 0;
	}
	cmdline[r] = 0;

	fd = open(statline, O_RDONLY);
	if (fd == 0)
		return -1;

	r = read(fd, statline, 1023);
	close(fd);
	if (r < 0)
		return -1;
	statline[r] = 0;

	ptr = statline;
	_nexttok(&ptr); // skip pid
	ptr++; // skip "("

	name = ptr;
	ptr = strrchr(ptr, ')'); // Skip to *last* occurence of ')',
	*ptr++ = '\0'; // and null-terminate name.

	ptr++; // skip " "
	state = _nexttok(&ptr);
	ppid = atoi(_nexttok(&ptr));
	_nexttok(&ptr); // pgrp
	_nexttok(&ptr); // sid
	tty = atoi(_nexttok(&ptr));

	_nexttok(&ptr); // tpgid
	_nexttok(&ptr); // flags
	_nexttok(&ptr); // minflt
	_nexttok(&ptr); // cminflt
	_nexttok(&ptr); // majflt
	_nexttok(&ptr); // cmajflt
#if 1
	utime = atoi(_nexttok(&ptr));
	stime = atoi(_nexttok(&ptr));
#else
	_nexttok(&ptr); // utime
	_nexttok(&ptr); // stime
#endif
	_nexttok(&ptr); // cutime
	_nexttok(&ptr); // cstime
	prio = atoi(_nexttok(&ptr));
	nice = atoi(_nexttok(&ptr));
	_nexttok(&ptr); // threads
	_nexttok(&ptr); // itrealvalue
	_nexttok(&ptr); // starttime
	vss = strtoul(_nexttok(&ptr), 0, 10); // vsize
	rss = strtoul(_nexttok(&ptr), 0, 10); // rss
	_nexttok(&ptr); // rlim
	_nexttok(&ptr); // startcode
	_nexttok(&ptr); // endcode
	_nexttok(&ptr); // startstack
	_nexttok(&ptr); // kstkesp
	eip = strtoul(_nexttok(&ptr), 0, 10); // kstkeip
	_nexttok(&ptr); // signal
	_nexttok(&ptr); // blocked
	_nexttok(&ptr); // sigignore
	_nexttok(&ptr); // sigcatch
	wchan = strtoul(_nexttok(&ptr), 0, 10); // wchan
	_nexttok(&ptr); // nswap
	_nexttok(&ptr); // cnswap
	_nexttok(&ptr); // exit signal
	_nexttok(&ptr); // processor
	rtprio = atoi(_nexttok(&ptr)); // rt_priority
	sched = atoi(_nexttok(&ptr)); // scheduling policy

	tty = atoi(_nexttok(&ptr));

	pw = getpwuid(stats.st_uid);
	if (pw == 0) {
		sprintf(user, "%d", (int) stats.st_uid);
	} else {
		strcpy(user, pw->pw_name);
	}

	for (i = 0; proc_names[i].linux_name != NULL; i++) {
		if (!strcmp(name, proc_names[i].linux_name)) {
			found = 1;
			break;
		}
	}

	/* Not of our interest */
	if (!found) {
		ps->name[0] = 0;
		return -1;
	}

	sprintf(ps->name, "%s", proc_names[i].display_name);
	sprintf(ps->up_time, "(u:%d, s:%d)", utime, stime);
	sprintf(ps->user, "%s", user);

	return 0;
}

/**
 * librouter_ps_get_info
 *
 * Search for running processes
 * lconfig_free_ps_info be called after usage
 *
 * @return linked list of running processes
 */
struct process_t *librouter_ps_get_info(void)
{
	DIR *d;
	struct dirent *de;
	struct process_t *ps, *cur, *next;
	int first = 1;

	d = opendir("/proc");
	if (d == 0)
		return NULL;

	while ((de = readdir(d)) != 0) {
		if (isdigit(de->d_name[0])) {
			next = malloc(sizeof(struct process_t));
			if (next == NULL) {
				printf("%% Not enough space to store process information\n");
				return NULL;
			}
			memset(next, 0, sizeof(struct process_t));

			if (first) {
				ps = cur = next;
				first = 0;
			} else {
				cur->next = next;
				cur = next;
			}

			cur->pid = atoi(de->d_name);

			_ps_line(cur);
		}
	}
	closedir(d);
	return ps;
}


/**
 * lconfig_free_ps_info
 *
 * Free process info fetched with lconfig_get_ps_info
 *
 * @param ps
 */
void librouter_ps_free_info (struct process_t *ps)
{
	struct process_t *next;

	while (ps != NULL) {
		next = ps->next;
		free(ps);
		ps = next;
	}
}

