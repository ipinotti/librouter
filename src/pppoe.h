/*
 * pppoe.h
 *
 *  Created on: Dec 10, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef PPPOE_H_
#define PPPOE_H_


#define PPPOE_OPTIONSPPPOE "/etc/ppp/options.pppoe"
#define PPPOE_TUNNEL "/etc/ppp/peers/tunnel_pppoe"
#define PPPOE_CHAPSECRETS "/etc/ppp/chap-secrets"
#define PPPOE_PAPSECRETS "/etc/ppp/pap-secrets"
#define MAX_LINES_PPPOE 30
#define DEF_SIZE_FIELDS_PPPOE 128
#define PPPOE_PPP "tunnel_pppoe"

typedef struct {
	char service_name[DEF_SIZE_FIELDS_PPPOE];
	char ac_name[DEF_SIZE_FIELDS_PPPOE];
	char username[DEF_SIZE_FIELDS_PPPOE];
	char password[DEF_SIZE_FIELDS_PPPOE];
	char network[DEF_SIZE_FIELDS_PPPOE];
} pppoe_config;


int librouter_pppoe_set_config(pppoe_config * pppoe_cfg);
int librouter_pppoe_set_config_cli(char * field, char * value);
int librouter_pppoe_analyze_input(char * input);
int librouter_pppoe_get_config(pppoe_config * pppoe_conf);
int librouter_pppoe_set_clientmode(int add);
int librouter_pppoe_get_clientmode(void);
int librouter_pppoe_dump(FILE *out);



#endif /* PPPOE_H_ */
