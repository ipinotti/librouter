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

int librouter_ntp_set(int timeout, char *ip);
int librouter_ntp_get(int *timeout, char *ip);

void librouter_ntp_hup(void);
int librouter_ntp_is_auth_used(void);
int librouter_ntp_authenticate(int used);
int librouter_ntp_restrict(char *server, char *mask);
int librouter_ntp_server(char *server, char *key_num);
int librouter_ntp_trust_on_key(char *num);
int librouter_ntp_exclude_restrict(char *addr);
int librouter_ntp_exclude_server(char *addr);
int librouter_ntp_exclude_trustedkeys(char *num);
int librouter_ntp_set_key(char *key_num, char *value);

void librouter_ntp_dump(FILE *out);

int librouter_ntp_get_servers(char *servers);
int librouter_ntp_set_daemon(int enable);
int librouter_ntp_get_daemon(void);

#endif /* NTP_H_ */
