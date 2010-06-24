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

#include "str.h"
#include "error.h"
#include "modem3G.h"

#define MODEM3G_CHAT_FILE "/etc/ppp/chat-modem-3g-"
#define MODEM3G_PEERS_FILE "/etc/ppp/peers/modem-3g-"

/**
 * Adquire o APN - Acess Point Name, no arquivo de script - ARQ1,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna APN por parâmetro e retorna 1 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param apn
 * @return
 */
int libconfig_modem3g_get_apn (char * apn, int devcish)
{
	int check=0,length=0,i=0;
	char file[100] = MODEM3G_CHAT_FILE;
	char device[10];
	char key[] = "\"IP\",";

	snprintf(device,10,"%d",devcish);
	strcat(file,device);

	check = libconfig_str_find_string_in_file (file, key, apn, 100);
	length = strlen(apn);

	for (i = 1; i < (length-1); ++i)
		apn[i-1] = apn[i];

	apn[length-3]='\0';

	if (check == 0)
		return 1;
	else
		return -1;

}


/**
 * Grava o APN - Acess Point Name, no arquivo de script - ARQ1,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * APN é passado por parâmetro.
 *
 * Retorna 1 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param apn
 * @return
 */
int libconfig_modem3g_set_apn (char * apn, int devcish)
{
	//arq1 == "chat-modem-3g-";

	int check=0;
	char file[100] = MODEM3G_CHAT_FILE;
	char device[10];
	char key[] = "\"IP\",";

	snprintf(device,10,"%d",devcish);
	strcat(file,device);

	check = libconfig_str_replace_string_in_file (file, key, apn);

	if (check == 0)
		return 1;
	else
		return -1;
}

/**
 * Adquire o USERNAME, no arquivo de script - ARQ2,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna USERNAME por parâmetro e retorna 1 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param username
 * @return
 */
int libconfig_modem3g_get_username (char * username, int devcish)
{
	//arq2 == "modem-3g-";

	int check=0;
	char file[100] = MODEM3G_PEERS_FILE;
	char device[10];
	char key[] = "user";

	snprintf(device,10,"%d",devcish);
	strcat(file,device);

	check = libconfig_str_find_string_in_file (file, key, username, 100);

	if (check == 0)
		return 1;
	else
		return -1;

}

/**
 * Grava o USERNAME, no arquivo de script - ARQ2,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * USERNAME é passado por parâmetro.
 *
 * Retorna 1 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param username
 * @return
 */
int libconfig_modem3g_set_username (char * username, int devcish)
{
	//arq2 == "modem-3g-";

	int check=0;
	char file [100] = MODEM3G_PEERS_FILE;
	char device[10];
	char key[] = "user";

	snprintf(device,10,"%d",devcish);
	strcat(file,device);

	check = libconfig_str_replace_string_in_file (file, key, username);

	if (check == 0)
		return 1;
	else
		return -1;
}

/**
 * Adquire o PASSWORD, no arquivo de script - ARQ2,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna PASSWORD por parâmetro e retorna 1 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param password
 * @return
 */
int libconfig_modem3g_get_password (char * password, int devcish)
{
	//arq2 == "modem-3g-";

	int check=0;
	char file [100] = MODEM3G_PEERS_FILE;
	char device[10];
	char key[] = "password";

	snprintf(device,10,"%d",devcish);
	strcat(file,device);

	check = libconfig_str_find_string_in_file (file, key, password, 100);

	if (check == 0)
		return 1;
	else
		return -1;
}

/**
 * Grava o PASSWORD, no arquivo de script - ARQ2,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * PASSWORD é passado por parâmetro.
 *
 * Retorna 1 se sucesso.
 * Caso ocorra problema, é retornado -1 e controle de erros referente a função descrita em str.c
 *
 * @param password
 * @return
 */
int libconfig_modem3g_set_password (char * password, int devcish)
{
	//arq2 == "modem-3g-";

	int check=0;
	char file [100] = MODEM3G_PEERS_FILE;
	char device[10];
	char key[] = "password";

	snprintf(device,10,"%d",devcish);
	strcat(file,device);

	check = libconfig_str_replace_string_in_file (file, key, password);

	if (check == 0)
		return 1;
	else
		return -1;
}
