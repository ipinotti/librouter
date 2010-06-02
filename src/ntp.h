
#define NTP_DAEMON "ntpd"
#define NTP_PID "/var/run/ntpd.pid"
#define NTP_KEY_FILE "/etc/ntp.keys"

#define NTP_CFG_FILE "/var/run/ntp.config"

int ntp_set(int timeout, char *ip);
int ntp_get(int *timeout, char *ip);

void ntp_hup(void);
int is_ntp_auth_used(void);
int do_ntp_authenticate(int used);
int do_ntp_restrict(char *server, char *mask);
int do_ntp_server(char *server, char *key_num);
int do_ntp_trust_on_key(char *num);
int do_exclude_ntp_restrict(char *addr);
int do_exclude_ntp_server(char *addr);
int do_exclude_ntp_trustedkeys(char *num);
int do_ntp_key_set(char *key_num, char *value);

void lconfig_ntp_dump(FILE *out);

