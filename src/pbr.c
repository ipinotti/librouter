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
//TODO
//	if (librouter_mangle_rule_exists_by_num(mark_no)) {
//
//	}

	sprintf(cmd, "/bin/ip rule add fwmark %d table %s ", mark_no,table);

	pbr_dbgs("Applying PBR rule: %s\n", cmd);

	/* Apply rule */
	if (system(cmd) != 0)
		return -1;

	return 0;
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

//TODO
//	if (librouter_mangle_rule_exists_by_num(mark_no)) {
//
//	}

	sprintf(cmd, "/bin/ip rule del fwmark %d table %s ", mark_no,table);

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
		if (strlen(pbr->network_opt_ipmask) !=0)
			sprintf(cmd, "/bin/ip route add %s/%d via %s dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr(pbr->network_opt_ipmask), pbr->via_ipaddr, pbr->dev, pbr->table);
		else
			sprintf(cmd, "/bin/ip route add %s via %s dev %s table %s ", pbr->network_opt, pbr->via_ipaddr, pbr->dev, pbr->table);
	}
	else{
		if (strlen(pbr->network_opt_ipmask) !=0)
			sprintf(cmd, "/bin/ip route add %s/%d dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr(pbr->network_opt_ipmask), pbr->dev, pbr->table);
		else
			sprintf(cmd, "/bin/ip route add %s dev %s table %s ", pbr->network_opt, pbr->dev, pbr->table);
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
 * librouter_pbr_route_del	Função apaga rota de determinada tabela (table0-9)
 *
 * @param pbr
 * @return 0 if ok, -1 if not
 */
int librouter_pbr_route_del(librouter_pbr_struct *pbr)
{
	char cmd[256];

	if (strlen(pbr->via_ipaddr) != 0){
		if (strlen(pbr->network_opt_ipmask) !=0)
			sprintf(cmd, "/bin/ip route del %s/%d via %s dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr(pbr->network_opt_ipmask), pbr->via_ipaddr, pbr->dev, pbr->table);
		else
			sprintf(cmd, "/bin/ip route del %s via %s dev %s table %s ", pbr->network_opt, pbr->via_ipaddr, pbr->dev, pbr->table);
	}
	else{
		if (strlen(pbr->network_opt_ipmask) !=0)
			sprintf(cmd, "/bin/ip route del %s/%d dev %s table %s ", pbr->network_opt, librouter_ip_netmask2cidr(pbr->network_opt_ipmask), pbr->dev, pbr->table);
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


