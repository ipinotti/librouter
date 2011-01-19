/*
 * modem3G.h
 *
 *  Created on: Apr 12, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef _MODEM3G_H
#define _MODEM3G_H

#include "../../cish/util/backupd.h" /*FIXME*/

//#define DEBUG
#ifdef DEBUG
#define dbgS_modem3g(x,...) \
		syslog(LOG_INFO,  "%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define dbgS_modem3g(x,...)
#endif

//#define DEBUGB
#ifdef DEBUGB
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

#if defined(CONFIG_DIGISTAR_3G)
#define GPIO_MODULE_RESET_PIN   12
#elif defined(CONFIG_DIGISTAR_EFM)
#define GPIO_MODULE_RESET_PIN   17
#endif

#define MODULE_STATUS_ON	1
#define MODULE_STATUS_OFF	0
#define MODULE_USBHUB_PORT	3

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
