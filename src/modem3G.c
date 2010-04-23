/*
 ============================================================================
 Name        : modem3G.c
 Author      : Igor Pinotti
 Version     :
 Copyright   : Copyrighted
 Description : Functions for modem 3G
 ============================================================================
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

#define arq1 "chat-modem-3g"
#define arq2 "modem-3g"


/**
 * Adquire o APN - Acess Point Name, no arquivo de script - ARQ1,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna APN por parâmetro e retorna 1 se sucesso.
 * Caso ocorra problema, é retornado controle de erros referente a função descrita em str.c
 *
 * @param apn
 * @return
 */
int modem3g_get_apn (char * apn){
	//arq1 == "chat-modem-3g";

	char key[] = "\"IP\",";

	find_string_in_file_nl(arq1,&key,apn,100);

	return 1;

}


/**
 * Grava o APN - Acess Point Name, no arquivo de script - ARQ1,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * APN é passado por parâmetro.
 *
 * Retorna 1 se sucesso.
 * Caso ocorra problema, é retornado controle de erros referente a função descrita em str.c
 *
 * @param apn
 * @return
 */
int modem3g_set_apn (char * apn){
	//arq1 == "chat-modem-3g";

	char key[] = "\"IP\",";

	replace_string_in_file_nl(arq1,&key,apn);

	return 1;

}


/**
 * Adquire o USERNAME, no arquivo de script - ARQ2,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna USERNAME por parâmetro e retorna 1 se sucesso.
 * Caso ocorra problema, é retornado controle de erros referente a função descrita em str.c
 *
 * @param username
 * @return
 */
int modem3g_get_username (char * username){
	//arq2 == "modem-3g";

	char key[] = "user";

	find_string_in_file_nl(arq2,&key,username,100);

	return 1;


}


/**
 * Grava o USERNAME, no arquivo de script - ARQ2,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * USERNAME é passado por parâmetro.
 *
 * Retorna 1 se sucesso.
 * Caso ocorra problema, é retornado controle de erros referente a função descrita em str.c
 *
 * @param username
 * @return
 */
int modem3g_set_username (char * username){
	//arq2 == "modem-3g";

	char key[] = "user";

	replace_string_in_file_nl(arq2,&key,username);

	return 1;

}


/**
 * Adquire o PASSWORD, no arquivo de script - ARQ2,
 * através da função find_string_in_file_nl, descrita em str.c
 *
 * Retorna PASSWORD por parâmetro e retorna 1 se sucesso.
 * Caso ocorra problema, é retornado controle de erros referente a função descrita em str.c
 *
 * @param password
 * @return
 */
int modem3g_get_password (char * password){
	//arq2 == "modem-3g";

	char key[] = "password";

	find_string_in_file_nl(arq2,&key,password,100);

	return 1;

}


/**
 * Grava o PASSWORD, no arquivo de script - ARQ2,
 * através da função replace_string_in_file_nl, descrita em str.c.
 * PASSWORD é passado por parâmetro.
 *
 * Retorna 1 se sucesso.
 * Caso ocorra problema, é retornado controle de erros referente a função descrita em str.c
 *
 * @param password
 * @return
 */
int modem3g_set_password (char * password){
	//arq2 == "modem-3g";

	char key[] = "password";

	replace_string_in_file_nl(arq2,&key,password);

	return 1;

}
