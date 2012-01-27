#ifndef __LIBROUTER_VRRP_H
#define __LIBROUTER_VRRP_H

#define VRRP_STATE_INIT			0	/* rfc2338.6.4.1 */
#define VRRP_STATE_BACK			1	/* rfc2338.6.4.2 */
#define VRRP_STATE_MAST			2	/* rfc2338.6.4.3 */

#define MAX_VRRP_IFNAME 16
#define MAX_VRRP_AUTHENTICATION 32
#define MAX_VRRP_DESCRIPTION 81
#define MAX_VRRP_IP 8

struct vrrp_group
{
	unsigned char ifname[MAX_VRRP_IFNAME];
	unsigned char group;
	unsigned char authentication_type;
	unsigned char authentication_password[MAX_VRRP_AUTHENTICATION];
	unsigned char description[MAX_VRRP_DESCRIPTION];
	struct in_addr ip[MAX_VRRP_IP];
	unsigned char preempt;
	unsigned int  preempt_delay;
	unsigned char priority;
	unsigned char advertise_delay;
	unsigned char track_interface[MAX_VRRP_IFNAME];
};

enum {
	VRRP_AUTHENTICATION_NONE=0,
	VRRP_AUTHENTICATION_AH,
	VRRP_AUTHENTICATION_TEXT
};

#define VRRP_DAEMON "keepalived"
#define VRRP_CONF_FILE "/etc/keepalived/keepalived.conf"
#define VRRP_BIN_FILE "/etc/keepalived/keepalived.bin"
#define VRRP_SHOW_FILE "/var/run/vrrp.show"
#define VRRP_STAT_FILE "/var/run/vrrp.stat"

void librouter_vrrp_no_group(char *dev, int group);
void librouter_vrrp_option_authenticate(char *dev, int group, int type, char *password);
void librouter_vrrp_option_description(char *dev, int group, char *description);
int librouter_vrrp_option_ip(char *dev, int group, int add, char *ip_string, int secondary);
void librouter_vrrp_option_preempt(char *dev, int group, int preempt, int delay);
void librouter_vrrp_option_priority(char *dev, int group, int priority);
void librouter_vrrp_option_advertise_delay(char *dev, int group, int advertise_delay);
void librouter_vrrp_option_track_interface(char *dev, int group, char *track_iface);
void librouter_vrrp_dump_interface(FILE *out, char *dev);
void librouter_vrrp_dump_status(void);


/* FIXME Copied from keepalived vrrp.h */
struct vrrp_status_t {
	char ifname[24];
	int vrid;
	int base_priority;
	int state;
	int adver_int;
	int down_interval;
	int master_prio;
	char ipaddr[16];
};

#define VRRP_STATUS_FILE	"/var/run/vrrp.status.%d"
#define VRRPD_PID_FILE 		"/var/run/keepalived.pid"

#endif /* __LIBROUTER_VRRP_H */
