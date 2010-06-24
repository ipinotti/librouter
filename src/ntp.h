/*
 * ntp.c
 *
 *  Created on: Jun 24, 2010
 */

#ifndef NTP_H_
#define NTP_H_

#define NTP_DAEMON "ntpd"
#define NTP_PID "/var/run/ntpd.pid"
#define NTP_KEY_FILE "/etc/ntp.keys"

#define NTP_CFG_FILE "/var/run/ntp.config"

int libconfig_ntp_set(int timeout, char *ip);
int libconfig_ntp_get(int *timeout, char *ip);

void libconfig_ntp_hup(void);
int libconfig_ntp_is_auth_used(void);
int libconfig_ntp_authenticate(int used);
int libconfig_ntp_restrict(char *server, char *mask);
int libconfig_ntp_server(char *server, char *key_num);
int libconfig_ntp_trust_on_key(char *num);
int libconfig_ntp_exclude_restrict(char *addr);
int libconfig_ntp_exclude_server(char *addr);
int libconfig_ntp_exclude_trustedkeys(char *num);
int libconfig_ntp_set_key(char *key_num, char *value);

void libconfig_ntp_dump(FILE *out);

#endif /* NTP_H_ */
