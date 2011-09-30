/*
 * config_fetcher.h
 *
 *  Created on: May 31, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef CONFIG_FETCHER_H_
#define CONFIG_FETCHER_H_

void librouter_config_dump_version(FILE *f, struct router_config *cfg);
void librouter_config_dump_terminal(FILE *f, struct router_config *cfg);
void librouter_config_dump_secret(FILE *f, struct router_config *cfg);
void librouter_config_dump_aaa(FILE *f, struct router_config *cfg);
void librouter_config_dump_hostname(FILE *f);
void librouter_config_dump_ip(FILE *f, int conf_format);
void librouter_config_dump_snmp(FILE *f, int conf_format);
void librouter_config_dump_rmon(FILE *f);
void librouter_config_dump_chatscripts(FILE *f);

void librouter_config_ospf_dump_router(FILE *out);
void librouter_config_rip_dump_router(FILE *out);
void librouter_config_bgp_dump_router(FILE *f, int main_nip);

void librouter_config_ospf_dump_interface(FILE *out, char *intf);
void librouter_config_rip_dump_interface(FILE *out, char *intf);

void librouter_config_dump_routing(FILE *f);
void librouter_config_dump_routing_ipv6(FILE *f);

void librouter_config_dump_policy_route(FILE *f);

int librouter_config_write(char *filename, struct router_config *cfg);

#endif /* CONFIG_FETCHER_H_ */
