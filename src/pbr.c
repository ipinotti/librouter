/*
 * pbr.c
 *
 *  Created on: Feb 2, 2011
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "args.h"
#include "ip.h"
#include "pbr.h"
#include "str.h"

#if defined (OPTION_PBR)
/**
 * librouter_pbr_rule_add	Função adiciona regra baseada em FWMARK nas tabelas disponiveis (table0-9)
 *
 * @param mark_no
 * @param table
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_rule_add(int mark_no, char * table)
{
	char cmd[128];

#ifdef NOT_YET_IMPLEMENTED
//TODO
//	if (librouter_mangle_rule_exists_by_num(mark_no)) {
//
//	}
#endif

	sprintf(cmd, "/bin/ip rule add fwmark %d table %s ", mark_no,table);

	pbr_dbgs("Applying PBR rule: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	return 0;
}

int librouter_pbr_verify_rule_exist_in_log_entry(int mark_no, int table_no)
{
	char cmd_verify[128];

	sprintf(cmd_verify," rule %d table %d", mark_no, table_no);
	return (librouter_str_find_string_in_file_return_stat(PBR_RULES_ENTRY_FILE_CONTROL, cmd_verify));
}

/**
 * librouter_pbr_rule_del	Função remove regra baseada em FWMARK nas tabelas disponiveis (table0-9)
 *
 * @param mark_no
 * @param table
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_rule_del(int mark_no, char * table)
{
	char cmd[128];

#ifdef NOT_YET_IMPLEMENTED
//TODO
//	if (librouter_mangle_rule_exists_by_num(mark_no)) {
//
//	}
#endif

	sprintf(cmd, "/bin/ip rule del fwmark %d table %s ", mark_no, table);

	pbr_dbgs("Applying PBR rule: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	return 0;
}

/**
 *  librouter_pbr_get_show_rules_cli_size	Função fornece uma estimativa de tamanho
 *  para ser alocado no buffer que irá receber o show das regras do PBR. O tamanho se basea nas linhas,
 *  sendo 128 caracteres na linha multiplicados pelo número de linhas do show.
 *
 * @return Size of show rules (int)
 */
int librouter_pbr_get_show_rules_cli_size(void)
{
	FILE *pbr_show_file;
	char line[128];
	int lines_file=0;

	if (!(pbr_show_file = popen("/bin/ip rule show", "r"))) {
		fprintf(stderr, "%% Policy-Route subsystem not found\n");
		pclose(pbr_show_file);
		return -1;
	}

	while (fgets(line, sizeof(line), pbr_show_file) != NULL)
		lines_file++;

	pclose(pbr_show_file);
	pbr_show_file = NULL;

	return (sizeof(line)*lines_file);
}

/**
 * librouter_pbr_get_show_rules_cli	Função adquire a lista de regras do PBR e sistema
 *
 * @param pbr_show_rules_buff
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_get_show_rules_cli(char * pbr_show_rules_buff)
{
	FILE *pbr_show_file;
	char line[128];
	int check = 0;

	if (!(pbr_show_file = popen("/bin/ip rule show", "r"))) {
		fprintf(stderr, "%% Policy-Route subsystem not found\n");
		check = -1;
		goto end;
	}

	while (fgets(line, sizeof(line), pbr_show_file) != NULL){
		strcat(pbr_show_rules_buff,line);
	};

	if (pbr_show_rules_buff == NULL)
		check = -1;

end:
	pclose(pbr_show_file);

	return check;
}

/**
 * librouter_pbr_get_show_routes_cli	Função adquire a lista das rotas do PBR e sistema
 *
 * @param table
 * @param pbr_show_routes_buff
 * @return 1 if ok, -1 if not
 */
int librouter_pbr_get_show_routes_cli(char * table, char * pbr_show_routes_buff)
{
	FILE *pbr_show_file;
	char line[128];
	char file_cmd[26];
	int lines_file=0, check=0;

	sprintf(file_cmd,"/bin/ip route show table %s",table);

	if (!(pbr_show_file = popen(file_cmd, "r"))) {
		fprintf(stderr, "%% Policy-Route subsystem not found\n");
		check = -1;
		goto end;
	}

	while (fgets(line, sizeof(line), pbr_show_file) != NULL)
		strcat(pbr_show_routes_buff,line);

	if (pbr_show_routes_buff == NULL)
		check = -1;

end:
	pclose(pbr_show_file);

	return check;
}

/**
 *  librouter_pbr_get_show_routes_cli_size	Função fornece uma estimativa de tamanho
 *  para ser alocado no buffer que irá receber o show das rotas do PBR. O tamanho se basea nas linhas,
 *  sendo 128 caracteres na linha multiplicados pelo número de linhas do show.
 *
 * @param table
 * @return 1 if ok, -1 if not
 */
int librouter_pbr_get_show_routes_cli_size(char * table)
{
	FILE *pbr_show_file;
	char line[128];
	char file_cmd[26];
	int lines_file=0;

	sprintf(file_cmd,"/bin/ip route show table %s",table);

	if (!(pbr_show_file = popen(file_cmd, "r"))) {
		fprintf(stderr, "%% Policy-Route subsystem not found\n");
		pclose(pbr_show_file);
		return -1;
	}

	while (fgets(line, sizeof(line), pbr_show_file) != NULL)
		lines_file++;

	pclose(pbr_show_file);

	return (sizeof(line)*lines_file);
}

/**
 * librouter_pbr_flush_route_table	Função apaga todas as rotas de determinada tabela
 *
 * @param table
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_flush_route_table(char * table)
{
	char cmd[128];

	sprintf(cmd, "/bin/ip route flush table %s ", table);

	pbr_dbgs("Applying PBR command: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	return 0;
}

/**
 * librouter_pbr_route_add	Função adiciona rota ao sistema, em determinada tabela (table0-9)
 *
 * @param pbr
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_route_add(librouter_pbr_struct * pbr)
{
	char cmd[256];
	if (strlen(pbr->via_ipaddr) != 0){
		if (strlen(pbr->dev) !=0){
			if (strlen(pbr->network_opt_ipmask) !=0)
				sprintf(cmd, "/bin/ip route add %s/%d via %s dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr_pbr(pbr->network_opt_ipmask), pbr->via_ipaddr, pbr->dev, pbr->table);
			else
				sprintf(cmd, "/bin/ip route add %s via %s dev %s table %s ", pbr->network_opt, pbr->via_ipaddr, pbr->dev, pbr->table);
		}
		else {
			if (strlen(pbr->network_opt_ipmask) !=0)
				sprintf(cmd, "/bin/ip route add %s/%d via %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr_pbr(pbr->network_opt_ipmask), pbr->via_ipaddr, pbr->table);
			else
				sprintf(cmd, "/bin/ip route add %s via %s table %s ", pbr->network_opt, pbr->via_ipaddr, pbr->table);
		}
	}
	else{
		if (strlen(pbr->network_opt_ipmask) !=0)
			sprintf(cmd, "/bin/ip route add %s/%d dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr_pbr(pbr->network_opt_ipmask), pbr->dev, pbr->table);
		else
			sprintf(cmd, "/bin/ip route add %s dev %s table %s ", pbr->network_opt, pbr->dev, pbr->table);
	}
	pbr_dbgs("Mask entered: %s -- cidr %d \n\n",pbr->network_opt_ipmask,librouter_ip_netmask2cidr_pbr(pbr->network_opt_ipmask));
	pbr_dbgs("Applying PBR command: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	/* Flush route cache */
	if (librouter_pbr_flush_cache() < 0)
		return -1;

	return 0;
}

/**
 * librouter_pbr_route_del	Função apaga rota de determinada tabela (table0-9)
 *
 * @param pbr
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_route_del(librouter_pbr_struct *pbr)
{
	char cmd[256];

	if (strlen(pbr->via_ipaddr) != 0){
		if (strlen(pbr->dev) !=0){
			if (strlen(pbr->network_opt_ipmask) !=0)
				sprintf(cmd, "/bin/ip route del %s/%d via %s dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr_pbr(pbr->network_opt_ipmask), pbr->via_ipaddr, pbr->dev, pbr->table);
			else
				sprintf(cmd, "/bin/ip route del %s via %s dev %s table %s ", pbr->network_opt, pbr->via_ipaddr, pbr->dev, pbr->table);
		}
		else {
			if (strlen(pbr->network_opt_ipmask) !=0)
				sprintf(cmd, "/bin/ip route del %s/%d via %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr_pbr(pbr->network_opt_ipmask), pbr->via_ipaddr, pbr->table);
			else
				sprintf(cmd, "/bin/ip route del %s via %s table %s ", pbr->network_opt, pbr->via_ipaddr, pbr->table);
		}
	}
	else{
		if (strlen(pbr->network_opt_ipmask) !=0)
			sprintf(cmd, "/bin/ip route del %s/%d dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr_pbr(pbr->network_opt_ipmask), pbr->dev, pbr->table);
		else
			sprintf(cmd, "/bin/ip route del %s dev %s table %s ", pbr->network_opt, pbr->dev, pbr->table);
	}

	pbr_dbgs("Applying PBR command: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	/* Flush route cache */
	if (librouter_pbr_flush_cache() < 0)
		return -1;

	return 0;
}

/**
 * librouter_pbr_flush_cache	Função executa um "Flush" na cache do gerenciador de rotas
 * do sistema (ip route)
 *
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_flush_cache(void)
{
	char cmd[64];

	sprintf(cmd, "/bin/ip route flush cache ");

	pbr_dbgs("Applying PBR command: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	return 0;
}

/**
 * librouter_pbr_log_entry_route_add	Função adiciona rota configurada pelo cli no log de comandos aplicados
 *
 * @param command
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_log_entry_route_add(char * command)
{
	if (librouter_str_find_string_in_file_return_stat(PBR_ROUTES_ENTRY_FILE_CONTROL,command))
		return 0;
	if (librouter_str_add_line_to_file(PBR_ROUTES_ENTRY_FILE_CONTROL," ") < 0)
		return -1;
	if (librouter_str_add_line_to_file(PBR_ROUTES_ENTRY_FILE_CONTROL,command) < 0)
		return -1;
	return librouter_str_add_line_to_file(PBR_ROUTES_ENTRY_FILE_CONTROL,"\n");

}

/**
 * librouter_pbr_log_entry_route_del	Função remove rota apontada pelo cli no log de comandos aplicados
 *
 * @param command
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_log_entry_route_del(char * command)
{
	return librouter_str_del_line_in_file(PBR_ROUTES_ENTRY_FILE_CONTROL,command);
}

/**
 * librouter_pbr_log_entry_rule_add		Função adiciona regra apontada pelo cli no log de comandos aplicados
 *
 * @param command
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_log_entry_rule_add(char * command)
{
	if (librouter_str_find_string_in_file_return_stat(PBR_RULES_ENTRY_FILE_CONTROL,command))
		return 0;
	if (librouter_str_add_line_to_file(PBR_RULES_ENTRY_FILE_CONTROL," ") < 0)
		return -1;
	if (librouter_str_add_line_to_file(PBR_RULES_ENTRY_FILE_CONTROL,command) < 0)
		return -1;
	return librouter_str_add_line_to_file(PBR_RULES_ENTRY_FILE_CONTROL,"\n");
}

/**
 * librouter_pbr_log_entry_rule_del		Função remove rota apontada pelo cli no log de comandos aplicados
 *
 * @param command
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_log_entry_rule_del(char * command)
{
	return librouter_str_del_line_in_file(PBR_RULES_ENTRY_FILE_CONTROL,command);
}

/**
 * librouter_pbr_log_entry_flush_table		Função remove todas as rotas armazenadas na tabela em questão
 *
 * @param table_name
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_log_entry_flush_table(char * table_name)
{
	return librouter_str_del_line_in_file(PBR_ROUTES_ENTRY_FILE_CONTROL,table_name);
}

/**
 * pbr_log_verify_entries		Função verifica se existem entradas nos logs de comandos aplicados
 *
 * (opt == 0 para rotas || opt == 1 para regras)
 * @param opt
 * @return 0 if ok, -1 if not
 */
static int pbr_log_verify_entries(int opt)
{
	FILE *fd;

	switch (opt){
		case 0:
				if ((fd = fopen(PBR_ROUTES_ENTRY_FILE_CONTROL, "r")) == NULL) {
						syslog(LOG_ERR, "Could not open PBR_entry_routes_log file\n");
						return -1;
				}

			    fseek (fd, 0, SEEK_END);
				if (ftell(fd)){
					fclose(fd);
					return 1;
				}
				fclose(fd);
				break;
		case 1:
				if ((fd = fopen(PBR_RULES_ENTRY_FILE_CONTROL, "r")) == NULL) {
						syslog(LOG_ERR, "Could not open PBR_entry_rules_log file\n");
						return -1;
				}

				fseek (fd, 0, SEEK_END);
				if (ftell(fd)){
					fclose(fd);
					return 1;
				}
				fclose(fd);
				break;
		default:
				return -1;
				break;
	}
	return 0;
}

/**
 * pbr_fetch_log_entries		Função recupera entrada no log de comandos aplicados pelo PBR no cli
 * e retorna em buffer (opt == 0 para rotas || opt == 1 para regras)
 *
 * @param opt
 * @param buffer
 * @return 0 if ok, -1 if not
 */
static int pbr_fetch_log_entries(int opt, char **buffer)
{
	FILE * pFile;
	long lSize;
	size_t result;

	switch (opt){
		case 0:
				pFile = fopen (PBR_ROUTES_ENTRY_FILE_CONTROL, "r");
				break;
		case 1:
				pFile = fopen (PBR_RULES_ENTRY_FILE_CONTROL, "r");
				break;
		default:
				return -1;
				break;
	}

	if (pFile == NULL){
		syslog(LOG_ERR, "Could not open PBR_entry_log file\n");
		return -1;
	}

	// obtain file size:
	fseek (pFile , 0 , SEEK_END);
	lSize = ftell(pFile) + 1;
	rewind (pFile);

	// allocate memory to contain the whole file:
	*buffer = (char*) malloc (lSize + 1);
	memset(*buffer, 0, lSize+1);
	if (*buffer == NULL){
		syslog(LOG_ERR, "Could not open PBR_entry_log file\n");
		return -1;
	}
	// copy the file into the buffer:
	result = fread (*buffer,1,lSize,pFile);
	if (result != lSize){
		syslog(LOG_ERR, "Could not open PBR_entry_log file\n");
		return -1;
	}
	fclose (pFile);
	return 0;
}

/**
 * librouter_pbr_dump		Função realiza o dump das configurações aplicadas pelo PBR
 *
 * @param out
 * @return 0 if ok
 */
int librouter_pbr_dump(FILE *out)
{
	int rules = 0, routes = 0;
	char * buffer = NULL;

	routes = pbr_log_verify_entries(0);
	rules = pbr_log_verify_entries(1);

	if (routes == 1 || rules == 1){
		fprintf(out, "policy-route\n");
		if (routes){
			pbr_fetch_log_entries(0, &buffer);
			fprintf(out, "%s", buffer);
			free(buffer);
			buffer = NULL;
		}
		if (rules){
			pbr_fetch_log_entries(1, &buffer);
			fprintf(out, "%s", buffer);
			free(buffer);
			buffer = NULL;
		}
		fprintf(out, "!\n");
	}
	return 0;
}

#endif /*OPTION_PBR*/

