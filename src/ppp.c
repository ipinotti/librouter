/*
 * ppp.c
 *
 *  Created on: Jun 24, 2010
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/autoconf.h>

#include "options.h"
#include "defines.h"
#include "nv.h"
#include "ppp.h"
#include "process.h"
#include "ipsec.h"
#include "ip.h"
#include "pam.h"

#ifdef OPTION_MODEM3G
#include "modem3G.h"
#endif

#ifdef OPTION_MODEM3G
/**
 * Função seta método de backup no arquivo do backupd
 * Ex: intf3g_ppp == ppp0  ; method == "ping" ou "link" ; ping_addr == "8.8.8.8" ou NULL-> no caso do link.
 *
 * @param intf3g_ppp
 * @param method
 * @param ping_addr
 * @return 0 if ok, -1 if not
 */
int librouter_ppp_backupd_set_backup_method(char * intf3g_ppp, char * method, char * ping_addr)
{
	if (!strcmp(method, "ping")) {
		if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, METHOD_STR, "ping") < 0)
			return -1;

		if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, PING_ADDR_STR, ping_addr)
		                < 0)
			return -1;
	} else if (!strcmp(method, "link")) {
		if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, METHOD_STR, "link") < 0)
			return -1;

		if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, PING_ADDR_STR, "") < 0)
			return -1;
	} else
		return -1;

	return 0;
}

/**
 * Função seta "no backup interface" no arquivo para que o backupd deamon efetue a operação no Modem3G
 * @param interface3g
 * @return 0 if ok, -1 if not
 */
int librouter_ppp_backupd_set_no_backup_interface(char * intf3g_ppp)
{
	if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, BCKUP_STR, "no") < 0)
		return -1;

	if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, MAIN_INTF_STR, "") < 0)
		return -1;

	return 0;
}

/**
 * Função seta backup interface no arquivo para efetuar operação pelo backupd deamon
 * Ex: intf3g_ppp == "ppp0" && main_interface == "ethernet0"
 *
 * Caso a main_interface já esta em backup, a função retorna em qual PPP
 * ela está configurada pelo intf_return
 * @param interface3g
 * @param main_interface
 * @param intf_return
 * @return 0 if ok, -1 if not
 */
int librouter_ppp_backupd_set_backup_interface(char * intf3g_ppp,
                                               char * main_interface,
                                               char * intf_return)
{
	if (librouter_dev_exists(intf3g_ppp))
		return -1;

	if (librouter_ppp_backupd_verif_param_infile(MAIN_INTF_STR, main_interface, intf_return))
		return -1;
	else
		intf_return = NULL;

	if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, BCKUP_STR, "yes") < 0)
		return -1;

	if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, MAIN_INTF_STR, main_interface) < 0)
		return -1;

	return 0;
}
/**
 * Função verifica se interface 3G está ativada no sistema do backupd
 *
 * @return 1 if ENABLE, 0 if DISABLED
 */
int librouter_ppp_backupd_is_interface_3G_enable(char * intf3g_ppp)
{
	return librouter_ppp_backupd_verif_param_byintf_infile(intf3g_ppp, SHUTD_STR, "no");
}

/**
 * Função faz "no shutdown" no modem3g escolhido (seta no arquivo backupd.conf),
 * OBS: É necessário realizar reload no backupd para efetuar alteração
 * @param interface
 * @return 0 if ok, -1 if not
 */
int librouter_ppp_backupd_set_no_shutdown_3Gmodem(char * intf3g_ppp)
{
	if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, SHUTD_STR, "no") < 0)
		return -1;
	return 0;
}

/**
 * Função faz shutdown no modem3g escolhido (seta no arquivo backupd.conf)
 * OBS: É necessário realizar reload no backupd para efetuar alteração
 * @param interface
 * @return 0 if ok, -1 if not
 */
int librouter_ppp_backupd_set_shutdown_3Gmodem(char * intf3g_ppp)
{
	if (librouter_ppp_backupd_set_param_infile(intf3g_ppp, SHUTD_STR, "yes") < 0)
		return -1;
	return 0;
}

int librouter_ppp_backupd_set_default_route(char *iface, int enable)
{
	if (librouter_ppp_backupd_set_param_infile(iface, DEFAULT_ROUTE_STR, enable ? "yes" : "no") < 0)
		return -1;
	return 0;
}

int librouter_ppp_backupd_set_default_metric(char *iface, int metric)
{
	char str[8];

	sprintf(str, "%d", metric);
	if (librouter_ppp_backupd_set_param_infile(iface, ROUTE_DISTANCE_STR, str) < 0)
		return -1;
	return 0;
}

/**
 * Faz a leitura do arquivo de config. do backupd, carregando as infos. referentes
 * ao serial number(ex:serial number == 0  --> ppp0)
 * As infos. são passada por parâmetro (struct bckp_conf_t)
 *
 * @param serial_num
 * @param back_conf
 * @return 0 if OK, -1 if not
 */
int librouter_ppp_backupd_get_config(int serial_num, struct bckp_conf_t * back_conf)
{
	FILE *fd;
	char line[128] = { (int) NULL };
	char intf_ref[32];
	char *p;
	snprintf(intf_ref, 32, "%sppp%d\n", INTF_STR, serial_num);

	if ((fd = fopen(BACKUPD_CONF_FILE, "r")) == NULL) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return -1;
	}

	while (fgets(line, sizeof(line), fd) != NULL) {
		if (!strncmp(line, intf_ref, strlen(intf_ref))) {

			/* Interface string */
			memset(back_conf, 0, sizeof(struct bckp_conf_t));
			strcpy(back_conf->intf_name, line + INTF_STR_LEN);

			/* Remove any line break */
			for (p = back_conf->intf_name; *p != '\0'; p++) {
				if (*p == '\n')
					*p = '\0';
			}

			while ((fgets(line, sizeof(line), fd) != NULL) && (strcmp(line, "\n") != 0)) {

				/* Shutdown field */
				if (!strncmp(line, SHUTD_STR, SHUTD_STR_LEN)) {
					if (strstr(line, "yes"))
						back_conf->shutdown = 1;
					continue;
				}

				/* Is backup field */
				if (!strncmp(line, BCKUP_STR, BCKUP_STR_LEN)) {
					if (strstr(line, "yes"))
						back_conf->is_backup = 1;
					continue;
				}

				/* Main interface field */
				if (!strncmp(line, MAIN_INTF_STR, MAIN_INTF_STR_LEN)) {
					strcpy(back_conf->main_intf_name, line + MAIN_INTF_STR_LEN);

					/* Remove any line break */
					for (p = back_conf->main_intf_name; *p != '\0'; p++) {
						if (*p == '\n')
							*p = '\0';
					}
					continue;
				}

				/* Which method */
				if (!strncmp(line, METHOD_STR, METHOD_STR_LEN)) {
					if (strstr(line, "link"))
						back_conf->method = BCKP_METHOD_LINK;
					else
						back_conf->method = BCKP_METHOD_PING;
					continue;
				}

				/* Is backup field */
				if (!strncmp(line, PING_ADDR_STR, PING_ADDR_STR_LEN)) {
					strcpy(back_conf->ping_address, line + PING_ADDR_STR_LEN);

					/* Remove any line break */
					for (p = back_conf->ping_address; *p != '\0'; p++) {
						if (*p == '\n')
							*p = '\0';
					}
					continue;
				}


				/* Should install a default route? */
				if (!strncmp(line, DEFAULT_ROUTE_STR, DEFAULT_ROUTE_STR_LEN)) {
					if (strstr(line, "yes"))
						back_conf->is_default_gateway = 1;
					else
						back_conf->is_default_gateway = 0;

					continue;
				}

				/* Is backup field */
				if (!strncmp(line, ROUTE_DISTANCE_STR, ROUTE_DISTANCE_STR_LEN)) {
					back_conf->default_route_distance = atoi(line + ROUTE_DISTANCE_STR_LEN);
					continue;
				}

			}
			goto end;
		}

	}

	end: fclose(fd);
	return 0;
}

/**
 * Verifica a existencia de um parametro em uma dada interface, no arquivo de conf. do backupd
 * BACKUPD_CONF_FILE-> "/etc/backupd/backupd.conf"
 *
 * @param intf
 * @param field
 * @param value
 * @return 1 if (field+value) on a given interface is in file, 0 if not
 */
int librouter_ppp_backupd_verif_param_byintf_infile(char * intf, char *field, char *value)
{
	FILE *fd;
	char line[128] = { (int) NULL };
	char fvalue[32], intf_ref[32];
	int verif = 0;

	snprintf(intf_ref, 32, "%s%s\n", INTF_STR, intf);

	snprintf(fvalue, 32, "%s%s\n", field, value);

	if ((fd = fopen(BACKUPD_CONF_FILE, "r")) == NULL) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return;
	}

	while (fgets(line, sizeof(line), fd) != NULL) {
		if (!strncmp(line, intf_ref, strlen(intf_ref))) {
			while ((fgets(line, sizeof(line), fd) != NULL) && (strcmp(line, "\n") != 0)) {
				if (!strncmp(line, fvalue, strlen(fvalue))) {
					verif = 1;
					goto end;
				}
			}
			goto end;
		}
	}

	end: fclose(fd);
	return verif;
}

/**
 * Verifica a existencia de um parametro desejavel no arquivo de conf. do backupd
 * BACKUPD_CONF_FILE-> "/etc/backupd/backupd.conf"
 *
 * @param field
 * @param value
 * @param intf_return
 * @return 1 and the interface aimed if (field+value) is in file, 0 if not
 */
int librouter_ppp_backupd_verif_param_infile(char *field, char *value, char * intf_return)
{
	FILE *fd;
	char line[128] = { (int) NULL };
	char fvalue[32], intf_ref[32], interface[24];
	int verif = 0;

	snprintf(fvalue, 32, "%s%s\n", field, value);

	if ((fd = fopen(BACKUPD_CONF_FILE, "r")) == NULL) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return;
	}

	while (fgets(line, sizeof(line), fd) != NULL) {
		librouter_str_striplf(line); /* delete new line at end */
		if (!strncmp(line, INTF_STR, INTF_STR_LEN)) {
			strcpy(intf_return, line + INTF_STR_LEN);
			while ((fgets(line, sizeof(line), fd) != NULL) && (strcmp(line, "\n") != 0)) {
				if (!strncmp(line, fvalue, strlen(fvalue))) {
					verif = 1;
					goto end;
				}
			}
		}
	}

	strcpy(intf_return, "");

	end: fclose(fd);
	return verif;
}

/**
 * Grava os parâmetros desejados no arquivo de configurações do backupd,
 * BACKUPD_CONF_FILE-> "/etc/backupd/backupd.conf"
 *
 * Necessita a interface PPP a ser modifica, o campo desejado, e o valor.
 *
 * @param intf
 * @param field
 * @param value
 * @return 0 if success, -1 if it had problems with the file
 */
int librouter_ppp_backupd_set_param_infile(char * intf, char * field, char *value)
{
	FILE *fd;
	char line[128] = { (int) NULL };
	char buff[1024] = { (int) NULL }; /*buff de tamanho determinado para 3 conex PPP */
	char filename_new[64], fvalue[32], intf_ref[32];

	snprintf(intf_ref, 32, "%s%s", INTF_STR, intf);
	snprintf(fvalue, 32, "%s%s\n", field, value);

	if ((fd = fopen(BACKUPD_CONF_FILE, "r")) == NULL) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return -1;
	}

	while (fgets(line, sizeof(line), fd) != NULL) {
		if (!strncmp(line, intf_ref, strlen(intf_ref))) {
			strcat(buff, (const char *) line);
			while (fgets(line, sizeof(line), fd) != NULL) {
				if (!strncmp(line, field, strlen(field))) {
					strcat(buff, (const char *) fvalue);
					break;
				} else
					strcat(buff, (const char *) line);
			}
			continue;
		}
		strcat(buff, (const char *) line);
	}

	fclose(fd);

	strncpy(filename_new, BACKUPD_CONF_FILE, 63);
	filename_new[63] = 0;
	strcat(filename_new, ".new");

	if ((fd = fopen((const char *) filename_new, "w+")) < 0) {
		syslog(LOG_ERR, "Could not open configuration file\n");
		return -1;
	}

	fputs((const char *) buff, fd);

	fclose(fd);

	unlink(BACKUPD_CONF_FILE);
	rename(filename_new, BACKUPD_CONF_FILE);

	return 0;
}

/**
 * ppp_reload_backupd - signal backupd to reload configuration
 *
 * If backupd configuration is changed, it must
 * be aware of it by means of this function.
 *
 * @param: void
 * @ret: 0 if success, -1 if failure
 */
int librouter_ppp_reload_backupd(void)
{
	FILE *f;
	char buf[16];

	f = fopen("/var/run/backupd.pid", "r");
	if (!f) {
		fprintf(stderr, "%% backupd does not seem to be running\n");
		return 0;
	}

	fgets(buf, 15, f);
	fclose(f);

	if (kill((pid_t) atoi(buf), SIGUSR1)) {
		fprintf(stderr, "%% backupd[%i] seems to be down\n", atoi(buf));
		return (-1);
	}

	return 0;
}

#endif

int librouter_ppp_notify_systtyd(void)
{
	FILE *F;
	char buf[16];

	F = fopen("/var/run/systty.pid", "r");

	if (!F)
		return 0;

	fgets(buf, 15, F);
	fclose(F);

	if (kill((pid_t) atoi(buf), SIGHUP)) {
		fprintf(stderr, "%% systtyd[%i] seems to be down\n", atoi(buf));
		return (-1);
	}

	return 0;
}

int librouter_ppp_notify_mgetty(int serial_no)
{
	FILE *F;
	char buf[32];

	if (serial_no < MAX_WAN_INTF)
		sprintf(buf, MGETTY_PID_WAN, serial_no);
	else
		sprintf(buf, MGETTY_PID_AUX, serial_no - MAX_WAN_INTF);

	if ((F = fopen(buf, "r"))) {
		fgets(buf, 31, F);
		fclose(F);

		if (kill((pid_t) atoi(buf), SIGHUP)) {
			fprintf(stderr, "%% mgetty[%i] seems to be down\n", atoi(buf));
			return (-1);
		}

		return 0;
	}

	/* Try to signal tty lock owner */
	if (serial_no < MAX_WAN_INTF)
		sprintf(buf, LOCK_PID_WAN, serial_no);
	else
		sprintf(buf, LOCK_PID_AUX, serial_no - MAX_WAN_INTF);

	if ((F = fopen(buf, "r"))) {
		fgets(buf, 31, F);
		fclose(F);

		if (kill((pid_t) atoi(buf), SIGHUP)) {
			fprintf(stderr, "%% %i seems to be down\n", atoi(buf));
			return (-1);
		}
		return 0;
	}

	return (-1);
}

int librouter_ppp_get_config(int serial_no, ppp_config *cfg)
{
	FILE *f;
	char file[32];

	snprintf(file, 32, PPP_CFG_FILE, serial_no);
	f = fopen(file, "rb");

	if (!f) {
		librouter_ppp_set_defaults(serial_no, cfg);
	} else {
		fread(cfg, sizeof(ppp_config), 1, f);
		fclose(f);
	}

	return 0;
}

int librouter_ppp_has_config(int serial_no)
{
	FILE *f;
	char file[32];

	snprintf(file, 32, PPP_CFG_FILE, serial_no);
	f = fopen(file, "rb");

	if (f) {
		fclose(f);
		return 1;
	}

	return 0;
}

int librouter_ppp_set_config(int serial_no, ppp_config *cfg)
{
	FILE *f;
	char file[32];

	snprintf(file, 32, PPP_CFG_FILE, serial_no);
	f = fopen(file, "wb");

	if (!f)
		return -1;

	snprintf(cfg->osdevice, 16, "ttyU%d", serial_no);

	fwrite(cfg, sizeof(ppp_config), 1, f);
	fclose(f);

	return librouter_ppp_notify_systtyd();
}

void librouter_ppp_set_defaults(int serial_no, ppp_config *cfg)
{

	/* Clean memory! */
	memset(cfg, 0, sizeof(ppp_config));
#ifdef OPTION_MODEM3G
	snprintf(cfg->osdevice, 16, "ttyUSB%d", librouter_usb_device_is_modem(
	                librouter_usb_get_realport_by_aliasport(serial_no)));

	librouter_ppp_backupd_get_config(serial_no, &cfg->bckp_conf);

	cfg->unit = serial_no;

	if (serial_no == BTIN_M3G_ALIAS) {
		cfg->sim_main.sim_num = librouter_modem3g_sim_order_get_mainsim();
		cfg->sim_backup.sim_num = !librouter_modem3g_sim_order_get_mainsim();
		librouter_modem3g_sim_get_info_fromfile(&cfg->sim_main);
		librouter_modem3g_sim_get_info_fromfile(&cfg->sim_backup);
	} else {
		cfg->sim_main.sim_num = 0;
		librouter_modem3g_get_apn(cfg->sim_main.apn, serial_no);
		librouter_modem3g_get_username(cfg->sim_main.username, serial_no);
		librouter_modem3g_get_password(cfg->sim_main.password, serial_no);
	}

	cfg->ip_unnumbered = -1;

	cfg->up = !cfg->bckp_conf.shutdown;

	/*
	 * FIXME
	 * cfg->novj = 1;
	 */
#else
	snprintf(cfg->osdevice, 16, "ttyS%d", serial_no);

	cfg->unit=serial_no;
	cfg->speed=9600;
	cfg->echo_failure=3;
	cfg->echo_interval=5;
	cfg->novj=1;
	cfg->ip_unnumbered=-1;
	cfg->backup=-1;

#endif

}

/*
 ABORT "NO CARRIER"
 ABORT "NO DIALTONE"
 ABORT "ERROR"
 ABORT "NO ANSWER"
 ABORT "BUSY"
 ABORT "Username/Password Incorrect"
 ""    ATZ
 */
int librouter_ppp_add_chat(char *chat_name, char *chat_str)
{
	FILE *f;
	char file[50];

	sprintf(file, PPP_CHAT_FILE, chat_name);
	f = fopen(file, "wt");

	if (!f)
		return -1;

	fputs(chat_str, f);
	fclose(f);

	return 0;
}

int librouter_ppp_del_chat(char *chat_name)
{
	char file[50];

	sprintf(file, PPP_CHAT_FILE, chat_name);

	return unlink(file); /* -1 on error */
}

char *librouter_ppp_get_chat(char *chat_name)
{
	FILE *f;
	char file[50];
	long len;
	char *chat_str;

	sprintf(file, PPP_CHAT_FILE, chat_name);

	f = fopen(file, "rt");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	len = ftell(f) + 1;
	fseek(f, 0, SEEK_SET);

	if (len == 0) {
		fclose(f);
		return NULL;
	}

	chat_str = malloc(len + 1);

	if (chat_str == 0) {
		fclose(f);
		return NULL;
	}

	fgets(chat_str, len, f);
	fclose(f);

	return chat_str;
}

int librouter_ppp_chat_exists(char *chat_name)
{
	FILE *f;
	char file[50];

	sprintf(file, PPP_CHAT_FILE, chat_name);

	f = fopen(file, "rt");

	if (!f)
		return 0;

	fclose(f);

	return 1;
}

int librouter_ppp_get_state(int serial_no)
{
	FILE *f;
	char file[50];

	if (serial_no < MAX_WAN_INTF)
		sprintf(file, PPP_UP_FILE_SERIAL, serial_no);
	else
		sprintf(file, PPP_UP_FILE_AUX, serial_no - MAX_WAN_INTF);

	f = fopen(file, "rt");

	if (!f)
		return 0;

	fclose(f);

	return 1;
}

int librouter_ppp_is_pppd_running(int serial_no)
{
	FILE *f;
	char file[50];
	char buf[32];
	pid_t pid;

	if (serial_no < MAX_WAN_INTF)
		sprintf(file, PPP_PPPD_UP_FILE_SERIAL, serial_no);
	else
		sprintf(file, PPP_PPPD_UP_FILE_AUX, serial_no - MAX_WAN_INTF);

	f = fopen(file, "rt");

	if (!f)
		return 0;

	fread(buf, 32, 1, f);
	fclose(f);

	buf[31] = 0;
	pid = atoi(buf);

	if ((pid > 0) && (librouter_process_pid_exists(pid)))
		return pid;

	return 0;
}

/**
 * Retorna o device serialX correspondente a serial.
 *
 * Ex.: serial_no = 0 -> "serial0"
 *
 * @param serial_no
 * @return
 */
char *librouter_ppp_get_device(int serial_no)
{
	FILE *f;
	char file[50];
	static char pppdevice[32];
	int len;

	pppdevice[0] = 0;

	if (serial_no < MAX_WAN_INTF)
		sprintf(file, PPP_UP_FILE_SERIAL, serial_no);
	else
		sprintf(file, PPP_UP_FILE_AUX, serial_no - MAX_WAN_INTF);

	f = fopen(file, "rt");

	if (!f)
		return NULL;

	fgets(pppdevice, 32, f);
	pppdevice[31] = 0;

	len = strlen(pppdevice);

	if (pppdevice[len - 1] == '\n')
		pppdevice[len - 1] = 0;

	fclose(f);

	return pppdevice;
}

#define MKARG(s)  { arglist[n] = (char *)malloc(strlen(s)+1); strcpy(arglist[n], s); n++; }
#define MKARGI(i) { arglist[n] = (char *)malloc(16); sprintf(arglist[n], "%d", i); n++; }
void librouter_ppp_pppd_arglist(char **arglist, ppp_config *cfg, int server)
{
	int n = 0;
	char filename[100];
	char addr[32], mask[32]; /* strings utilizadas para receber o ip da ethernet quando IP UNNUMBERED ativo */

#if 0
	MKARG("/bin/nice");
	MKARG("-n");
	MKARG("-20");
#endif

	MKARG("/bin/pppd");
	//snprintf(filename, 99, "/etc/ppp/peers/%s", tty->ppp.osdevice); filename[99]=0;

	MKARG(cfg->osdevice); /* tts/aux[0|1] tts/wan[0|1] */

	if (cfg->sync_nasync) {
		MKARG("sync");
	} else {

		if (cfg->speed)
			MKARGI(cfg->speed);

		if (cfg->flow_control == FLOW_CONTROL_RTSCTS) {
			MKARG("crtscts");
		} else {
			MKARG("nocrtscts");

			if (cfg->flow_control == FLOW_CONTROL_XONXOFF)
				MKARG("xonxoff");
		}

		if (server) {
#if 1
			/* !!! Cannot determine ethernet address for proxy ARP */
			MKARG("proxyarp");
#endif
		} else {
			if (cfg->chat_script[0]) {
				char buf[100];

				snprintf(filename, 99, PPP_CHAT_FILE, cfg->chat_script);
				filename[99] = 0;

				MKARG("connect");
#if 1
				/* -t 90 */
				snprintf(buf, 99, "chat -v -f %s", filename);
				buf[99] = 0;
#else
				/* -t 90 */
				snprintf(buf, 99, "chat -f %s", filename);
				buf[99]=0;
#endif
				MKARG(buf);

				if (cfg->dial_on_demand) {
					MKARG("demand");
					MKARG("ktune");
					MKARG("ipcp-accept-remote");
					MKARG("ipcp-accept-local");
				}

				if (cfg->idle) {
					MKARG("idle");
					MKARGI(cfg->idle);
				}

				if (cfg->holdoff) {
					MKARG("holdoff");
					MKARGI(cfg->holdoff);
				}
			}
		}
	}

	MKARG("lock");

	MKARG("asyncmap");
	MKARG("0");

	MKARG("unit");

	/* Undocumented! "serial" index */
	MKARGI(cfg->unit);

	MKARG("nodetach");

	if (!server)
		MKARG("persist");

	if (cfg->mtu) {
		MKARG("mtu");
		MKARGI(cfg->mtu);

		/* !!! same as mtu */
		MKARG("mru");
		MKARGI(cfg->mtu);
	}

	/* Teste para verificar se IP UNNUMBERED esta ativo */
	if (cfg->ip_unnumbered != -1) {
		char ethernetdev[16];

		sprintf(ethernetdev, "ethernet%d", cfg->ip_unnumbered);

		/* Captura o endereço e mascara da interface Ethernet */
		librouter_ip_ethernet_ip_addr(ethernetdev, addr, mask);

		/* Atualiza cfg com os dados da ethernet */
		strncpy(cfg->ip_addr, addr, 16);
		cfg->ip_addr[15] = 0;

		strncpy(cfg->ip_mask, mask, 16);
		cfg->ip_mask[15] = 0;
	}

	if (server) {
		if (cfg->server_ip_addr[0] || cfg->server_ip_peer_addr[0]) {
			/* Configures local and/or remote IP address */
			char buf[40];

			snprintf(buf, 39, "%s:%s", cfg->server_ip_addr, cfg->server_ip_peer_addr);
			buf[39] = 0;
			MKARG(buf);
		}
	} else if (cfg->ip_addr[0] || cfg->ip_peer_addr[0]) {
		/* !!! teste para usar "noip" quando os parametros ip nao estiverem configurados "noipdefault" */
		char buf[40];

		snprintf(buf, 39, "%s:%s", cfg->ip_addr, cfg->ip_peer_addr);
		buf[39] = 0;
		MKARG(buf);
	}

	if (cfg->server_ip_mask[0]) {
		MKARG("netmask");
		MKARG(cfg->server_ip_mask);
	}

	if (cfg->server_auth_user[0]) {
		MKARG("user");
		MKARG(cfg->server_auth_user);
	}

	if (cfg->default_route) {
		/* only work if system has not a default route already */
		MKARG("defaultroute");
	} else {
		MKARG("nodefaultroute");
	}

	if (cfg->auth_user[0] && ((cfg->server_flags & SERVER_FLAGS_PAP) || (cfg->server_flags
	                & SERVER_FLAGS_CHAP))) {
		MKARG("auth");

		if (cfg->server_flags & SERVER_FLAGS_PAP) {
			MKARG("require-pap");
		} else if (cfg->server_flags & SERVER_FLAGS_CHAP) {
			MKARG("require-chap");
		}
	} else {
		/* Will not ask for the peer to authenticate itself */
		MKARG("noauth");
	}

	/* pppd will use lines in the secrets files which have name as 
	 * the second field when looking for a secret to use in authenticating the peer */
	if (cfg->server_auth_user[0]) {
		MKARG("name");
		MKARG(cfg->server_auth_user);
	} else {
		MKARG("name");
		MKARG(cfg->cishdevice);
	}

	/* recebe servidores DNS do peer (Problemas com Juniper!) */
	if (cfg->usepeerdns)
		MKARG("usepeerdns");

	if (cfg->novj)
		MKARG("novj");

	if (cfg->echo_interval) {
		MKARG("lcp-echo-interval");
		MKARGI(cfg->echo_interval);
	}

	if (cfg->echo_failure) {
		MKARG("lcp-echo-failure");
		MKARGI(cfg->echo_failure);
	}

	MKARG("nodeflate");
	MKARG("nobsdcomp");

	if (cfg->multilink)
		MKARG("multilink");

#ifdef CONFIG_HDLC_SPPP_LFI
	{
		char conv[(CONFIG_MAX_LFI_PRIORITY_MARKS * 11)];

		/* Fragmentation */
		if( cfg->fragment_size != 0 ) {
			sprintf(conv, "%d", cfg->fragment_size);
			MKARG("lfifragsize");
			MKARG(conv);
		}

		/* Interleaving */
		if( cfg->priomarks[0] != 0 ) {
			int k;
			char tmp[16];

			conv[0] = 0;
			for(k=0; (k < CONFIG_MAX_LFI_PRIORITY_MARKS) && (cfg->priomarks[k] != 0); k++) {
				sprintf(tmp, "%d:", cfg->priomarks[k]);
				strcat(conv, tmp);
			}
			conv[strlen(conv)-1] = 0;
			MKARG("lfimarks");
			MKARG(conv);
		}
	}
#endif

	if (cfg->ipx_enabled) {
		char ipx_network[10], ipx_node[14];

		snprintf(ipx_network, 10, "%08lX", (unsigned long int) cfg->ipx_network);

		ipx_network[8] = 0;

		sprintf(ipx_node, "%02x%02x%02x%02x%02x%02x", cfg->ipx_node[0], cfg->ipx_node[1],
		                cfg->ipx_node[2], cfg->ipx_node[3], cfg->ipx_node[4],
		                cfg->ipx_node[5]);

		MKARG("ipx");
		MKARG("ipx-network");
		MKARG(ipx_network);
		MKARG("ipx-node");
		MKARG(ipx_node);
		MKARG("ipxcp-accept-remote");

		/* When ip address is not configured and ipx is configured, 
		 * disable ipcp negotiation, otherwire pppd complains that there is no ip address */
		if (!cfg->ip_addr[0])
			MKARG("noip");
#if 0
		MKARG(ipx-routing);
		/* RIP/SAP */
		MKARGI(2);
#endif
	}

#if 1
	/* pppd faz a selecao do que apresentar em log! */
	if (cfg->debug)
		MKARG("debug");
#endif

#if 0
	/* debug */
	{
		int i;

		syslog(LOG_ERR, "pppd line");
		for (i=0; i < n; i++)
		syslog(LOG_INFO, ">> %s", arglist[i]);
	}
#endif
	arglist[n] = NULL;
}

#ifdef OPTION_IPSEC
int librouter_ppp_l2tp_get_config(char *name, ppp_config *cfg)
{
	FILE *f;
	char file[50];

	sprintf(file, L2TP_PPP_CFG_FILE, name);

	f = fopen(file, "rb");

	if (!f) {
		librouter_ppp_l2tp_set_defaults(name, cfg);
	} else {
		fread(cfg, sizeof(ppp_config), 1, f);
		fclose(f);
	}

	return 0;
}

int librouter_ppp_l2tp_has_config(char *name)
{
	FILE *f;
	char file[50];

	sprintf(file, L2TP_PPP_CFG_FILE, name);

	f = fopen(file, "rb");

	if (f) {
		fclose(f);
		return 1;
	}

	return 0;
}

int librouter_ppp_l2tp_set_config(char *name, ppp_config *cfg)
{
	FILE *f;
	char file[50];

	sprintf(file, L2TP_PPP_CFG_FILE, name);

	f = fopen(file, "wb");
	if (!f)
		return -1;

	fwrite(cfg, sizeof(ppp_config), 1, f);
	fclose(f);

	librouter_l2tp_exec(RESTART);

	return 0;
}

void librouter_ppp_l2tp_set_defaults(char *name, ppp_config *cfg)
{
	memset(cfg, 0, sizeof(ppp_config));

	cfg->echo_failure = 5;
	cfg->echo_interval = 30;

	/* default to ethernet0 */
	cfg->ip_unnumbered = 0;
}
#endif

