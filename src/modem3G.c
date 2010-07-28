/*
 * modem3G.c
 *
 *  Created on: Apr 12, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <ctype.h>
#include <termios.h>
#include <syslog.h>

#include "str.h"
#include "error.h"
#include "modem3G.h"


/**
 * Função grava informações referentes as configurações do SIM card (M3G0) no arquivo apontado por MODEM3G_SIM_INFO_FILE.
 * É necessário passar por parâmetro o número do cartão (0 ou 1), o campo da configuração e o valor desejado.
 * @param sim
 * @param field
 * @param info
 * @return 0 if ok, -1 if not
 */
int librouter_modem3g_sim_set_info_infile(int sim, char * field, char * info){
	FILE *fd;
	char line[128] = {(int)NULL};
	char buff[220] = {(int)NULL};
	char filenamesim_new[64], fvalue[32], sim_ref[32];

	snprintf(sim_ref,32,"%s%d",SIMCARD_STR,sim);
	snprintf(fvalue,32,"%s=%s\n",field, info);

	if ((fd = fopen(MODEM3G_SIM_INFO_FILE, "r")) == NULL) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return -1;
	}

	while (fgets(line, sizeof(line), fd) != NULL) {
		if (!strncmp(line, sim_ref, strlen(sim_ref))) {
			strcat(buff, (const char *)line);
			while( (fgets(line, sizeof(line), fd) != NULL) && (strcmp(line, "\n") != 0) ){
				if (!strncmp(line, field, strlen(field))){
					strcat(buff, (const char *)fvalue);
					break;
				}
				else
					strcat(buff,(const char *)line);
			}
			continue;
		}
		strcat(buff, (const char *)line);
	}

	fclose(fd);

	strncpy(filenamesim_new, MODEM3G_SIM_INFO_FILE, 63);
	filenamesim_new[63] = 0;
	strcat(filenamesim_new, ".new");

	if ((fd = fopen((const char *)filenamesim_new,"w+")) < 0) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return -1;
	}

	fputs((const char *)buff,fd);

	fclose(fd);

	unlink(MODEM3G_SIM_INFO_FILE);
	rename(filenamesim_new, MODEM3G_SIM_INFO_FILE);

	return 0;

}

/**
 * Função recupera informações do SIM card (M3G0) desejado, passado por parâmetro, que estão gravadas no arquivo apontado por MODEM3G_SIM_INFO_FILE.
 * É passado por parâmetro a struct sim_conf, sendo necessário informar nesta struct, qual SIM card desejado (0 ou 1).
 * @param sim_card
 * @return 0 if ok, -1 if not.
 */
int librouter_modem3g_sim_get_info_fromfile(struct sim_conf * sim_card){
	FILE *fd;
	char line[128] = {(int)NULL};
	char sim_ref[32];
	char *p;
	snprintf(sim_ref,32,"%s%d\n",SIMCARD_STR, sim_card->sim_num);

	if ((fd = fopen(MODEM3G_SIM_INFO_FILE, "r")) == NULL) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return -1;
	}

	while (fgets(line, sizeof(line), fd) != NULL) {
		if (!strncmp(line, sim_ref, strlen(sim_ref))) {
			while( (fgets(line, sizeof(line), fd) != NULL) && (strcmp(line, "\n") != 0) ){

				if (!strncmp(line, APN_STR, APN_STR_LEN)) {
					strcpy(sim_card->apn, line + APN_STR_LEN);

					/* Remove any line break */
					for (p = sim_card->apn; *p != '\0'; p++) {
						if (*p == '\n')
							*p = '\0';
					}
					continue;
				}

				if (!strncmp(line, USERN_STR, USERN_STR_LEN)){
					strcpy(sim_card->username, line + USERN_STR_LEN);

					/* Remove any line break */
					for (p = sim_card->username; *p != '\0'; p++) {
						if (*p == '\n')
							*p = '\0';
					}
					continue;
				}

				if (!strncmp(line, PASSW_STR, PASSW_STR_LEN)){
					strcpy(sim_card->password, line + PASSW_STR_LEN);

					/* Remove any line break */
					for (p = sim_card->password; *p != '\0'; p++) {
						if (*p == '\n')
							*p = '\0';
					}
					continue;
				}
			}
			break;
		}
	}

	fclose(fd);
	return 0;

}


/**
 * Grava o main SIM CARD no arquivo apontado por MODEM3G_SIM_ORDER_FILE
 * SIM CARDS --> 0 ou 1
 * @param sim
 * @return 0 if ok, -1 if not
 */
int librouter_modem3g_sim_order_set_mainsim(int sim){

	char file[] = MODEM3G_SIM_ORDER_FILE;
	char * card = malloc(2);
	int ret = -1;

	if ( !(sim == 0 || sim == 1) )
		goto end;

	sprintf(card,"%d", sim);

	ret = librouter_str_replace_string_in_file(file,MAINSIM_STR,card);

end:
	free(card);
	return ret;
}

/**
 * Recupera o main SIM CARD armazenado no arquivo apontador por MODEM3G_SIM_ORDER_FILE
 * @return Number of the main SIM
 */
int librouter_modem3g_sim_order_get_mainsim(){

	char file[] = MODEM3G_SIM_ORDER_FILE;
	char * card = malloc(2);
	int card_int = -1;

	if (librouter_str_find_string_in_file(file,MAINSIM_STR,card,sizeof(card)) < 0)
		goto end;

	card_int = atoi(card);

end:
	free(card);
	return card_int;
}

/**
 * Habilita(1) ou desabilita(0) suporte de backup do MAIN SIM pelo BACKUP SIM
 *
 * @param value
 * @return 0 if ok, -1 if not
 */
int librouter_modem3g_sim_order_set_enable(int value){

	char file[] = MODEM3G_SIM_ORDER_FILE;
	char * enable = malloc(2);
	int ret = -1;

	if ( !(value == 0 || value == 1) )
		goto end;

	sprintf(enable,"%d", value);

	ret = librouter_str_replace_string_in_file(file,SIMORDER_ENABLE_STR,enable);

end:
	free(enable);
	return ret;
}

/**
 * Recupera estado do sim_order (se está ativo o sistema de backup do MAIN SIM)
 * @return 1 if enable, 0 if not, -1 if a problem occurred
 */
int librouter_modem3g_sim_order_is_enable(){

	char file[] = MODEM3G_SIM_ORDER_FILE;
	char * enable = malloc(2);
	int result = -1;

	if (librouter_str_find_string_in_file(file,SIMORDER_ENABLE_STR,enable,sizeof(enable)) < 0)
		goto end;

	result = atoi(enable);

end:
	free(enable);
	return result;
}

/**
 * Função relacionada a seleção do SIM CARD pelo HW.
 * @return
 */
int librouter_modem3g_sim_card_set(int sim){

	return 0;
}

/**
 * Função relacionada a seleção do SIM CARD pelo Hw.
 * @return
 */
int librouter_modem3g_sim_card_get(){

	return 0;
}



/**
 * Adquire o APN - Acess Point Name, no arquivo de script - ARQ1,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna APN por parâmetro e retorna 0 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param apn
 * @param devcish
 * @return 0 if OK, -1 if not
 */
int librouter_modem3g_get_apn(char * apn, int devcish)
{
	int check = -1, length = 0, i = 0;
	char file[48] = MODEM3G_CHAT_FILE;
	char device[2];
	char key[] = "\"IP\",";

	snprintf(device, 2, "%d", devcish);
	strcat(file, device);

	check = librouter_str_find_string_in_file(file, key, apn, SIZE_FIELDS_STRUCT);
	length = strlen(apn);

	for (i = 1; i < (length - 1); ++i)
		apn[i - 1] = apn[i];

	/* retira aspas e demais no final do APN */
	apn[length - 3] = '\0';

	return check;

}

/**
 * Grava o APN - Acess Point Name, no arquivo de script - ARQ1,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * APN é passado por parâmetro.
 *
 * Retorna 0 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param apn
 * @param devcish
 * @return 0 if OK, -1 if not
 */
int librouter_modem3g_set_apn(char * apn, int devcish)
{
	int check = -1;
	char file[48] = MODEM3G_CHAT_FILE;
	char device[2];
	char key[] = "\"IP\",";
	char buffer_apn[SIZE_FIELDS_STRUCT] = "\"";
	char plus[] = "\"'";

	strcat(buffer_apn, apn);
	strcat(buffer_apn, plus);

	snprintf(device, 2, "%d", devcish);
	strcat(file, device);

	check = librouter_str_replace_string_in_file(file, key, buffer_apn);

	return check;
}

/**
 * Adquire o USERNAME, no arquivo de script - ARQ2,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna USERNAME por parâmetro e retorna 0 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param username
 * @return 0 if OK, -1 if not
 */
int librouter_modem3g_get_username(char * username, int devcish)
{
	int check = -1;
	char file[48] = MODEM3G_PEERS_FILE;
	char device[2];
	char key[] = "user";

	snprintf(device, 2, "%d", devcish);
	strcat(file, device);

	check = librouter_str_find_string_in_file(file, key, username, SIZE_FIELDS_STRUCT);

	return check;
}

/**
 * Grava o USERNAME, no arquivo de script - ARQ2,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * USERNAME é passado por parâmetro.
 *
 * Retorna 0 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param username
 * @param devcish
 * @return 0 if OK, -1 if not
 */
int librouter_modem3g_set_username(char * username, int devcish)
{
	int check = -1;
	char file[48] = MODEM3G_PEERS_FILE;
	char device[2];
	char key[] = "user";

	snprintf(device, 2, "%d", devcish);
	strcat(file, device);

	check = librouter_str_replace_string_in_file(file, key, username);

	return check;
}

/**
 * Adquire o PASSWORD, no arquivo de script - ARQ2,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna PASSWORD por parâmetro e retorna 0 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros
 * referente a função descrita em str.c
 *
 * @param password
 * @param devcish
 * @return 0 if OK, -1 if not
 */
int librouter_modem3g_get_password(char * password, int devcish)
{
	int check = -1;
	char file[48] = MODEM3G_PEERS_FILE;
	char device[2];
	char key[] = "password";

	snprintf(device, 2, "%d", devcish);
	strcat(file, device);

	check = librouter_str_find_string_in_file(file, key, password, SIZE_FIELDS_STRUCT);

	return check;
}

/**
 * Grava o PASSWORD, no arquivo de script - ARQ2,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * PASSWORD é passado por parâmetro.
 *
 * Retorna 0 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros
 * referente a função descrita em str.c
 *
 * @param password
 * @param devcish
 * @return 0 if OK, -1 if not
 */
int librouter_modem3g_set_password(char * password, int devcish)
{
	int check = -1;
	char file[48] = MODEM3G_PEERS_FILE;
	char device[2];
	char key[] = "password";

	snprintf(device, 2, "%d", devcish);
	strcat(file, device);

	check = librouter_str_replace_string_in_file(file, key, password);

	return check;
}

/**
 * Função grava APN, username e password referentes ao CHAT-PPP do modem3G,
 * através da struct sim_conf.
 *
 * @param sim
 * @param devcish
 * @return 0 if ok, -1 if not
 */
int librouter_modem3g_set_all_info_inchat(struct sim_conf * sim, int devcish){
	int check = -1;
	char key_apn[] = "\"IP\",";
	char key_user[] = "user";
	char key_pass[] = "password";
	char buffer_apn[SIZE_FIELDS_STRUCT] = {(int)NULL};
	char file[56] = {(int)NULL};

	snprintf(buffer_apn,SIZE_FIELDS_STRUCT,"\"%s\"'",sim->apn);
	snprintf(file,56,"%s%d",MODEM3G_CHAT_FILE,devcish);

	check = librouter_str_replace_string_in_file(file, key_apn, buffer_apn);
	if (check < 0)
		goto end;

	snprintf(file,56,"%s%d",MODEM3G_PEERS_FILE,devcish);

	check = librouter_str_replace_string_in_file(file, key_user, sim->username);
	if (check < 0)
		goto end;

	check = librouter_str_replace_string_in_file(file, key_pass, sim->password);

end:
	return check;
}

/**
 * Função permite setar informações do SIM Card 0 e 1 da M3G0 para o chat-script
 *
 * É necessário passar por parâmetro o número do SIM Card desejado, e a interface M3G, no momento, somente
 * a interfacae M3G0 possui DUAL SIM.
 * @param simcard
 * @param devcish -> must be 0 for now
 * @return 0 if ok, -1 if not
 */
int librouter_modem3g_sim_set_all_info_inchat(int simcard, int m3g){
	struct sim_conf * sim = malloc(sizeof(struct sim_conf));

	sim->sim_num = simcard;

	if (librouter_modem3g_sim_get_info_fromfile(sim) < 0 ){
		free(sim);
		return -1;

	}

	if (librouter_modem3g_set_all_info_inchat(sim,m3g) < 0){
		free(sim);
		return -1;
	}

	free (sim);
	return 0;

}
