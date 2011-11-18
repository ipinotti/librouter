/*
 * modem3G.h
 *
 *  Created on: Apr 12, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef _MODEM3G_H
#define _MODEM3G_H

//#define DEBUG_MODEM3G_SYSLOG
#ifdef DEBUG_MODEM3G_SYSLOG
#define dbgS_modem3g(x,...) \
		syslog(LOG_INFO,  "%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define dbgS_modem3g(x,...)
#endif

//#define DEBUG_MODEM3G_PRINTF
#ifdef DEBUG_MODEM3G_PRINTF
#define dbgP_modem3g(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define dbgP_modem3g(x,...)
#endif



#define SIZE_FIELDS_STRUCT 256
#define BTIN_M3G_ALIAS 0

#define MODEM3G_CHAT_FILE 	"/etc/ppp/chat-modem-3g-"
#define MODEM3G_PEERS_FILE 	"/etc/ppp/peers/modem-3g-"
#define MODEM3G_SIM_ORDER_FILE 	"/etc/ppp/modem-3g-0-sim_order"
#define MODEM3G_SIM_INFO_FILE 	"/etc/ppp/modem-3g-0-sim_info"

#define SIMCARD_STR 		"simcard="
#define APN_STR			"apn="
#define USERN_STR 		"username="
#define PASSW_STR 		"password="

#define SIMCARD_STR_LEN		strlen(SIMCARD_STR)
#define APN_STR_LEN		strlen(APN_STR)
#define USERN_STR_LEN 		strlen(USERN_STR)
#define PASSW_STR_LEN 		strlen(PASSW_STR)

#define MAINSIM_STR		"mainsim="
#define SIMORDER_ENABLE_STR	"simorder_enable="

#define MAINSIM_STR_LEN		strlen(MAINSIM_STR)
#define SIMORDER_ENABLE_STR_LEN	strlen(SIMORDER_ENABLE_STR)

#define GPIO_SIM_SELECT_PIN	21
#define GPIO_SIM_SELECT_PORT	0

#ifdef OPTION_DUAL_SIM
#define NUMBER_OF_SIMCARD_PORTS 2
#else
#define NUMBER_OF_SIMCARD_PORTS 1
#endif

#define MODULE_STATUS_ON	1
#define MODULE_STATUS_OFF	0
#define MODULE_USBHUB_PORT	3

#define BACKUPD_PID_FILE        "/var/run/backupd.pid"
#define BACKUPD_CONF_FILE       "/etc/backupd/backupd.conf"

#define INTF_STR                "interface="
#define SHUTD_STR               "shutdown="
#define BCKUP_STR               "backing_up="
#define MAIN_INTF_STR           "main_interface="
#define METHOD_STR              "method="
#define PING_ADDR_STR           "ping-address="
#define DEFAULT_ROUTE_STR       "default-route="
#define ROUTE_DISTANCE_STR      "default-route-distance="

#define INTF_STR_LEN            strlen(INTF_STR)
#define SHUTD_STR_LEN           strlen(SHUTD_STR)
#define BCKUP_STR_LEN           strlen(BCKUP_STR)
#define MAIN_INTF_STR_LEN       strlen(MAIN_INTF_STR)
#define METHOD_STR_LEN          strlen(METHOD_STR)
#define PING_ADDR_STR_LEN       strlen(PING_ADDR_STR)
#define DEFAULT_ROUTE_STR_LEN   strlen(DEFAULT_ROUTE_STR)
#define ROUTE_DISTANCE_STR_LEN  strlen(ROUTE_DISTANCE_STR)

typedef enum {
	real_sc, alias_sc, non_sc
} port_simcard_type;

typedef struct {
	port_simcard_type type;
	const int port[NUMBER_OF_SIMCARD_PORTS];
} port_family_simcard;

extern port_family_simcard _simcard_ports[];

/*
 * Which methodology to use? For now
 * we define link and ping methods
 */
enum bckp_method {
        BCKP_METHOD_LINK,
        BCKP_METHOD_PING
};

enum bckp_config_field {
        FIELD_INTF,
        FIELD_SHUTD,
        FIELD_BCK_UP,
        FIELD_MAIN_INTF,
        FIELD_METHOD,
        FIELD_PING_ADDR,
        FIELD_INSTALL_DEFAULT_ROUTE,
        FIELD_ROUTE_DISTANCE
};

enum bckp_state {
        STATE_SHUTDOWN,
        STATE_NOBACKUP,
        STATE_SIMCHECK,
        STATE_WAITING,
        STATE_MAIN_INTF_RESTABLISHED,
        STATE_CONNECT
};

struct bckp_conf_t {
        struct bckp_conf_t *next;
        char intf_name[32];
        int is_backup; /* Is backing up another interface */
        int shutdown;
        char main_intf_name[32];
        enum bckp_method method;
        char ping_address[128];
        enum bckp_state state;
        pid_t pppd_pid;
        int is_default_gateway;
        int default_route_distance;
};

struct sim_conf {
	int sim_num;
	char apn[SIZE_FIELDS_STRUCT];
	char username[SIZE_FIELDS_STRUCT];
	char password[SIZE_FIELDS_STRUCT];
};

int librouter_modem3g_get_apn (char * apn, int devnum);
int librouter_modem3g_set_apn (char * apn, int devnum);
int librouter_modem3g_get_username (char * username, int devnum);
int librouter_modem3g_set_username (char * username, int devnum);
int librouter_modem3g_get_password (char * password, int devnum);
int librouter_modem3g_set_password (char * password, int devnum);
int librouter_modem3g_set_all_info_inchat(struct sim_conf * sim, int devnum);
int librouter_modem3g_sim_get_realport_by_aliasport(int simcard_port);
int librouter_modem3g_sim_get_aliasport_by_realport(int simcard_port);
int librouter_modem3g_sim_set_info_infile(int sim, char * field, char * info);
int librouter_modem3g_sim_get_info_fromfile(struct sim_conf * sim_card);
int librouter_modem3g_sim_order_set_mainsim(int sim);
int librouter_modem3g_sim_order_get_mainsim();
int librouter_modem3g_sim_card_set(int sim);
int librouter_modem3g_sim_card_get();
int librouter_modem3g_sim_order_set_enable(int value);
int librouter_modem3g_sim_order_is_enable();
int librouter_modem3g_sim_set_all_info_inchat(int simcard, int m3g);
int librouter_modem3g_sim_order_set_allinfo(int sim_main, int sim_back, char * interface, int intfnumber);
int librouter_modem3g_module_reset (void);
int librouter_modem3g_module_set_status (int status);


#endif
