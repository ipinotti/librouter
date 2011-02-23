/*
 * pbr.h
 *
 *  Created on: Feb 2, 2011
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */
#include <syslog.h>


#ifndef PBR_H_
#define PBR_H_

#define DEBUG_PBR_SYSLOG
#ifdef DEBUG_PBR_SYSLOG
#define pbr_dbgs(x,...) \
		syslog(LOG_INFO,  "%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define pbr_dbgs(x,...)
#endif

//#define DEBUG_PBR_PRINTF
#ifdef DEBUG_PBR_PRINTF
#define pbr_dbgp(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define pbr_dbgp(x,...)
#endif

#define PBR_STR_SIZE 32

typedef struct {
	char network_opt[PBR_STR_SIZE];
	char network_opt_ipmask[PBR_STR_SIZE];
	char dev[PBR_STR_SIZE]; /* linux/kernel dev */
	char table[PBR_STR_SIZE];
	char via_ipaddr[PBR_STR_SIZE];
} librouter_pbr_struct;


int librouter_pbr_rule_add(int mark_no, char * table);
int librouter_pbr_rule_del(int mark_no, char * table);
int librouter_pbr_get_show_rules_cli(char * pbr_show_rules_buff);
int librouter_pbr_get_show_rules_cli_size(void);

int librouter_pbr_route_add(librouter_pbr_struct * pbr);
int librouter_pbr_route_del(librouter_pbr_struct * pbr);
int librouter_pbr_get_show_routes_cli(char * table, char * pbr_show_routes_buff);
int librouter_pbr_get_show_routes_cli_size(char * table);
int librouter_pbr_flush_route_table(char * table);
int librouter_pbr_flush_cache(void);


#endif /* PBR_H_ */
