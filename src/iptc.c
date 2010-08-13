/*
 * iptc.c
 *
 *  Created on: Aug 10, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include <libiptc/libiptc.h>

#include "error.h"
#include "exec.h"
#include "iptc.h"

const char *filter_table = "filter";
const char *nat_table = "nat";
const char *mangle_table = "mangle";

/**
 * _get_iptc_handle	Get handle loading iptables module if necessary
 *
 * @param filter_table
 * @return
 */
static struct iptc_handle * _get_iptc_handle(const char *filter_table)
{
	struct iptc_handle *h;

	h = iptc_init(filter_table);
	/* Perhaps we need to insmod */
	if (h == NULL) {
		librouter_exec_prog(0, "/sbin/modprobe", "ip_tables", NULL);
		sleep(1); /* wait a little bit */
		h = iptc_init(filter_table);
	}

	if (h == NULL) {
		librouter_logerr("Error initializing: %s\n", iptc_strerror(errno));
		return NULL;
	}

	return h;
}

/**
 * _iptc_query		Query iptables sub-system for chain names
 *
 * Names are filled in table buffer
 *
 * @param buf
 * @param table
 * @return
 */
static int _iptc_query(char *buf, const char *table)
{
	struct iptc_handle *h;
	const char *chain = NULL;

	h = _get_iptc_handle(table);
	if (h == NULL)
		return -1;

	for (chain = iptc_first_chain(h); chain; chain = iptc_next_chain(h)) {
		/* Ignore built-in chains */
		if (iptc_builtin(chain, h))
			continue;

		strcat(buf, chain);
		strcat(buf, " "); /* separate with spaces */
	}

	iptc_free(h);

	return 0;
}

int librouter_iptc_query_filter(char *buf)
{
	return _iptc_query(buf, filter_table);
}

int librouter_iptc_query_nat(char *buf)
{
	return _iptc_query(buf, nat_table);
}

int librouter_iptc_query_mangle(char *buf)
{
	return _iptc_query(buf, mangle_table);
}

/**
 * _iptc_get_chain_for_interface	Search chain for a determined interface
 *
 * This function checks whether the interface in question is bounded
 * to the INPUT or OUTPUT chain, depending on the requested direction.
 * If so, it returns the target chain, the one created by the user.
 *
 * @param table
 * @param direction: 0 for input, 1 for output
 * @param interface
 * @return chain name
 */
static char *_iptc_get_chain_for_interface(const char *table, int direction, char *interface)
{
	struct iptc_handle *h;
	const struct ipt_entry *e;
	const char *chain = NULL;

	h = _get_iptc_handle(filter_table);
	if (h == NULL)
		return NULL;

	for (chain = iptc_first_chain(h); chain != NULL; chain = iptc_next_chain(h)) {
		/* Check which chain we are interested in */
		if (direction) {
			if (strcmp(chain, "OUTPUT"))
				break;
		} else {
			if (strcmp(chain, "INPUT"))
				break;
		}
		/* For each rule in the chain, check if it has the interface we want
		 * and return the target chain */
		for (e = iptc_first_rule(chain, h); e != NULL; e = iptc_next_rule(e,h)) {
			if (!strcmp(interface, e->ip.iniface))
				return ((char *)iptc_get_target(e, h));
		}
	}

	return NULL;
}

char *librouter_iptc_filter_get_chain_for_iface(int direction, char *interface)
{
	return _iptc_get_chain_for_interface(filter_table, direction, interface);
}

char *librouter_iptc_nat_get_chain_for_iface(int direction, char *interface)
{
	return _iptc_get_chain_for_interface(nat_table, direction, interface);
}

char *librouter_iptc_mangle_get_chain_for_iface(int direction, char *interface)
{
	return _iptc_get_chain_for_interface(mangle_table, direction, interface);
}
