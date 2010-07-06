/*
 * ppp.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef PPP_H_
#define PPP_H_

#include <linux/config.h>
#include "../../cish/util/backupd.h" /*FIXME*/


/* 0 1 2 */
#define PPP_CFG_FILE "/var/run/serial%d.config"
/* l2tpd */
#define L2TP_PPP_CFG_FILE "/var/run/l2tp.%s.config"

#define PPP_CHAT_DIR "/var/run/chat/"
#define PPP_CHAT_FILE PPP_CHAT_DIR"%s"

/* ppp_get_state; ppp_get_pppdevice; */
#define PPP_UP_FILE_AUX "/var/run/ppp/aux%d.tty"

/* ppp_get_state; ppp_get_pppdevice; */
#define PPP_UP_FILE_SERIAL "/var/run/ppp/serial%d.tty"

/* ppp_is_pppd_running; */
#define PPP_PPPD_UP_FILE_AUX "/var/run/aux%d.pid"
/* ppp_is_pppd_running; */
#define PPP_PPPD_UP_FILE_SERIAL "/var/run/serial%d.pid"

#if defined(CONFIG_DEVFS_FS)
#define MGETTY_PID_AUX "/var/run/mgetty.pid.tts_aux%d"
#define MGETTY_PID_WAN "/var/run/mgetty.pid.tts_wan%d"
#define LOCK_PID_AUX "/var/lock/LCK..tts_aux%d"
#define LOCK_PID_WAN "/var/lock/LCK..tts_wan%d"
#else
#define MGETTY_PID_AUX "/var/run/mgetty.pid.ttyS%d"
#define MGETTY_PID_WAN "/var/run/mgetty.pid.ttyW%d"
#define LOCK_PID_AUX "/var/lock/LCK..ttyS%d"
#define LOCK_PID_WAN "/var/lock/LCK..ttyW%d"
#endif

#define MAX_PPP_USER 256
#define MAX_PPP_PASS 256
#define MAX_CHAT_SCRIPT 256
#define MAX_PPP_APN 256

#define SERVER_FLAGS_ENABLE 0x01
#define SERVER_FLAGS_INCOMING 0x02
#define SERVER_FLAGS_PAP 0x04
#define SERVER_FLAGS_CHAP 0x08

#define MAX_RADIUS_AUTH_KEY 256
#define MAX_RADIUS_SERVERS 256

enum {
	FLOW_CONTROL_NONE = 0, FLOW_CONTROL_RTSCTS, FLOW_CONTROL_XONXOFF
};

typedef struct {
	/* tts/wanX; tts/auxX */
	char osdevice[16];
	char cishdevice[16];
	int unit;
	char auth_user[MAX_PPP_USER];
	char auth_pass[MAX_PPP_PASS];
	char chat_script[MAX_CHAT_SCRIPT];
	char ip_addr[16];
	char ip_mask[16];
	char ip_peer_addr[16];
	unsigned int ipx_network;
	char ipx_node[6];
	int ipx_enabled;
	int default_route;
	int novj;
	int mtu, speed;
	int flow_control;
	int up;
	int sync_nasync;
	int dial_on_demand;
	int idle;
	int holdoff;
	int usepeerdns;
	int echo_interval;
	int echo_failure;
	/* 0:disable 1:aux0 2:aux1 */
	int backup;
	int activate_delay;
	int deactivate_delay;
	int server_flags;
	char server_auth_user[MAX_PPP_USER];
	char server_auth_pass[MAX_PPP_PASS];
	char server_ip_addr[16];
	char server_ip_mask[16];
	char server_ip_peer_addr[16];
	char radius_authkey[MAX_RADIUS_AUTH_KEY];
	int radius_retries;
	int radius_sameserver;
	char radius_servers[MAX_RADIUS_SERVERS];
	int radius_timeout;
	int radius_trynextonreject;
	char tacacs_authkey[MAX_RADIUS_AUTH_KEY];
	int tacacs_sameserver;
	char tacacs_servers[MAX_RADIUS_SERVERS];
	int tacacs_trynextonreject;
	/* Flag indicadora do IP UNNUMBERED (interface_major) -1 to disable */
	int ip_unnumbered;
	char peer[16];
	int peer_mask;
	/* Enable multilink (mp) option */
	int multilink;
	int debug;
	char apn[MAX_PPP_APN];
	struct bckp_conf_t bckp_conf;
#ifdef CONFIG_HDLC_SPPP_LFI
int fragment_size;
int priomarks[CONFIG_MAX_LFI_PRIORITY_MARKS];
#endif
} ppp_config;

int librouter_ppp_notify_systtyd(void);
int librouter_ppp_notify_mgetty(int serial_no);
int librouter_ppp_get_config(int serial_no, ppp_config *cfg);
int librouter_ppp_set_config(int serial_no, ppp_config *cfg);
int librouter_ppp_has_config(int serial_no);
int librouter_ppp_add_chat(char *chat_name, char *chat_str);
int librouter_ppp_del_chat(char *chat_name);
char *librouter_ppp_get_chat(char *chat_name);
int librouter_ppp_chat_exists(char *chat_name);
void librouter_ppp_set_defaults(int serial_no, ppp_config *cfg);
int librouter_ppp_get_state(int serial_no);
int librouter_ppp_is_pppd_running(int serial_no);
char *librouter_ppp_get_device(int serial_no);
void librouter_ppp_pppd_arglist(char **arglist, ppp_config *cfg, int server);

int librouter_ppp_l2tp_get_config(char *name, ppp_config *cfg);
int librouter_ppp_l2tp_has_config(char *name);
int librouter_ppp_l2tp_set_config(char *name, ppp_config *cfg);
void librouter_ppp_l2tp_set_defaults(char *name, ppp_config *cfg);

int librouter_ppp_reload_backupd(void);
int librouter_ppp_set_param_infile_backupd(char * intf, char * field, char *value);
int librouter_ppp_verif_param_infile_backupd(char * intf, char *field, char *value);
int librouter_ppp_verif_param_byintf_infile_backupd(char * intf, char *field, char *value);


#endif /* PPP_H_ */
