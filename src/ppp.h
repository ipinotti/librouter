#include <linux/config.h>
#include "typedefs.h"

#define PPP_CFG_FILE "/var/run/serial%d.config" /* 0 1 2 */
#define L2TP_PPP_CFG_FILE "/var/run/l2tp.%s.config" /* l2tpd */

#define PPP_CHAT_DIR "/var/run/chat/"
#define PPP_CHAT_FILE PPP_CHAT_DIR"%s"

#define PPP_UP_FILE_AUX "/var/run/ppp/aux%d.tty" /* ppp_get_state; ppp_get_pppdevice; */
#define PPP_UP_FILE_SERIAL "/var/run/ppp/serial%d.tty" /* ppp_get_state; ppp_get_pppdevice; */

#define PPP_PPPD_UP_FILE_AUX "/var/run/aux%d.pid" /* ppp_is_pppd_running; */
#define PPP_PPPD_UP_FILE_SERIAL "/var/run/serial%d.pid" /* ppp_is_pppd_running; */

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

enum
{
	FLOW_CONTROL_NONE=0,
	FLOW_CONTROL_RTSCTS,
	FLOW_CONTROL_XONXOFF
};

typedef struct
{
	char osdevice[16]; /* tts/wanX; tts/auxX */
	char cishdevice[16];
	int unit;
	char auth_user[MAX_PPP_USER], auth_pass[MAX_PPP_PASS];
	char chat_script[MAX_CHAT_SCRIPT];
	char ip_addr[16], ip_mask[16], ip_peer_addr[16];
	u32  ipx_network;
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
	int backup; /* 0:disable 1:aux0 2:aux1 */
	int activate_delay;
	int deactivate_delay;
	int server_flags;
	char server_auth_user[MAX_PPP_USER], server_auth_pass[MAX_PPP_PASS];
	char server_ip_addr[16], server_ip_mask[16], server_ip_peer_addr[16];
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
	int ip_unnumbered; /* Flag indicadora do IP UNNUMBERED (interface_major) -1 to disable */
	char peer[16];
	int peer_mask;
	int multilink; /* Enable multilink (mp) option */
	int debug;
	char apn[MAX_PPP_APN];
#ifdef CONFIG_HDLC_SPPP_LFI
	int fragment_size;
	int priomarks[CONFIG_MAX_LFI_PRIORITY_MARKS];
#endif
} ppp_config;

int notify_systtyd(void);
int notify_mgetty(int serial_no);
int ppp_get_config(int serial_no, ppp_config *cfg);
int ppp_set_config(int serial_no, ppp_config *cfg);
int ppp_has_config(int serial_no);
int ppp_add_chat(char *chat_name, char *chat_str);
int ppp_del_chat(char *chat_name);
char *ppp_get_chat(char *chat_name);
int ppp_chat_exists(char *chat_name);
void ppp_set_defaults(int serial_no, ppp_config *cfg);
int ppp_get_state(int serial_no);
int ppp_is_pppd_running(int serial_no);
char *ppp_get_pppdevice(int serial_no);
void ppp_pppd_arglist(char **arglist, ppp_config *cfg, int server);

int l2tp_ppp_get_config(char *name, ppp_config *cfg);
int l2tp_ppp_has_config(char *name);
int l2tp_ppp_set_config(char *name, ppp_config *cfg);
void l2tp_ppp_set_defaults(char *name, ppp_config *cfg);

