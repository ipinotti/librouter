/*
 * config_fetcher.h
 *
 *  Created on: May 31, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef CONFIG_FETCHER_H_
#define CONFIG_FETCHER_H_

cish_config *libconfig_config_mmap_cfg(void);
int libconfig_config_munmap_cfg(cish_config *cish_cfg);

void libconfig_config_dump_version(FILE *f, cish_config *cish_cfg);
void libconfig_config_dump_terminal(FILE *f, cish_config *cish_cfg);
void libconfig_config_dump_secret(FILE *f, cish_config *cish_cfg);
void libconfig_config_dump_aaa(FILE *f, cish_config *cish_cfg);
void libconfig_config_dump_hostname(FILE *f);
void libconfig_config_dump_ip(FILE *f, int conf_format);
void libconfig_config_dump_snmp(FILE *f, int conf_format);
void libconfig_config_dump_rmon(FILE *f);
void libconfig_config_dump_chatscripts(FILE *f);

void libconfig_config_ospf_dump_router(FILE *out);
void libconfig_config_rip_dump_router(FILE *out);
void libconfig_config_bgp_dump_router(FILE *f, int main_nip);

void libconfig_config_ospf_dump_interface(FILE *out, char *intf);
void libconfig_config_rip_dump_interface(FILE *out, char *intf);

void libconfig_config_dump_routing(FILE *f);

int libconfig_config_write(char *filename, cish_config *cish_cfg);

#endif /* CONFIG_FETCHER_H_ */
