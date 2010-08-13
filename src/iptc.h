/*
 * iptc.h
 *
 *  Created on: Aug 10, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef IPTC_H_
#define IPTC_H_

#define DEBUG
#ifdef DEBUG
#define iptc_dbg(x,...) \
		syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define iptc_dbg(x,...)
#endif

int librouter_iptc_query_filter(char *buf);
int librouter_iptc_query_nat(char *buf);
int librouter_iptc_query_mangle(char *buf);

char * librouter_iptc_filter_get_chain_for_iface(int direction, char *interface);
char * librouter_iptc_nat_get_chain_for_iface(int direction, char *interface);
char * librouter_iptc_mangle_get_chain_for_iface(int direction, char *interface);

#endif /* IPTC_H_ */
