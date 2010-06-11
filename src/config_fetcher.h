/*
 * config_fetcher.h
 *
 *  Created on: May 31, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef CONFIG_FETCHER_H_
#define CONFIG_FETCHER_H_

cish_config *lconfig_mmap_cfg(void);
int lconfig_munmap_cfg(cish_config *cish_cfg);

void lconfig_dump_version(FILE *f, cish_config *cish_cfg);
void lconfig_dump_terminal(FILE *f, cish_config *cish_cfg);
void lconfig_dump_secret(FILE *f, cish_config *cish_cfg);
void lconfig_dump_aaa(FILE *f, cish_config *cish_cfg);
void lconfig_dump_hostname(FILE *f);
void lconfig_dump_ip(FILE *f, int conf_format);
void lconfig_dump_snmp(FILE *f, int conf_format);
void lconfig_dump_rmon(FILE *f);
void lconfig_dump_chatscripts(FILE *f);

void lconfig_ospf_dump_router(FILE *out);
void lconfig_rip_dump_router(FILE *out);
void lconfig_bgp_dump_router(FILE *f, int main_nip);

void dump_ospf_interface(FILE *out, char *intf);
void dump_rip_interface(FILE *out, char *intf);

void lconfig_dump_routing(FILE *f);

int lconfig_write_config(char *filename, cish_config *cish_cfg);
#endif
