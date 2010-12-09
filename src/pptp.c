/*
 * pptp.c
 *
 *  Created on: Dec 2, 2010
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

#include "pptp.h"
#include "options.h"
#include "args.h"
#include "defines.h"
#include "device.h"
#include "error.h"
#include "exec.h"
#include "str.h"

static int pptp_destroy_args_conffile(arglist * args[], int lines)
{
	int i=0;
	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	return 0;
}

/**
 * pptp_read_conffile	Função utilizada para armazenar as informações
 * de configuração dos arquivos do PPTP em arglist.
 *
 * @param args
 * @param conffile
 * @return Retorna número de linhas que foram lidas do arquivo
 */
static int pptp_read_conffile(arglist * args[], char * conffile)
{
	FILE *f;
	int lines = -1;
	char line[200];

	if ((f = fopen(conffile, "r")) != NULL) {
		lines = 0;
		while (fgets(line, 200, f) && lines < MAX_LINES_PPTP) {
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

static int pptp_update_args_conffile(arglist * args[], char * conffile, int lines)
{
	int i=0;

	pptp_destroy_args_conffile(args, lines);

	return pptp_read_conffile(args, conffile);
}

static int pptp_write_conffile(arglist * args[], char * conffile, int lines, int l, int c, char * value)
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
	if ( (lines = pptp_update_args_conffile(args,conffile,lines)) < 0 )
			return -1;

	return 0;
}

static int pptp_set_server(char * server)
{
	arglist *args[MAX_LINES_PPTP];
	int i,j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info*/
	for (i = 0; i < lines; i++){
		for (j = 0; j < args[i]->argc; j++){
			if ( !strcmp(args[i]->argv[j], "pty") ){
				/*write modified info to conf file*/
				pptp_write_conffile(args,PPTP_TUNNEL,lines,i,2,server);
			}
		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_set_domain_chapsecrets(char * domain)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0, empty=0;
	char * buff_domain;
	char * buff_username;
	char buff[256]="";

	if (domain == NULL || (strlen(domain) == 0 ))
		empty = 1;

	/*read conf file PPTP_CHAPSECRETS to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info for file PPTP_CHAPSECRETS*/
	if ( strstr(args[0]->argv[0], "\\")  ){
		buff_domain = strtok(args[0]->argv[0],"\\");
		buff_username = strtok(NULL,"\\");

		if (!empty)
			sprintf(buff,"%s\\\\%s",domain,buff_username);
		else
			sprintf(buff,"%s",buff_username);

		pptp_write_conffile(args,PPTP_CHAPSECRETS,lines,0,0,buff);
	}
	else{
		if (!empty){
			sprintf(buff,"%s\\\\%s",domain,args[0]->argv[0]);
			pptp_write_conffile(args,PPTP_CHAPSECRETS,lines,0,0,buff);
		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_set_domain_tunnel(char * domain)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0, empty = 0;
	char * buff_domain;
	char * buff_username;
	char buff[256]="";

	if (domain == NULL || (strlen(domain) == 0 ))
		empty = 1;

	/*read conf file PPTP_TUNNEL to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info for file PPTP_TUNNEL*/
	for (i = 0; i < lines; i++) {
		if ( !strcmp(args[i]->argv[0], "name") ){
			if ( strstr(args[i]->argv[1], "\\")  ){
				buff_domain = strtok(args[i]->argv[1],"\\");
				buff_username = strtok(NULL,"\\");

				if (!empty)
					sprintf(buff,"%s\\\\%s",domain,buff_username);
				else
					sprintf(buff,"%s",buff_username);

				pptp_write_conffile(args,PPTP_TUNNEL,lines,i,1,buff);
			}
			else{
				if (!empty){
					sprintf(buff,"%s\\\\%s",domain,args[i]->argv[1]);
					pptp_write_conffile(args,PPTP_TUNNEL,lines,i,1,buff);
				}
			}
		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_set_domain(char * domain)
{
	if (pptp_set_domain_chapsecrets(domain) < 0)
		return -1;

	if (pptp_set_domain_tunnel(domain) < 0)
		return -1;

	return 0;
}

static int pptp_set_username_chapsecrets(char * username)
{
	arglist *args[MAX_LINES_PPTP];
	int i,lines = 0;
	char * buff_domain;
	char buff[256]="";

	if (username == NULL || (strlen(username) == 0 ))
		return -1;

	/*read conf file PPTP_CHAPSECRETS to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info for file PPTP_CHAPSECRETS*/
	if ( strstr(args[0]->argv[0], "\\")  ){
		buff_domain = strtok(args[0]->argv[0],"\\");
		sprintf(buff,"%s\\\\%s",buff_domain,username);
		pptp_write_conffile(args,PPTP_CHAPSECRETS,lines,0,0,buff);
	}
	else
		pptp_write_conffile(args,PPTP_CHAPSECRETS,lines,0,0,username);

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_set_username_tunnel(char * username)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0;
	char * buff_domain;
	char buff[256]="";

	if (username == NULL || (strlen(username) == 0 ))
		return -1;

	/*read conf file PPTP_TUNNEL to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info for file PPTP_TUNNEL*/
	for (i = 0; i < lines; i++) {
		if ( !strcmp(args[i]->argv[0], "name") ){
			if ( strstr(args[i]->argv[1], "\\")  ){
				buff_domain = strtok(args[i]->argv[1],"\\");
				sprintf(buff,"%s\\\\%s",buff_domain,username);
				pptp_write_conffile(args,PPTP_TUNNEL,lines,i,1,buff);
			}
			else
				pptp_write_conffile(args,PPTP_TUNNEL,lines,i,1,username);

		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_set_username(char * username)
{

	if (pptp_set_username_chapsecrets(username) < 0)
		return -1;

	if (pptp_set_username_tunnel(username) < 0)
		return -1;

	return 0;

}

static int pptp_set_password(char * password)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0;
	char buff[DEF_SIZE_FIELDS_PPTP+2]="";

	/*read conf file to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	snprintf(buff,DEF_SIZE_FIELDS_PPTP+2,"\"%s\"",password);

	/*modify info*/
	pptp_write_conffile(args,PPTP_CHAPSECRETS,lines,0,2,buff);

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

int librouter_pptp_analyze_input(char * input)
{
	char invalids[] = ".@<>#\\/&*";

	if ( strpbrk(input, invalids) == NULL )
		return 0;
	else
		return -1;
}

static int pptp_get_username(char * username)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0;
	char * buff_domain;
	char * buff_username;

	lines = pptp_read_conffile(args, PPTP_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	if ( strstr(args[0]->argv[0], "\\")  ){
		buff_domain = strtok(args[0]->argv[0],"\\");
		buff_username = strtok(NULL,"\\");
		strcpy(username,buff_username);
	}
	else
		strcpy(username,args[0]->argv[0]);

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_get_password(char * password)
{
	arglist *args[MAX_LINES_PPTP];
	int lines = 0;
	char buff[DEF_SIZE_FIELDS_PPTP+2]="";
	char *p="";

	/*read conf file to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	strncpy(buff,args[0]->argv[2],strlen(args[0]->argv[2]));

	while ((p = strchr(buff,'\"')) != NULL)
		memmove(p, p+1, strlen(p));

	strcpy(password,buff);

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_get_domain(char * domain)
{
	arglist *args[MAX_LINES_PPTP];
	int lines = 0;
	char * buff_domain;

	/*read conf file PPTP_CHAPSECRETS to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_CHAPSECRETS);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	if ( strstr(args[0]->argv[0], "\\")  ){
		buff_domain = strtok(args[0]->argv[0],"\\");
		strcpy(domain,buff_domain);
	}
	else
		strcpy(domain,"");

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_get_server(char * server)
{
	arglist *args[MAX_LINES_PPTP];
	int i,j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	for (i = 0; i < lines; i++){
		for (j = 0; j < args[i]->argc; j++){
			if ( !strcmp(args[i]->argv[j], "pty") ){
				strcpy(server,args[i]->argv[2]);
			}
		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_get_mppe(void)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0, mppe=-1;

	lines = pptp_read_conffile(args, PPTP_OPTIONSPPTP);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	for (i = 0; i < lines; i++) {
		if ( !strcmp(args[i]->argv[0], "require-mppe-128") )
			mppe = 1;
		else
			mppe = 0;
	}

	pptp_destroy_args_conffile(args, lines);

	return mppe;

}
int librouter_pptp_get_config(pptp_config * pptp_conf)
{

	memset(pptp_conf, 0, sizeof(pptp_conf));

	if (pptp_get_username(pptp_conf->username) < 0)
		return -1;

	if (pptp_get_password(pptp_conf->password) < 0)
		return -1;

	if (pptp_get_domain(pptp_conf->domain) < 0)
		return -1;

	if (pptp_get_server(pptp_conf->server) < 0)
		return -1;

	if ( (pptp_conf->mppe = pptp_get_mppe()) < 0)
		return -1;

	return 0;
}

int librouter_pptp_set_config(pptp_config * pptp_cfg) /*TODO*/
{

}

int librouter_pptp_set_config_cli(char * field, char * value)
{
	char key;
	key = field[0];
	int check=0;

	switch(key){
		case 's':
			check = pptp_set_server(value);
			break;

		case 'd':
			check = pptp_set_domain(value);
			break;

		case 'u':
			check = pptp_set_username(value);
			break;

		case 'p':
			check = pptp_set_password(value);
			break;

		default:
			check = -1;
			break;

	}

	return check;
}

static int pptp_set_mppe_tunnel(int add)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0;

	/*read conf file PPTP_TUNNEL to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_TUNNEL);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info for file PPTP_TUNNEL*/
	for (i = 0; i < lines; i++) {
		if ( strstr(args[i]->argv[0], "require-mppe-128") ){
			if (add)
				pptp_write_conffile(args,PPTP_TUNNEL,lines,i,0,"require-mppe-128");
			else
				pptp_write_conffile(args,PPTP_TUNNEL,lines,i,0,"#require-mppe-128");
		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_set_mppe_options(int add)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0;

	/*read conf file PPTP_OPTIONSPPTP to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_OPTIONSPPTP);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info for file PPTP_OPTIONSPPTP*/
	for (i = 0; i < lines; i++) {
		if ( strstr(args[i]->argv[0], "require-mppe-128") ){
			if (add)
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"require-mppe-128");
			else
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"#require-mppe-128");
		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

static int pptp_set_authentication_protocol(int add)
{
	arglist *args[MAX_LINES_PPTP];
	int i, lines = 0;

	/*read conf file PPTP_OPTIONSPPTP to retrieve info*/
	lines = pptp_read_conffile(args, PPTP_OPTIONSPPTP);
	if (lines < 0){
		syslog(LOG_ERR,"No file to retrieve info about PPTP");
		return -1;
	}

	/*modify info for file PPTP_OPTIONSPPTP*/
	for (i = 0; i < lines; i++) {
		if ( strstr(args[i]->argv[0], "refuse-pap") ){
			if (add)
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"refuse-pap");
			else
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"#refuse-pap");
			continue;
		}
		if ( strstr(args[i]->argv[0], "refuse-eap") ){
			if (add)
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"refuse-eap");
			else
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"#refuse-eap");
			continue;
		}
		if ( strstr(args[i]->argv[0], "refuse-chap") ){
			if (add)
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"refuse-chap");
			else
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"#refuse-chap");
			continue;
		}
		if ( strstr(args[i]->argv[0], "refuse-mschap") ){
			if (add)
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"refuse-mschap");
			else
				pptp_write_conffile(args,PPTP_OPTIONSPPTP,lines,i,0,"#refuse-mschap");
		}
	}

	pptp_destroy_args_conffile(args, lines);

	return 0;
}

int librouter_pptp_set_mppe(int add)
{
	if (pptp_set_mppe_options(add) < 0)
		return -1;

	if (pptp_set_mppe_tunnel(add) < 0)
		return -1;

	if (pptp_set_authentication_protocol(add) < 0)
		return -1;

	return 0;
}

static int pptp_clientmode_on(void)
{
	if (!librouter_exec_check_daemon(PPTP_PPP)){
		librouter_exec_daemon(PPTP_PPP);
	}
}

static int pptp_clientmode_off(void)
{
	if (librouter_exec_check_daemon(PPTP_PPP)){
		librouter_kill_daemon(PPTP_PPP);
	}
}

int librouter_pptp_set_clientmode(int add)
{
	if (add)
		pptp_clientmode_on();
	else
		pptp_clientmode_off();

	return 0;
}

int librouter_pptp_get_clientmode(void)
{
	if (librouter_exec_check_daemon(PPTP_PPP))
		return 1;
	else
		return 0;
}

int librouter_pptp_dump(FILE *out)
{
	pptp_config pptp_cfg;
	librouter_pptp_get_config(&pptp_cfg);

	fprintf(out, "pptp username %s\n",pptp_cfg.username);

	fprintf(out, "pptp password %s\n",pptp_cfg.password);

	if (strlen(pptp_cfg.domain) > 0 )
		fprintf(out, "pptp domain %s\n",pptp_cfg.domain);

	if (pptp_cfg.mppe)
		fprintf(out, "pptp mppe\n");
	else
		fprintf(out, "no pptp mppe\n");

	fprintf(out, "pptp server %s\n", pptp_cfg.server);

	if (librouter_pptp_get_clientmode())
		fprintf (out, "pptp client-mode\n");
	else
		fprintf (out, "no pptp client-mode\n");

	return 0;

}

