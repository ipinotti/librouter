/*
 * dns.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#include "typedefs.h"
#include "ip.h"
#include "args.h"
#include "dns.h"
#include "error.h"
#include "str.h"

struct _entry_info {
	unsigned int active;
	char address[16];
	unsigned int type;
};

/***************************************************************
 **                  Rotinas de manipulacao                  **
 ***************************************************************/

static unsigned int _libconfig_dns_get_info(char *line, struct _entry_info *info)
{
	IP addr;
	char *p;
	int n_args;
	unsigned int idx = 0;
	arg_list argl = NULL;

	if ((n_args = libconfig_parse_args_din(line, &argl)) < 2) {
		libconfig_destroy_args_din(&argl);
		return 0;
	}

	memset(info, 0, sizeof(struct _entry_info));

	if (argl[0][0] == '#') {
		for (p = argl[0]; *p == '#'; p++)
			;

		if (*p == 0) {
			p = argl[1];
			idx = 1;
		}

		if (((idx + 1) >= n_args) || (strcmp(p, "nameserver") != 0)
		                || (inet_aton(argl[idx + 1], &addr) == 0)) {
			libconfig_destroy_args_din(&argl);
			return 0;
		}

	} else {
		if ((strcmp(argl[0], "nameserver") != 0) || (inet_aton(argl[1],
		                &addr) == 0)) {
			libconfig_destroy_args_din(&argl);
			return 0;
		}
		info->active = 1;
	}

	idx++;
	strcpy(info->address, argl[idx]);

	idx++;
	if (idx >= n_args) {
		info->type = DNS_STATIC_NAMESERVER;
		libconfig_destroy_args_din(&argl);
		return 1;
	}

	p = argl[idx];

	if (*p != '#') {
		libconfig_destroy_args_din(&argl);
		return 0;
	}

	for (; *p == '#'; p++)
		;

	if (*p == 0) {
		idx++;
		if (idx >= n_args) {
			info->type = DNS_STATIC_NAMESERVER;
			libconfig_destroy_args_din(&argl);
			return 1;
		}
		p = argl[idx];
	}

	if (strcasecmp(p, "static") == 0) {
		info->type = DNS_STATIC_NAMESERVER;
	} else if (strcasecmp(p, "dynamic") == 0) {
		info->type = DNS_DYNAMIC_NAMESERVER;
	} else {
		info->type = DNS_STATIC_NAMESERVER;
	}

	libconfig_destroy_args_din(&argl);

	return 1;
}

static unsigned int _libconfig_dns_get_info_by_addr(char *nameserver,
                                           struct _entry_info *info)
{
	FILE *f;
	char buf[256];
	unsigned int ret = 0;

	if ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL)
		return ret;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;

		if (_libconfig_dns_get_info(buf, info)) {
			if (strcmp(nameserver, info->address) == 0) {
				ret = 1;
				break;
			}
		}
	}

	fclose(f);

	return ret;
}

static unsigned int _libconfig_dns_get_info_by_type_actv_index(unsigned int type,
                                                      unsigned int actv,
                                                      unsigned int idx,
                                                      struct _entry_info *info)
{
	FILE *f;
	char buf[256];
	unsigned int ret = 0, count = 0;

	if ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL)
		return ret;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;
		if (_libconfig_dns_get_info(buf, info)) {
			if ((info->type == type) && (info->active == actv)) {
				if (count == idx) {
					ret = 1;
					break;
				}
				count++;
			}
		}
	}

	fclose(f);
	return ret;
}

static unsigned int _libconfig_dns_change_state(struct _entry_info *new_info)
{
	FILE *f;
	unsigned int ret = 0;
	struct _entry_info info;
	char buf[256], line[256];

	if ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL)
		return ret;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;
		if (_libconfig_dns_get_info(buf, &info)) {
			if (strcmp(new_info->address, info.address) == 0) {
				ret = 1;
				break;
			}
		}
	}

	fclose(f);

	if (ret == 0)
		return ret;

	line[0] = 0;

	if (new_info->active == 0)
		strcat(line, "# ");

	strcat(line, "nameserver ");
	strcat(line, new_info->address);
	strcat(line, " ");
	strcat(line, "# ");

	switch (new_info->type) {
	case DNS_DYNAMIC_NAMESERVER:
		strcat(line, "dynamic");
		break;
	case DNS_STATIC_NAMESERVER:
	default:
		strcat(line, "static");
		break;
	}

	strcat(line, "\n");
	return ((replace_exact_string(FILE_RESOLVRELAYCONF, buf, line) < 0) ? 0 : 1);
}

/***************************************************************
 **            Insercao/remocao de um servidor DNS            **
 ***************************************************************/

static unsigned int _libconfig_dns_add_nameserver(struct _entry_info *info)
{
	FILE *f;
	char line[256];
	struct _entry_info local;

	if (_libconfig_dns_get_info_by_addr(info->address, &local)) {
		if (memcmp(info, &local, sizeof(struct _entry_info)) != 0)
			return _libconfig_dns_change_state(info);
		return 1;
	}

	if ((f = fopen(FILE_RESOLVRELAYCONF, "a")) == NULL)
		return 0;
	line[0] = 0;
	if (info->active == 0)
		strcat(line, "# ");
	strcat(line, "nameserver ");
	strcat(line, info->address);
	strcat(line, " ");
	strcat(line, "# ");
	switch (info->type) {
	case DNS_DYNAMIC_NAMESERVER:
		strcat(line, "dynamic");
		break;
	case DNS_STATIC_NAMESERVER:
	default:
		strcat(line, "static");
		break;
	}
	strcat(line, "\n");
	fwrite(line, 1, strlen(line), f);
	fclose(f);
	return 1;
}

static unsigned int _libconfig_dns_del_nameserver(char *nameserver)
{
	FILE *f;
	char buf[256];
	unsigned int found = 0;
	struct _entry_info info;

	if ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL)
		return 0;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;
		if (_libconfig_dns_get_info(buf, &info)) {
			if (strcmp(nameserver, info.address) == 0) {
				found = 1;
				break;
			}
		}
	}

	fclose(f);

	if (!found)
		return 0;

	return ((replace_exact_string(FILE_RESOLVRELAYCONF, buf, "") < 0) ? 0 : 1);
}

/***************************************************************
 **              Rotinas de ativacao/desativacao              **
 ***************************************************************/

static unsigned int _libconfig_dns_change_nameserver_activation(char *nameserver,
                                                 unsigned int on_off)
{
	struct _entry_info info;

	if (!_libconfig_dns_get_info_by_addr(nameserver, &info))
		return 0;
	if (info.active == on_off)
		return 1;
	info.active = on_off;
	return _libconfig_dns_change_state(&info);
}

/***************************************************************
 **             Rotinas para configuracao do tipo             **
 ***************************************************************/

static unsigned int _libconfig_dns_get_nameserver_by_type(unsigned int type,
                                                    unsigned int activation)
{
	FILE *f;
	char buf[256];
	unsigned int count = 0;
	struct _entry_info info;

	if ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL)
		return count;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;
		if (_libconfig_dns_get_info(buf, &info) && (info.type == type)
		                && (info.active == activation))
			count++;
	}

	fclose(f);
	return count;
}

/***************************************************************
 **                      Rotina de busca                      **
 ***************************************************************/

static int _libconfig_dns_search_nsswitch(int add_del, char *key)
{
	FILE *f;
	char *p, buf[128];

	if ((f = fopen(FILE_NSSWITCHCONF, "r+")) == NULL) {
		libconfig_pr_error(1, "could not open %s", FILE_NSSWITCHCONF);
		return -1;
	}

	while (!feof(f)) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			break;

		if ((p = strstr(buf, key)) != NULL) {
			fseek(f, -strlen(buf) + (p - buf) - 1, SEEK_CUR);
			fputc(add_del ? ' ' : '#', f);
			fgets(buf, sizeof(buf), f); /* re-skip match line! */
		}
	}

	fclose(f);
	return 0;
}

/***************************************************************
 **         Rotinas disponiveis para o mundo exterior         **
 ***************************************************************/

void libconfig_dns_nameserver(int add, char *nameserver)
{
	struct _entry_info info;
	unsigned int statics_on, statics_off, dynamics_on, dynamics_off;

	if (nameserver == NULL)
		return;

	/* Remocao de uma entrada estatica na lista de servidores DNS */
	if (!add) {
		if (_libconfig_dns_get_info_by_addr(nameserver, &info) && (info.type == DNS_STATIC_NAMESERVER)) {
			_libconfig_dns_del_nameserver(nameserver);
			if (info.active) {
				statics_off = _libconfig_dns_get_nameserver_by_type(DNS_STATIC_NAMESERVER, 0);
				if (statics_off > 0) {
					if (_libconfig_dns_get_info_by_type_actv_index(DNS_STATIC_NAMESERVER, 0, 0, &info))
						_libconfig_dns_change_nameserver_activation(info.address, 1);
				} else {
					dynamics_off = _libconfig_dns_get_nameserver_by_type(DNS_DYNAMIC_NAMESERVER, 0);
					if ((dynamics_off > 0)
						&& _libconfig_dns_get_info_by_type_actv_index(DNS_DYNAMIC_NAMESERVER, 0, 0, &info))
						_libconfig_dns_change_nameserver_activation( info.address, 1);
				}
			}
		}
		return;
	}

	/* Adicao de uma nova entrada na lista de servidores DNS */
	if (_libconfig_dns_get_info_by_addr(nameserver, &info) && (info.active == 1)
	                && (strcmp(info.address, nameserver) == 0)
	                && (info.type == DNS_STATIC_NAMESERVER))
		return;

	statics_off = _libconfig_dns_get_nameserver_by_type(DNS_STATIC_NAMESERVER, 0);
	statics_on = _libconfig_dns_get_nameserver_by_type(DNS_STATIC_NAMESERVER, 1);

	if ((statics_off + statics_on) >= DNS_MAX_SERVERS) {
		printf("%% Name-server table is full; %s not added\n",
		                nameserver);
		return;
	}

	dynamics_on = _libconfig_dns_get_nameserver_by_type(DNS_DYNAMIC_NAMESERVER, 1);

	if ((statics_on + dynamics_on) >= DNS_MAX_SERVERS) {
		if (_libconfig_dns_get_info_by_type_actv_index(DNS_DYNAMIC_NAMESERVER, 1, 0, &info))
			_libconfig_dns_change_nameserver_activation(info.address, 0);
	}

	if (_libconfig_dns_get_info_by_addr(nameserver, &info)) {
		if ((info.type != DNS_STATIC_NAMESERVER) || (info.active != 1)) {
			info.active = 1;
			info.type = DNS_STATIC_NAMESERVER;
			if (!_libconfig_dns_change_state(&info))
				printf("%% Not possible to add name-server %s\n", nameserver);
		}
	} else {
		info.active = 1;
		memcpy(info.address, nameserver, 15);
		info.address[15] = 0;
		info.type = DNS_STATIC_NAMESERVER;
		if (!_libconfig_dns_add_nameserver(&info))
			printf("%% Not possible to add name-server %s\n", nameserver);
	}
}

void libconfig_dns_dynamic_nameserver(int add, char *nameserver)
{
	struct _entry_info info;
	unsigned int statics_on, statics_off, dynamics_on, dynamics_off;

	if (nameserver == NULL)
		return;

	if (!add) { /* Remocao de uma entrada dinamica na lista de servidores DNS */
		if (_libconfig_dns_get_info_by_addr(nameserver, &info) && (info.type == DNS_DYNAMIC_NAMESERVER)) {
			_libconfig_dns_del_nameserver(nameserver);
			if (info.active) {
				statics_off = _libconfig_dns_get_nameserver_by_type(DNS_STATIC_NAMESERVER, 0);
				if (statics_off > 0) {
					if (_libconfig_dns_get_info_by_type_actv_index(DNS_STATIC_NAMESERVER, 0, 0, &info))
						_libconfig_dns_change_nameserver_activation(info.address, 1);
				} else {
					dynamics_off = _libconfig_dns_get_nameserver_by_type(DNS_DYNAMIC_NAMESERVER, 0);
					if ((dynamics_off > 0)
						&& _libconfig_dns_get_info_by_type_actv_index(DNS_DYNAMIC_NAMESERVER, 0, 0, &info))
						_libconfig_dns_change_nameserver_activation(info.address, 1);
				}
			}
		}
		return;
	}

	/* Adicao de uma nova entrada na lista de servidores DNS */
	if (_libconfig_dns_get_info_by_addr(nameserver, &info) && (info.active == 1)
	                && (strcmp(info.address, nameserver) == 0)
	                && (info.type == DNS_DYNAMIC_NAMESERVER))
		return;

	statics_on = _libconfig_dns_get_nameserver_by_type(DNS_STATIC_NAMESERVER, 1);
	dynamics_on = _libconfig_dns_get_nameserver_by_type(DNS_DYNAMIC_NAMESERVER, 1);

	if ((statics_on + dynamics_on) >= DNS_MAX_SERVERS) {
		if (_libconfig_dns_get_info_by_type_actv_index(
		                (statics_on > 0) ? DNS_STATIC_NAMESERVER : DNS_DYNAMIC_NAMESERVER,
		                1, 0, &info))
			_libconfig_dns_change_nameserver_activation(info.address, 0);
		else
			return;
	}

	if (_libconfig_dns_get_info_by_addr(nameserver, &info)) {
		if ((info.type != DNS_DYNAMIC_NAMESERVER) || (info.active != 1)) {
			info.active = 1;
			info.type = DNS_DYNAMIC_NAMESERVER;
			_libconfig_dns_change_state(&info);
		}
	} else {
		info.active = 1;
		memcpy(info.address, nameserver, 15);
		info.address[15] = 0;
		info.type = DNS_DYNAMIC_NAMESERVER;
		_libconfig_dns_add_nameserver(&info);
	}
}

int libconfig_dns_get_nameserver_by_type_actv_index(unsigned int type,
                                      unsigned int actv,
                                      unsigned int index,
                                      char *addr)
{
	FILE *f;
	char buf[256];
	struct _entry_info info;
	unsigned int count = 0, found = 0;

	if ((addr == NULL) || ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL))
		return -1;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;
		if (_libconfig_dns_get_info(buf, &info)) {
			if ((info.type == type) && (info.active == actv)) {
				if (count == index) {
					found = 1;
					break;
				}
				count++;
			}
		}
	}

	fclose(f);

	if (!found)
		return -1;

	strcpy(addr, info.address);
	return 0;
}

int libconfig_dns_get_nameserver_by_type_index(unsigned int type,
                                 unsigned int index,
                                 char *addr)
{
	FILE *f;
	char buf[256];
	struct _entry_info info;
	unsigned int count = 0, found = 0;

	if ((addr == NULL) || ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL))
		return -1;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;
		if (_libconfig_dns_get_info(buf, &info)) {
			if (info.type == type) {
				if (count == index) {
					found = 1;
					break;
				}
				count++;
			}
		}
	}

	fclose(f);

	if (!found)
		return -1;

	strcpy(addr, info.address);
	return 0;
}

int libconfig_dns_get_nameserver_by_actv_index(unsigned int actv,
                                 unsigned int index,
                                 char *addr)
{
	FILE *f;
	char buf[256];
	struct _entry_info info;
	unsigned int count = 0, found = 0;

	if ((addr == NULL) || ((f = fopen(FILE_RESOLVRELAYCONF, "r")) == NULL))
		return -1;

	while (!feof(f)) {
		if (fgets(buf, 255, f) == NULL)
			break;

		buf[255] = 0;
		if (_libconfig_dns_get_info(buf, &info)) {
			if (info.active == actv) {
				if (count == index) {
					found = 1;
					break;
				}
				count++;
			}
		}
	}

	fclose(f);

	if (!found)
		return -1;

	strcpy(addr, info.address);
	return 0;
}

void libconfig_dns_lookup(int on_off)
{
	_libconfig_dns_search_nsswitch(on_off, "dns");
	if (on_off)
		symlink(FILE_RESOLVRELAYCONF, FILE_RESOLVCONF);
	else
		unlink(FILE_RESOLVCONF);
	__res_init();
}

int libconfig_dns_domain_lookup_enabled(void)
{
	FILE *f;

	if ((f = fopen(FILE_RESOLVCONF, "r"))) {
		fclose(f);
		return 1;
	}

	return 0;
}

void libconfig_dns_dump_nameservers(FILE *out)
{
	char addr[16];
	unsigned int i;

	/* Lista servidores DNS estaticos */
	for (i = 0; i < DNS_MAX_SERVERS; i++) {
		if (libconfig_dns_get_nameserver_by_type_index(DNS_STATIC_NAMESERVER, i, addr)
		                < 0)
			break;
		fprintf(out, "ip name-server %s\n", addr);
	}
}
