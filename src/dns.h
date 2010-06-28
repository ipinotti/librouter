/*
 * dns.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef DNS_H_
#define DNS_H_

#define DNS_DAEMON "dnsmasq"
#define FILE_RESOLVCONF "/etc/resolv.conf"
#define FILE_RESOLVRELAYCONF "/etc/resolv.relay.conf"
#define FILE_NSSWITCHCONF "/etc/nsswitch.conf"
#define KW_NAMESERVER "nameserver "
#define DNS_MAX_SERVERS 3

enum {
	DNS_STATIC_NAMESERVER = 2, DNS_DYNAMIC_NAMESERVER
};

void libconfig_dns_nameserver(int add, char *nameserver);
void libconfig_dns_dynamic_nameserver(int add, char *nameserver);

int libconfig_dns_get_nameserver_by_type_actv_index(unsigned int type,
                                      unsigned int actv,
                                      unsigned int index,
                                      char *addr);
int libconfig_dns_get_nameserver_by_type_index(unsigned int type,
                                 unsigned int index,
                                 char *addr);
int libconfig_dns_get_nameserver_by_actv_index(unsigned int actv,
                                 unsigned int index,
                                 char *addr);

void libconfig_dns_lookup(int on_off);
int libconfig_dns_domain_lookup_enabled(void);

void libconfig_dns_dump_nameservers(FILE *out);

#endif /* DNS_H_ */
