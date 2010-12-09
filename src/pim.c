/*
 *  pim.c
 *
 *  Refactored on: Nov 26, 2010
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
#include "args.h"
#include "defines.h"
#include "device.h"
#include "error.h"
#include "exec.h"
#include "pim.h"
#include "str.h"

/**
 * librouter_pim_sparse_hup	Função utilizada para fazer a releitura
 * das configurações do PIM Dense no arquivo /etc/pimdd.conf
 *
 */
static void librouter_pim_dense_hup(void)
{
	FILE *f;
	pid_t pid;
	char buf[32];

	if (!(f = fopen(PIM_DENSE_PID, "r")))
		return;

	fgets(buf, 31, f);
	fclose(f);

	pid = (pid_t) atoi(buf);

	if (pid > 1)
		kill(pid, SIGHUP);
}

/**
 * librouter_pim_sparse_hup	Função utilizada para fazer a releitura
 * das configurações do PIM Sparse no arquivo /etc/pimsd.conf
 *
 */
static void librouter_pim_sparse_hup(void)
{
	FILE *f;
	pid_t pid;
	char buf[32];

	if (!(f = fopen(PIM_SPARSE_PID, "r")))
		return;

	fgets(buf, 31, f);
	fclose(f);

	pid = (pid_t) atoi(buf);

	if (pid > 1)
		kill(pid, SIGHUP);
}

/**
 * pim_sparse_read_conffile	Função utilizada para armazenar as informações
 * de configuração do PIMSD (arquivo /etc/pimsd.conf) em arglist.
 *
 * @param arlist **args
 * @return Retorna número de linhas que foram lidas do arquivo
 */
static int pim_sparse_read_conffile(arglist * args[])
{
	FILE *f;
	int lines = 0;
	char line[200];

	if ((f = fopen(PIMS_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES_PIM) {
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
 * pim_dense_read_conffile	Função utilizada para armazenar as informações
 * de configuração do PIMDD (arquivo /etc/pimdd.conf) em arglist.
 *
 * @param arlist **args
 * @return Retorna número de linhas que foram lidas do arquivo
 */
static int pim_dense_read_conffile(arglist * args[])
{
	FILE *f;
	int lines = 0;
	char line[200];

	if ((f = fopen(PIMD_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES_PIM) {
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
 *  Nao foi realizado refactory, pois o uso de pim dense foi descontinuado
 * @deprecated
 *
 * @param add
 * @param dev
 * @return
 */
int librouter_pim_dense_phyint(int add, char *dev)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i, j, intf = 0, found = 0, lines = 0;
	char line[200];

	if ((f = fopen(PIMD_CFG_FILE, "r")) != NULL) {
		while (fgets(line, 200, f) && lines < MAX_LINES_PIM) {
			librouter_str_striplf(line);
			if (strlen(line))
				args[lines++] = librouter_make_args(line);
		}
		fclose(f);
	}

	if ((f = fopen(PIMD_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (!strcmp(args[i]->argv[0], "phyint")) {

				if (!strcmp(args[i]->argv[1], dev)) {

					found = 1;

					if (!add) {
						librouter_destroy_args(args[i]);
						/* skip line (delete) */
						continue;
					}
				}

				intf++;

			} else {
				/* add after phyint declarations */
				if (add && !found) {
					found = 1;
					fprintf(f, "phyint %s enable preference 101 metric 1024\n", dev);
					intf++;
				}
			}

			for (j = 0; j < args[i]->argc; j++) {
				if (j < args[i]->argc - 1)
					fprintf(f, "%s ", args[i]->argv[j]);
				else
					fprintf(f, "%s\n", args[i]->argv[j]);
			}

			librouter_destroy_args(args[i]);
		}

		fclose(f);
	}

	librouter_pim_dense_hup();

	return intf;
}

/**
 * librouter_pim_sparse_enable		Função grava no arquivo
 * de configuração do PIM Sparse, o status do deamon, enable/disable
 *
 * @param opt
 * @return 0 if ok, -1 if not
 */
int librouter_pim_sparse_enable(int opt)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i,j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return -1;
	}

	/*edit conf file to the deserved config*/
	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if ( (!strcmp(args[i]->argv[0], "#status")) ) {
				if (opt)
					fprintf(f, "#status enable\n");
				else
					fprintf(f, "#status disable\n");
				continue;
			}
			else {
				for (j = 0; j < args[i]->argc; j++) {
					if (j < args[i]->argc - 1)
						fprintf(f, "%s ", args[i]->argv[j]);
					else
						fprintf(f, "%s\n", args[i]->argv[j]);
				}
			}
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	fclose(f);

	librouter_pim_sparse_hup();

	return 0;
}

/**
 * librouter_pim_sparse_verify_intf_enable	Verifica se a interface está habilitada
 * nas configurações do PIM Sparse
 *
 * @param dev
 * @return 1 if enable, 0 if not, -1 if had problems with the source file
 */
int librouter_pim_sparse_verify_intf_enable(char *dev)
{
	arglist *args[MAX_LINES_PIM];
	int i, found = 0, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return -1;
	}

	for (i = 0; i < lines; i++) {
		if ( (!strcmp(args[i]->argv[0], "phyint")) && (!strcmp(args[i]->argv[1], dev)) ) {
			if ( (!strcmp(args[i]->argv[2], "enable")) )
				found = 1;
			break;
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	return found;
}

/**
 * librouter_pim_sparse_phyint		Função habilita(1) / desabilita(0) a interface
 * no arquivo de configuraçoes do PIM Sparse, configurando preference e metric quando necessário,
 * senão configura valores default passando 0 para os parâmetros correspondentes
 *
 * @param add
 * @param dev
 * @param pref
 * @param metric
 * @return 0 if ok, -1 if not
 */
int librouter_pim_sparse_phyint(int add, char *dev, int pref, int metric)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i, j, intf = 0, found = 0, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return -1;
	}

	/*edit conf file to the deserved config*/
	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if ( (!strcmp(args[i]->argv[0], "phyint")) && (!strcmp(args[i]->argv[1], dev)) ) {
				if (add){
					if (!pref && !metric)
						fprintf(f, "phyint %s enable preference 101 metric 1024\n", dev);
					else
						fprintf(f, "phyint %s enable preference %d metric %d\n", dev, pref, metric);
				}
				else
					fprintf(f, "phyint %s disable\n", dev);
				continue;
			}
			else {
				for (j = 0; j < args[i]->argc; j++) {
					if (j < args[i]->argc - 1)
						fprintf(f, "%s ", args[i]->argv[j]);
					else
						fprintf(f, "%s\n", args[i]->argv[j]);
				}
			}
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	fclose(f);

	librouter_pim_sparse_hup();

	return 0;
}

/**
 * librouter_pim_sparse_bsr_candidate		Função habilita(1) / desabilita(0)
 * bsr_candidate no arquivo de configuraçoes do PIM Sparse
 *
 * @param add
 * @param dev
 * @param major
 * @param priority
 * @return 0 if ok, -1 if not
 */
int librouter_pim_sparse_bsr_candidate(int add, char *dev, char *major, char *priority)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i, j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return -1;
	}

	/*edit conf file to the deserved config*/
	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (strstr(args[i]->argv[0], "bootstrap")) {
				if (add) {
					if (priority == NULL)
						fprintf(f, "cand_bootstrap_router %s%s\n", dev, major);
					else
						fprintf(f, "cand_bootstrap_router %s%s priority %s\n",
								dev, major, priority);
				}
				else
					fprintf(f, "#cand_bootstrap_router\n");
				continue;
			}
			else {
				for (j = 0; j < args[i]->argc; j++) {
					if (j < args[i]->argc - 1)
						fprintf(f, "%s ", args[i]->argv[j]);
					else
						fprintf(f, "%s\n", args[i]->argv[j]);
				}
			}
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	fclose(f);

	librouter_pim_sparse_hup();

	return 0;
}

/**
 * librouter_pim_sparse_rp_address		Função habilita(1) / desabilita(0)
 * rp_address no arquivo de configuraçoes do PIM Sparse
 *
 * @param add
 * @param rp
 * @return 0 if ok, -1 if not
 */
int librouter_pim_sparse_rp_address(int add, char *rp)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i, j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return -1;
	}

	/*edit conf file to the deserved config*/
	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (strstr(args[i]->argv[0], "rp_address")) {
				if (add)
					fprintf(f, "rp_address %s\n", rp);
				else
					fprintf(f, "#rp_address\n");
				continue;
			}
			else {
				for (j = 0; j < args[i]->argc; j++) {
					if (j < args[i]->argc - 1)
						fprintf(f, "%s ", args[i]->argv[j]);
					else
						fprintf(f, "%s\n", args[i]->argv[j]);
				}
			}
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	fclose(f);

	librouter_pim_sparse_hup();

	return 0;

}

/**
 * librouter_pim_sparse_rp_candidate		Função habilita(1) / desabilita(0)
 * rp_candidate no arquivo de configuraçoes do PIM Sparse
 *
 * @param add
 * @param dev
 * @param major
 * @param priority
 * @param interval
 * @return 0 if ok, -1 if not
 */
int librouter_pim_sparse_rp_candidate(int add, char *dev, char *major, char *priority, char *interval)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i, j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return -1;
	}

	/*edit conf file to the deserved config*/
	if ((f = fopen(PIMS_CFG_FILE, "w")) != NULL) {
		for (i = 0; i < lines; i++) {
			if (strstr(args[i]->argv[0], "cand_rp")) {
				if (add){
					if (priority == NULL)
						fprintf(f, "cand_rp %s%s\n", dev, major);
					else {
						if (interval == NULL)
							fprintf(f, "cand_rp %s%s priority %s\n", dev, major, priority);
						else
							fprintf(f, "cand_rp %s%s time %s priority %s \n", dev, major, interval, priority);
					}
				}
				else
					fprintf(f, "#cand_rp\n");
				continue;
			}
			else {
				for (j = 0; j < args[i]->argc; j++) {
					if (j < args[i]->argc - 1)
						fprintf(f, "%s ", args[i]->argv[j]);
					else
						fprintf(f, "%s\n", args[i]->argv[j]);
				}
			}
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

	fclose(f);

	librouter_pim_sparse_hup();

	return 0;
}

/**
 * librouter_pim_dump_interface			Função escreve no arquivo
 * de configurações a opção de configuração PIM Sparse da interface ethernet
 *
 * @param out
 * @param ifname
 */
void librouter_pim_dump_interface(FILE *out, char *ifname)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i, j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return;
	}
	for (i = 0; i < lines; i++) {
		if ( (!strcmp(args[i]->argv[0], "phyint")) && (!strcmp(args[i]->argv[1], ifname)) && (!strcmp(args[i]->argv[2], "enable")) ){
			fprintf(out, " ip pim sparse-mode\n");
			break;
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

#ifdef OPTION_PIMD_DENSE
	/*read conf file to retrieve info*/
	lines = pim_dense_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMD");
		return -1;
	}
	for (i = 0; i < lines; i++) {
		if ( (!strcmp(args[i]->argv[0], "phyint")) && (!strcmp(args[i]->argv[1], ifname)) && (!strcmp(args[i]->argv[2], "enable")) ){
			fprintf(out, " ip pim dense-mode\n");
			break;
		}
	}

	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);

#endif

}

/**
 * librouter_pim_dump		Função escreve no arquivo
 * de configuração, todas as opçoes configuradas para o PIM Sparse no sistema CLI
 *
 * @param out
 */
void librouter_pim_dump(FILE *out)
{
	FILE *f;
	arglist *args[MAX_LINES_PIM];
	int i, j, lines = 0;

	/*read conf file to retrieve info*/
	lines = pim_sparse_read_conffile(args);
	if (lines == 0){
		syslog(LOG_ERR,"No file to retrieve info about PIMSD");
		return;
	}

	for (i = 0; i < lines; i++) {
		if (!strcmp(args[i]->argv[0], "cand_bootstrap_router")) {
			switch (args[i]->argc){

			case 2:
				fprintf(out, "ip pim bsr-candidate %s\n", librouter_device_convert_os(args[i]->argv[1], 0));
				break;

			case 4:
				fprintf(out, "ip pim bsr-candidate %s priority %s\n", librouter_device_convert_os(args[i]->argv[1], 0), args[i]->argv[3]);				break;

			default:
				break;
			}
		}

		if (!strcmp(args[i]->argv[0], "rp_address")) {
			switch (args[i]->argc){

			case 2:
				fprintf(out, "ip pim rp-address %s\n", args[i]->argv[1]);
				break;

			default:
				break;
			}
		}

		if (!strcmp(args[i]->argv[0], "cand_rp")) {
			switch (args[i]->argc){

			case 2:
				fprintf(out,"ip pim rp-candidate %s\n", librouter_device_convert_os(args[i]->argv[1], 0));
				break;

			case 4:
				fprintf(out, "ip pim rp-candidate %s priority %s\n", librouter_device_convert_os(args[i]->argv[1], 0), args[i]->argv[3]);
				break;

			case 6:
				fprintf(out, "ip pim rp-candidate %s priority %s interval %s\n",librouter_device_convert_os(args[i]->argv[1], 0), args[i]->argv[5], args[i]->argv[3]);
				break;

			default:
				break;
			}
		}

		if (!strcmp(args[i]->argv[0], "#status")) {
			if (!strcmp(args[i]->argv[1], "enable"))
				fprintf(out,"ip pim sparse-mode\n");
		}

	}
	/*clean conf file buffer*/
	for (i=0; i<lines; i++)
		librouter_destroy_args(args[i]);
}
