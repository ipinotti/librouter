/*
 * modem3G.h
 *
 *  Created on: Apr 12, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef _MODEM3G_H
#define _MODEM3G_H

#define SIZE_FIELDS_STRUCT 256

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


#endif
