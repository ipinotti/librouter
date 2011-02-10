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

	return 0;
}

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

	return 0;
}


