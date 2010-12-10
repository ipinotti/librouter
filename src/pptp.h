/*
 * pptp.h
 *
 *  Created on: Dec 2, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef PPTP_H_
#define PPTP_H_

#define PPTP_OPTIONSPPTP "/etc/ppp/options.pptp"
#define PPTP_TUNNEL "/etc/ppp/peers/tunnel_pptp"
#define PPTP_CHAPSECRETS "/etc/ppp/chap-secrets"
#define MAX_LINES_PPTP 30
#define DEF_SIZE_FIELDS_PPTP 128
#define PPTP_PPP "tunnel_pptp"

typedef struct {
	char server[DEF_SIZE_FIELDS_PPTP];
	char domain[DEF_SIZE_FIELDS_PPTP];
	char username[DEF_SIZE_FIELDS_PPTP];
	char password[DEF_SIZE_FIELDS_PPTP];
	int mppe;
} pptp_config;

int librouter_pptp_set_config(pptp_config * pptp_cfg);
int librouter_pptp_set_config_cli(char * field, char * value);
int librouter_pptp_analyze_input(char * input);
int librouter_pptp_set_mppe(int add);
int librouter_pptp_get_config(pptp_config * pptp_conf);
int librouter_pptp_set_clientmode(int add);
int librouter_pptp_get_clientmode(void);
int librouter_pptp_dump(FILE *out);



#endif /* PPTP_H_ */
