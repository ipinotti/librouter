/*
 * pppoe.c
 *
 *  Created on: Dec 10, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>

#include "options.h"

#ifdef OPTION_PPPOE
#include "pppoe.h"
#include "args.h"
#include "defines.h"
#include "device.h"
#include "error.h"
#include "exec.h"
#include "str.h"

/**
 * pppoe_destroy_args_conffile		Free args list created from file read
 *
 * @param args
 * @param lines
 * @return 0
 */
static int pppoe_destroy_args_conffile(arglist * args[], int lines)
{
	int i=0;
	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	return 0;
}

/**
 * pppoe_read_conffile	Função utilizada para armazenar as informações
 * de configuração dos arquivos do PPPOE em arglist.
 *
 * @param args
 * @param conffile
 * @return Retorna número de linhas que foram lidas do arquivo
 */
static int pppoe_read_conffile(arglist * args[], char * conffile)
{
	FILE *f;
	int lines = -1;
	char line[200];

	if ((f = fopen(conffile, "r")) != NULL) {
		lines = 0;
		while (fgets(line, 200, f) && lines < MAX_LINES_PPPOE) {
			librouter_str_striplf(line);
			if (strlen(line)) {
				args[lines] = librouter_make_args(line);
				lines++;
			}
		}
		fclose(f);
	}
	return lines;
}

/**
 * pppoe_update_args_conffile		Function frees the args list and re-read the config file
 *
 * @param args
 * @param conffile
 * @param lines
 * @return total lines of the config file if ok, -1 if not
 */
static int pppoe_update_args_conffile(arglist * args[], char * conffile, int lines)
{
	int i=0;

	pppoe_destroy_args_conffile(args, lines);

	return pppoe_read_conffile(args, conffile);
}

/**
 * pppoe_search_for_target_infile	Função procura por target em arglist, e
 * retorna linha em que se encontra o mesmo
 *
 * @param args
 * @param lines
 * @param target
 * @return line if ok, -1 if not
 */
static int pppoe_search_for_target_infile(arglist * args[], int lines, char * target)
{
	int i,j;

	for (i = 0; i < lines; i++)
			for (j = 0; j < args[i]->argc; j++)
				if (!strcmp(args[i]->argv[j],target))
					return i;

	return -1;
}

/**
 * pppoe_write_conffile		Função edita arquivo de conf atraves da args list,
 * valor a gravar, linha (l) e posição do elemento da arglist (c) para ser modificado
 *
 * @param args
 * @param conffile
 * @param lines
 * @param l
 * @param c
 * @param value
 * @return 0 if ok, -1 if not
 */
static int pppoe_write_conffile(arglist * args[], char * conffile, int lines, int l, int c, char * value)
{
	FILE *f;
	int i,j;

	/*edit conf file to the deserved config*/
	if ((f = fopen(conffile, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			for (j = 0; j < args[i]->argc; j++) {
					if (j < args[i]->argc - 1){
						if ( (i == l) && (c == j))
							fprintf(f, "%s ", value);
						else
							fprintf(f, "%s ", args[i]->argv[j]);
					}
					else{
						if ( (i == l) && (c == j))
							fprintf(f, "%s\n", value);
						else
							fprintf(f, "%s\n", args[i]->argv[j]);
					}
			}
		}
	}

	fclose(f);

	/* update args with new info*/
	if ( (lines = pppoe_update_args_conffile(args,conffile,lines)) < 0 )
		return -1;

	return 0;
}

/**
 * pppoe_set_service_name		Writes the desired service name in the conf file (tunnel_pppoe)
 * @param server
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_service_name(char * service_name)
{
	arglist *args[MAX_LINES_PPPOE];
	int i, lines = 0, add = -1;
	char buff_service_name[DEF_SIZE_FIELDS_PPPOE+2];

	if (service_name == NULL || (strlen(service_name) == 0 ))
		add = 0;
	else{
		add = 1;
		snprintf(buff_service_name,sizeof(buff_service_name),"\'%s\'",service_name);

	}

	/*read conf file PPPOE_TUNNEL to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_TUNNEL*/
	for (i = 0; i < lines; i++) {
		if ( strstr(args[i]->argv[0], "rp_pppoe_service") ){
			if (add){
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,0,"rp_pppoe_service");
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,1,buff_service_name);

			}
			else{
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,0,"#rp_pppoe_service");
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,1,"'pppoe_service'");
			}
		}
	}

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_ac_name		Writes the desired access contractor name in the conf file (tunnel_pppoe)
 * @param server
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_ac_name(char * ac_name)
{
	arglist *args[MAX_LINES_PPPOE];
	int i, lines = 0, add = -1;
	char buff_ac_name[DEF_SIZE_FIELDS_PPPOE+2];

	if (ac_name == NULL || (strlen(ac_name) == 0 ))
		add = 0;
	else{
		add = 1;
		snprintf(buff_ac_name,sizeof(buff_ac_name),"\'%s\'",ac_name);

	}

	/*read conf file PPPOE_TUNNEL to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_TUNNEL*/
	for (i = 0; i < lines; i++) {
		if ( strstr(args[i]->argv[0], "rp_pppoe_ac") ){
			if (add){
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,0,"rp_pppoe_ac");
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,1,buff_ac_name);

			}
			else{
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,0,"#rp_pppoe_ac");
				pppoe_write_conffile(args,PPPOE_TUNNEL,lines,i,1,"'pppoe_ac'");
			}
		}
	}

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_network_chapsecrets		Writes the desired network domain in conf file (chapsecrets)
 *
 * @param network
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_network_chapsecrets(char * network)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe=0, lines = 0, empty=0;
	char * buff_network;
	char * buff_username;
	char buff[256]="";

	if (network == NULL || (strlen(network) == 0 ))
		empty = 1;

	/*read conf file PPPOE_CHAPSECRETS to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_CHAPSECRETS*/

	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	if ( strstr(args[line_pppoe]->argv[0], "@")  ){
		buff_username = strtok(args[line_pppoe]->argv[0],"@");
		buff_network = strtok(NULL,"@");

		if (!empty)
			sprintf(buff,"%s@%s",buff_username,network);
		else
			sprintf(buff,"%s",buff_username);

		pppoe_write_conffile(args,PPPOE_CHAPSECRETS,lines,line_pppoe,0,buff);
	}
	else{
		if (!empty){
			sprintf(buff,"%s@%s",args[line_pppoe]->argv[0],network);
			pppoe_write_conffile(args,PPPOE_CHAPSECRETS,lines,line_pppoe,0,buff);
		}
	}

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_network_papsecrets		Writes the desired network domain in conf file (papsecrets)
 *
 * @param network
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_network_papsecrets(char * network)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe=0, lines = 0, empty=0;
	char * buff_network;
	char * buff_username;
	char buff[256]="";

	if (network == NULL || (strlen(network) == 0 ))
		empty = 1;

	/*read conf file PPPOE_PAPSECRETS to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_PAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_PAPSECRETS*/

	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	if ( strstr(args[line_pppoe]->argv[0], "@")  ){
		buff_username = strtok(args[line_pppoe]->argv[0],"@");
		buff_network = strtok(NULL,"@");

		if (!empty)
			sprintf(buff,"%s@%s",buff_username,network);
		else
			sprintf(buff,"%s",buff_username);

		pppoe_write_conffile(args,PPPOE_PAPSECRETS,lines,line_pppoe,0,buff);
	}
	else{
		if (!empty){
			sprintf(buff,"%s@%s",args[line_pppoe]->argv[0],network);
			pppoe_write_conffile(args,PPPOE_PAPSECRETS,lines,line_pppoe,0,buff);
		}
	}

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}


/**
 * pppoe_set_network_tunnel		Writes the desired network in conf file (pppoe_tunnel)
 *
 *
 * @param network
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_network_tunnel(char * network)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_name = 0, lines = 0, empty = 0;
	char * buff_network;
	char * buff_username;
	char buff[256]="";

	if (network == NULL || (strlen(network) == 0 ))
		empty = 1;

	/*read conf file PPPOE_TUNNEL to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_TUNNEL*/
	if ( (line_name = pppoe_search_for_target_infile(args,lines,"name")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	if ( strstr(args[line_name]->argv[1], "@")  ){
		buff_username = strtok(args[line_name]->argv[1],"@");
		buff_network = strtok(NULL,"@");

		if (!empty)
			sprintf(buff,"%s@%s",buff_username,network);
		else
			sprintf(buff,"%s",buff_username);

		pppoe_write_conffile(args,PPPOE_TUNNEL,lines,line_name,1,buff);
	}
	else{
		if (!empty){
			sprintf(buff,"%s@%s",args[line_name]->argv[1], network);
			pppoe_write_conffile(args,PPPOE_TUNNEL,lines,line_name,1,buff);
		}
	}

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_network		Set the desired network in all conf files
 *
 * @param network
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_network(char * network)
{
	if (pppoe_set_network_chapsecrets(network) < 0)
		return -1;

	if (pppoe_set_network_papsecrets(network) < 0)
		return -1;

	if (pppoe_set_network_tunnel(network) < 0)
		return -1;

	return 0;
}

/**
 * pppoe_set_username_chapsecrets		Writes the desired username in conf file (chapsecrets)
 *
 * @param username
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_username_chapsecrets(char * username)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe = 0,lines = 0;
	char * buff_username;
	char * buff_network;
	char buff[256]="";

	if (username == NULL || (strlen(username) == 0 ))
		return -1;

	/*read conf file PPPOE_CHAPSECRETS to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_CHAPSECRETS*/
	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	if ( strstr(args[line_pppoe]->argv[0], "@")  ){
		buff_username = strtok(args[line_pppoe]->argv[0],"@");
		buff_network = strtok(NULL,"@");
		sprintf(buff,"%s@%s",username,buff_network);
		pppoe_write_conffile(args,PPPOE_CHAPSECRETS,lines,line_pppoe,0,buff);
	}
	else
		pppoe_write_conffile(args,PPPOE_CHAPSECRETS,lines,line_pppoe,0,username);

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_username_papsecrets		Writes the desired username in conf file (papsecrets)
 *
 * @param username
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_username_papsecrets(char * username)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe = 0,lines = 0;
	char * buff_username;
	char * buff_network;
	char buff[256]="";

	if (username == NULL || (strlen(username) == 0 ))
		return -1;

	/*read conf file PPPOE_CHAPSECRETS to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_PAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_CHAPSECRETS*/
	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	if ( strstr(args[line_pppoe]->argv[0], "@")  ){
		buff_username = strtok(args[line_pppoe]->argv[0],"@");
		buff_network = strtok(NULL,"@");
		sprintf(buff,"%s@%s",username,buff_network);
		pppoe_write_conffile(args,PPPOE_PAPSECRETS,lines,line_pppoe,0,buff);
	}
	else
		pppoe_write_conffile(args,PPPOE_PAPSECRETS,lines,line_pppoe,0,username);

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_username_tunnel		Writes the desired username in conf file (pppoe_tunnel)
 *
 *
 * @param username
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_username_tunnel(char * username)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_name = 0, lines = 0;
	char * buff_network;
	char * buff_username;
	char buff[256]="";

	if (username == NULL || (strlen(username) == 0 ))
		return -1;

	/*read conf file PPPOE_TUNNEL to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_TUNNEL*/
	if ( (line_name = pppoe_search_for_target_infile(args,lines,"name")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	if ( strstr(args[line_name]->argv[1], "@")  ){
		buff_username = strtok(args[line_name]->argv[1],"@");
		buff_network = strtok(NULL,"@");
		sprintf(buff,"%s@%s",username,buff_network);
		pppoe_write_conffile(args,PPPOE_TUNNEL,lines,line_name,1,buff);
	}
	else
		pppoe_write_conffile(args,PPPOE_TUNNEL,lines,line_name,1,username);

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_username		Set username in all conf files
 *
 * @param username
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_username(char * username)
{

	if (pppoe_set_username_chapsecrets(username) < 0)
		return -1;

	if (pppoe_set_username_papsecrets(username)< 0)
		return -1;

	if (pppoe_set_username_tunnel(username) < 0)
		return -1;

	return 0;

}

/**
 * pppoe_set_password_chapsecrets		Writes the desired password in conf file (chapsecrets)
 *
 * @param password
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_password_chapsecrets(char * password)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe = 0,lines = 0;
	char buff[DEF_SIZE_FIELDS_PPPOE+2]="";

	/*read conf file to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	snprintf(buff,DEF_SIZE_FIELDS_PPPOE+2,"\"%s\"",password);

	/*modify info*/
	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	pppoe_write_conffile(args,PPPOE_CHAPSECRETS,lines,line_pppoe,2,buff);

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_password_papsecrets		Writes the desired password in conf file (chapsecrets)
 *
 * @param password
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_password_papsecrets(char * password)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe = 0,lines = 0;
	char buff[DEF_SIZE_FIELDS_PPPOE+2]="";

	/*read conf file to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_PAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	snprintf(buff,DEF_SIZE_FIELDS_PPPOE+2,"\"%s\"",password);

	/*modify info*/
	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	pppoe_write_conffile(args,PPPOE_PAPSECRETS,lines,line_pppoe,2,buff);

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_set_password		Set password in all conf files
 *
 * @param password
 * @return 0 if ok, -1 if not
 */
static int pppoe_set_password(char * password)
{
	if (pppoe_set_password_chapsecrets(password) < 0)
		return -1;

	if (pppoe_set_password_papsecrets(password) < 0)
		return -1;

	return 0;
}

/**
 * librouter_pppoe_analyze_input		Analyze if the input is good to go,
 * avoiding invalids characters such as " <>#\\/&* "
 *
 * @param input
 * @return 0 if ok, -1 if not
 */
int librouter_pppoe_analyze_input(char * input)
{
	char invalids[] = "<>#\\/&*";

	if ( strpbrk(input, invalids) == NULL )
		return 0;
	else
		return -1;
}

/**
 * pppoe_get_username		Get username from conf file (chapsecrets)
 *
 * @param username
 * @return 0 if ok, -1 if not
 */
static int pppoe_get_username(char * username)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe = 0, lines = 0;
	char * buff_network;
	char * buff_username;

	lines = pppoe_read_conffile(args, PPPOE_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	if ( strstr(args[line_pppoe]->argv[0], "@")  ){
		buff_username = strtok(args[line_pppoe]->argv[0],"@");
		buff_network = strtok(NULL,"@");
		strcpy(username,buff_username);
	}
	else
		strcpy(username,args[line_pppoe]->argv[0]);

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_get_password		Get password from conf file (chapsecrets)
 *
 * @param password
 * @return 0 if ok, -1 if not
 */
static int pppoe_get_password(char * password)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe = 0,lines = 0;
	char buff[DEF_SIZE_FIELDS_PPPOE+2]="";
	char *p="";

	/*read conf file to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0 ){
		pppoe_destroy_args_conffile(args, lines);
		return -1;
	}

	strncpy(buff,args[line_pppoe]->argv[2],strlen(args[line_pppoe]->argv[2]));

	while ((p = strchr(buff,'\"')) != NULL)
		memmove(p, p+1, strlen(p));

	strcpy(password,buff);

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_get_network		Get network from conf file (chapsecrets)
 *
 * @param network
 * @return 0 if ok, -1 if not
 */
static int pppoe_get_network(char * network)
{
	arglist *args[MAX_LINES_PPPOE];
	int line_pppoe = 0, lines = 0;
	char * buff_username;
	char * buff_network;

	/*read conf file PPPOE_CHAPSECRETS to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	if ( (line_pppoe = pppoe_search_for_target_infile(args,lines,"PPPOE")) < 0 )
		return -1;

	if ( strstr(args[line_pppoe]->argv[0], "@")  ){
		buff_username = strtok(args[line_pppoe]->argv[0],"@");
		buff_network = strtok(NULL,"@");
		strcpy(network,buff_network);
	}
	else
		strcpy(network,"");

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_get_server		Get server from conf file (pppoe_tunnel)
 *
 * @param service_name
 * @return 0 if ok, -1 if not
 */
static int pppoe_get_service_name(char * service_name)
{
	arglist *args[MAX_LINES_PPPOE];
	int i, lines = 0;
	char buff[DEF_SIZE_FIELDS_PPPOE+2]="";
	char *p="";

	/*read conf file PPPOE_TUNNEL to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_TUNNEL*/
	for (i = 0; i < lines; i++) {
		if ( strstr(args[i]->argv[0], "rp_pppoe_service") ){
			if (strpbrk(args[i]->argv[0],"#"))
				strcpy(service_name,"");
			else{
				strncpy(buff,args[i]->argv[1],strlen(args[i]->argv[1]));

				while ((p = strchr(buff,'\'')) != NULL)
					memmove(p, p+1, strlen(p));

				strcpy(service_name,buff);
			}
			break;
		}
	}

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * pppoe_get_ac_name		Get access contractor name from conf file (pppoe_tunnel)
 *
 * @param ac_name
 * @return 0 if ok, -1 if not
 */
static int pppoe_get_ac_name(char * ac_name)
{
	arglist *args[MAX_LINES_PPPOE];
	int i, lines = 0;
	char buff[DEF_SIZE_FIELDS_PPPOE+2]="";
	char *p="";

	/*read conf file PPPOE_TUNNEL to retrieve info*/
	lines = pppoe_read_conffile(args, PPPOE_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPPOE");
		return -1;
	}

	/*modify info for file PPPOE_TUNNEL*/
	for (i = 0; i < lines; i++) {
		if ( strstr(args[i]->argv[0], "rp_pppoe_ac") ){
			if (strpbrk(args[i]->argv[0],"#"))
				strcpy(ac_name,"");
			else{
				strncpy(buff,args[i]->argv[1],strlen(args[i]->argv[1]));

				while ((p = strchr(buff,'\'')) != NULL)
					memmove(p, p+1, strlen(p));

				strcpy(ac_name,buff);
			}
			break;
		}
	}

	pppoe_destroy_args_conffile(args, lines);

	return 0;
}

/**
 * librouter_pppoe_get_config		Get all configs by pppoe_config struct
 *
 * @param pppoe_conf
 * @return 0 if ok, -1 if not
 */
int librouter_pppoe_get_config(pppoe_config * pppoe_conf)
{
	memset(pppoe_conf, 0, sizeof(pppoe_conf));

	if (pppoe_get_service_name(pppoe_conf->service_name) < 0)
		return -1;

	if (pppoe_get_ac_name(pppoe_conf->ac_name) < 0)
		return -1;

	if (pppoe_get_username(pppoe_conf->username) < 0)
		return -1;

	if (pppoe_get_password(pppoe_conf->password) < 0)
		return -1;

	if (pppoe_get_network(pppoe_conf->network) < 0)
		return -1;

	return 0;
}

/**
 * librouter_pppoe_set_config		Set all configs by pppoe_config struct
 *
 * @param pppoe_cfg
 * @return 0 if ok, -1 if not
 */
int librouter_pppoe_set_config(pppoe_config * pppoe_cfg) /*TODO*/
{

}

/**
 * librouter_pppoe_set_config_cli		Set configs based on field (username,
 * password,network,server) and its desired value
 *
 * @param field
 * @param value
 * @return 0 if ok, -1 if not
 */
int librouter_pppoe_set_config_cli(char * field, char * value)
{
	char key;
	key = field[0];
	int check=0;

	switch(key){
		case 's':
			check = pppoe_set_service_name(value);
			break;

		case 'a':
			check = pppoe_set_ac_name(value);
			break;

		case 'u':
			check = pppoe_set_username(value);
			break;

		case 'p':
			check = pppoe_set_password(value);
			break;

		case 'n':
			check = pppoe_set_network(value);
			break;
		default:
			check = -1;
			break;

	}

	return check;
}



/**
 * pppoe_clientmode_on		Execute PPP (PPPOE) program in inittab
 *
 * @return 0
 */
static int pppoe_clientmode_on(void)
{
	if (!librouter_exec_check_daemon(PPPOE_PPP)){
		librouter_exec_daemon(PPPOE_PPP);
	}
	return 0;
}

/**
 * pppoe_clientmode_off		Kill PPP (PPPOE) program
 *
 * @return 0
 */
static int pppoe_clientmode_off(void)
{
	if (librouter_exec_check_daemon(PPPOE_PPP)){
		librouter_kill_daemon(PPPOE_PPP);
	}
	return 0;
}

/**
 * librouter_pppoe_set_clientmode		Enable/disable PPPOE connection (client mode)
 *
 * @param add
 * @return 0
 */
int librouter_pppoe_set_clientmode(int add)
{
	if (add)
		pppoe_clientmode_on();
	else
		pppoe_clientmode_off();

	return 0;
}

/**
 * librouter_pppoe_get_clientmode		Get status of PPPOE Client Mode (on/off)
 *
 * @return 1 if enabled, 0 if disabled
 */
int librouter_pppoe_get_clientmode(void)
{
	if (librouter_exec_check_daemon(PPPOE_PPP))
		return 1;
	else
		return 0;
}

/**
 * librouter_pppoe_dump		Dump configuration setted for cish
 *
 * @param out
 * @return 0
 */
int librouter_pppoe_dump(FILE *out)
{
	pppoe_config pppoe_cfg;
	librouter_pppoe_get_config(&pppoe_cfg);

	if (strlen(pppoe_cfg.service_name) > 0 )
		fprintf(out, " service-name %s\n",pppoe_cfg.service_name);
	else
		fprintf(out, " no service-name\n");

	if (strlen(pppoe_cfg.ac_name) > 0 )
		fprintf(out, " ac-name %s\n",pppoe_cfg.ac_name);
	else
		fprintf(out, " no ac-name\n");

	fprintf(out, " username %s\n",pppoe_cfg.username);

	fprintf(out, " password %s\n",pppoe_cfg.password);

	if (strlen(pppoe_cfg.network) > 0 )
		fprintf(out, " network %s\n",pppoe_cfg.network);
	else
		fprintf(out, " no network\n");

	if (librouter_pppoe_get_clientmode())
		fprintf (out, " client-mode\n");
	else
		fprintf (out, " no client-mode\n");

	return 0;

}
#endif /* OPTION_PPPOE */

