/*
 * nv.c
 *
 * Copyright (C) 2010 PD3 Tecnologia
 *
 * Non volatile function support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mtd/mtd-user.h>

#include <linux/autoconf.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "options.h"

#include "nv.h"
#include "args.h"
#include "crc.h"
#include "error.h"
#include "libtime.h"
#include "defines.h"
#include "flashsave.h"
#include "exec.h"
#include "crc32.h"

#include <u-boot/image.h>

//#define DEBUG

char * librouter_nv_get_product_name(char * product_define)
{

	if (!strcmp(product_define, "CONFIG_DIGISTAR_3G"))
		return "DIGISTAR-3G";

	if (!strcmp(product_define, "CONFIG_DIGISTAR_EFM"))
		return "DIGISTAR-EFM";

	return;
}

/**
 * _get_pack_from_magic
 *
 * Get the corresponding cfg_pack pointer, depending on the magic number
 *
 * @param nv
 * @param magic
 * @return pointer to cfg_pack structure within nv
 */
cfg_pack * _get_pack_from_magic(struct _nv *nv, int magic)
{
	cfg_pack *pack;

	switch (magic) {
	case MAGIC_CONFIG:
		pack = &nv->config;
		break;
	case MAGIC_IPSEC:
		pack = &nv->secret;
		break;
	case MAGIC_SLOT0:
		pack = &nv->slot[0];
		break;
	case MAGIC_SLOT1:
		pack = &nv->slot[1];
		break;
	case MAGIC_SLOT2:
		pack = &nv->slot[2];
		break;
	case MAGIC_SLOT3:
		pack = &nv->slot[3];
		break;
	case MAGIC_SLOT4:
		pack = &nv->slot[4];
		break;
	case MAGIC_SSH:
		pack = &nv->ssh;
		break;
	case MAGIC_NTP:
		pack = &nv->ntp;
		break;
	case MAGIC_SNMP:
		pack = &nv->snmp;
		break;
	case MAGIC_BANNER_LOGIN:
		pack = &nv->banner_login;
		break;
	case MAGIC_BANNER_SYSTEM:
		pack = &nv->banner_system;
		break;
	default:
		pack = NULL;
		break;
	}

	return pack;
}

/**
 * _nv_fetch	Complete the nv structure
 *
 * @param nv
 * @param startup_config_path
 * @return 0 if success, -1 if error
 */
static int _nv_fetch(struct _nv *nv, char *startup_config_path)
{
	FILE *f;
	cfg_header header;
	int i;
	cfg_pack *pack;

	if ((f = fopen(DEV_STARTUP_CONFIG, "r")) == NULL) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	memset(nv, 0, sizeof(struct _nv));
	nv->next_header = -1;

	while (1) {
		if (fread(&header, sizeof(cfg_header), 1, f) == 0) {
			nvdbg("EOF\n");
			break; /* EOF */
		}

		nvdbg("header content : \n");
		nvdbg("magic %08x --> size %d bytes\n", header.magic_number, header.size);
		nvdbg("----------------------------------\n");

		if (header.magic_number == MAGIC_UNUSED) {
			nv->next_header = ftell(f) - sizeof(cfg_header);
			break; /* empty offset flash! */
		}

		switch (header.magic_number) {
		case MAGIC_CONFIG:
			if (nv->config.hdr.magic_number == MAGIC_CONFIG) {
				memcpy(&nv->previousconfig.hdr, &nv->config.hdr, sizeof(cfg_header));
				nv->previousconfig.offset = nv->config.offset;
			}
			pack = &nv->config;
			break;

		case MAGIC_IPSEC:
			pack = &nv->secret;
			break;

		case MAGIC_SLOT0:
		case MAGIC_SLOT1:
		case MAGIC_SLOT2:
		case MAGIC_SLOT3:
		case MAGIC_SLOT4:
			pack = &nv->slot[header.magic_number - MAGIC_SLOT0];
			break;

		case MAGIC_SSH:
			pack = &nv->ssh;
			break;
		case MAGIC_NTP:
			pack = &nv->ntp;
			break;
		case MAGIC_SNMP:
			pack = &nv->snmp;
			break;
		case MAGIC_BANNER_LOGIN:
			pack = &nv->banner_login;
			break;
		case MAGIC_BANNER_SYSTEM:
			pack = &nv->banner_system;
			break;
		default:
			librouter_pr_error(0, "bad magic number on startup configuration");
#ifdef DEBUG
			printf("bad magic:0x%x at 0x%lx\n", header.magic_number, ftell(f) - sizeof(cfg_header));
#endif
			fclose(f);
			return -1;
		}

		memcpy(&pack->hdr, &header, sizeof(cfg_header));
		pack->offset = ftell(f);

		fseek(f, header.size, SEEK_CUR);
	}
	fclose(f);

	if (nv->previousconfig.offset)
		nvdbg("previousconfig:0x%lx\n", nv->previousconfig.offset);

	if (nv->config.offset)
		nvdbg("config:0x%lx\n", nv->config.offset);

	if (nv->secret.offset)
		nvdbg("secret:0x%lx\n", nv->secret.offset);

	if (nv->slot[0].offset)
		nvdbg("slot0:0x%lx\n", nv->slot[0].offset);

	if (nv->slot[1].offset)
		nvdbg("slot1:0x%lx\n", nv->slot[1].offset);

	if (nv->slot[2].offset)
		nvdbg("slot2:0x%lx\n", nv->slot[2].offset);

	if (nv->slot[3].offset)
		nvdbg("slot3:0x%lx\n", nv->slot[3].offset);

	if (nv->ssh.offset)
		nvdbg("ssh:0x%lx\n", nv->ssh.offset);

	if (nv->ntp.offset)
		nvdbg("ntp:0x%lx\n", nv->ntp.offset);

	if (nv->snmp.offset)
		nvdbg("snmp:0x%lx\n", nv->snmp.offset);

	if (nv->banner_login.offset)
		nvdbg("banner_login:0x%lx\n", nv->banner_login.offset);

	if (nv->banner_system.offset)
		nvdbg("banner_system:0x%lx\n", nv->banner_system.offset);

	nvdbg("next_header:0x%lx\n", nv->next_header);

	return 0;
}

/**
 * _nv_read	Get data from nv
 *
 * @param fd
 * @param pack
 * @return pointer to data
 */
static char *_nv_read(int fd, cfg_pack *pack)
{
	unsigned char *buf;

	if ((buf = malloc(pack->hdr.size + 1)) == NULL) {
		librouter_pr_error(1, "unable to allocate memory for startup configuration");
		return NULL;
	}

	lseek(fd, pack->offset, SEEK_SET);
	read(fd, buf, pack->hdr.size);
	buf[pack->hdr.size] = 0;

	if (pack->hdr.crc != librouter_calculate_crc32_checksum(buf, pack->hdr.size)) {
		librouter_pr_error(0, "bad CRC on startup configuration");
		free(buf);
		return NULL;
	}

	pack->valid = 1;

	return (char *) buf;
}

/**
 * _nv_clean	Erase flash sector if necessary to fill next configuration
 *
 * @param fd
 * @param size
 * @param nv
 * @return
 */
static int _nv_clean(int fd, long size, struct _nv *nv)
{
	int regcount;
	mtd_info_t meminfo;
	char *config = NULL;
	char *secret = NULL;
	char *slot[5];
	char *ssh = NULL;
	char *ntp = NULL;
	char *snmp = NULL;
	char *banner_login = NULL;
	char *banner_system = NULL;
	int i;

	for (i = 0; i < 5; i++)
		slot[i] = NULL;


	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		librouter_pr_error(1, "unable to get MTD device info");
		return -1;
	}

	if (nv->next_header >= 0) {
		if ((nv->next_header + size + 2 * sizeof(cfg_header)) < meminfo.size) {
			/* continue to fill sector! */
			lseek(fd, nv->next_header, SEEK_SET);
			return 0;
		}

		if (nv->config.hdr.magic_number == MAGIC_CONFIG)
			config = _nv_read(fd, &nv->config);


		if (nv->secret.hdr.magic_number == MAGIC_IPSEC)
			secret = _nv_read(fd, &nv->secret);


		for (i = 0; i < 5; i++)
			if (nv->slot[i].hdr.magic_number == MAGIC_SLOT0 + i)
				slot[i] = _nv_read(fd, &nv->slot[i]);

		if (nv->ssh.hdr.magic_number == MAGIC_SSH)
			ssh = _nv_read(fd, &nv->ssh);

		if (nv->ntp.hdr.magic_number == MAGIC_NTP)
			ntp = _nv_read(fd, &nv->ntp);

		if (nv->snmp.hdr.magic_number == MAGIC_SNMP)
			snmp = _nv_read(fd, &nv->snmp);

		if (nv->banner_login.hdr.magic_number == MAGIC_BANNER_LOGIN)
			banner_login = _nv_read(fd, &nv->banner_login);

		if (nv->banner_system.hdr.magic_number == MAGIC_BANNER_SYSTEM)
			banner_system = _nv_read(fd, &nv->banner_system);
	}
#ifdef DEBUG
	printf("erase config! (0x%x)\n", meminfo.size);
#endif

	if (ioctl(fd, MEMGETREGIONCOUNT, &regcount) == 0) {
		if (regcount > 0) {
			int i, start;
			region_info_t *reginfo;

			reginfo = calloc(regcount, sizeof(region_info_t));

			for (i = 0; i < regcount; i++) {
				reginfo[i].regionindex = i;
				if (ioctl(fd, MEMGETREGIONINFO, &(reginfo[i])) != 0) {
					free(reginfo);
					goto error;
				}
			}

			for (start = 0, i = 0; i < regcount;) {
				erase_info_t erase;
				region_info_t *r = &(reginfo[i]);

				erase.start = start;
				erase.length = r->erasesize;

				if (ioctl(fd, MEMERASE, &erase) != 0) {
					free(reginfo);
					librouter_pr_error(1, "MTD Erase failure");
					goto error;
				}

				start += erase.length;
				if (start >= (r->offset + r->numblocks * r->erasesize)) {
					i++; /* We finished region i so move to region i+1 */
				}
			}

			free(reginfo);
		} else {
			erase_info_t erase;

			erase.length = meminfo.erasesize;
			for (erase.start = 0; erase.start < meminfo.size; erase.start
			                += meminfo.erasesize) {
				if (ioctl(fd, MEMERASE, &erase) != 0) {
					librouter_pr_error(1, "MTD Erase failure");
					goto error;
				}
			}
		}
	}

	/* config erased, rewrite last info! */
	lseek(fd, 0, SEEK_SET); /* re-start from begining! */

	if (config != NULL) {
		write(fd, &nv->config.hdr, sizeof(cfg_header));
		write(fd, config, nv->config.hdr.size);
		free(config);
	}

	if (secret != NULL) {
		write(fd, &nv->secret.hdr, sizeof(cfg_header));
		write(fd, secret, nv->secret.hdr.size);
		free(secret);
	}

	for (i = 0; i < 5; i++) {
		if (slot[i] != NULL) {
			write(fd, &nv->slot[i].hdr, sizeof(cfg_header));
			write(fd, slot[i], nv->slot[i].hdr.size);
			free(slot[i]);
		}
	}

	if (ssh != NULL) {
		write(fd, &nv->ssh.hdr, sizeof(cfg_header));
		write(fd, ssh, nv->ssh.hdr.size);
		free(ssh);
	}

	if (ntp != NULL) {
		write(fd, &nv->ntp.hdr, sizeof(cfg_header));
		write(fd, ntp, nv->ntp.hdr.size);
		free(ntp);
	}

	if (snmp != NULL) {
		write(fd, &nv->snmp.hdr, sizeof(cfg_header));
		write(fd, snmp, nv->snmp.hdr.size);
		free(snmp);
	}

	if (banner_login != NULL) {
		write(fd, &nv->banner_login.hdr, sizeof(cfg_header));
		write(fd, banner_login, nv->banner_login.hdr.size);
		free(banner_login);
	}

	if (banner_system != NULL) {
		write(fd, &nv->banner_system.hdr, sizeof(cfg_header));
		write(fd, banner_system, nv->banner_system.hdr.size);
		free(banner_system);
	}

	return 0;
	error: if (config != NULL)
		free(config);

	if (secret != NULL)
		free(secret);

	for (i = 0; i < 5; i++)
		free(slot[i]);

	if (ssh != NULL)
		free(ssh);

	if (ntp != NULL)
		free(ntp);

	if (snmp != NULL)
		free(snmp);

	if (banner_login != NULL)
		free(banner_login);

	if (banner_system != NULL)
		free(banner_system);

	return -1;
}

/**
 * _nv_save	Save data in flash
 *
 * @param data
 * @param len
 * @param magic
 * @return 0 if success, -1 if error
 */
int _nv_save(char *data, int len, int magic)
{
	struct _nv nv;
	cfg_header header;
	cfg_pack *pack;
	int fd;

	_nv_fetch(&nv, NULL);

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDWR)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	/* Check if sector has enough space */
	if (_nv_clean(fd, len, &nv) < 0) {
		close(fd);
		return -1;
	}

	pack = _get_pack_from_magic(&nv, magic);

	header.magic_number = magic;

	if (pack->hdr.magic_number != magic)
		header.id = 1;
	else
		header.id = pack->hdr.id + 1;

	header.size = len;
	header.crc = librouter_calculate_crc32_checksum(data, len);

	/* allow empty config with size == 0 */
	write(fd, &header, sizeof(cfg_header));
	if (len > 0)
		write(fd, data, len);

	close(fd);
	return 0;
}

/**
 * _nv_load	Fetch info from flash
 *
 * @param data
 * @param magic
 * @return Number of bytes read
 */
int _nv_load(char **data, int magic)
{
	int fd;
	char *nv_data;
	struct _nv nv;
	cfg_pack *pack;

	if (_nv_fetch(&nv, NULL) < 0)
		return -1;

	pack = _get_pack_from_magic(&nv, magic);
	if (pack == NULL) {
		printf("%s : Did not find data from magic %08x\n", __FUNCTION__, magic);
		return -1;
	}

	if (pack->hdr.magic_number != magic) {
		printf("%s : Magic number mismatch\n", __FUNCTION__);
		return -1;
	}

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	nv_data = _nv_read(fd, pack);

	if (nv_data == NULL)
		return -1;

	*data = malloc(pack->hdr.size);
	memcpy(*data, nv_data, pack->hdr.size);

	free(nv_data);

	return pack->hdr.size;
}

/**
 * _nv_file_write	Write data to file
 *
 * @param filename
 * @param data
 * @param len
 * @return 0 if success, -1 if error
 */
int _nv_file_write(char *filename, char *data, int len)
{
	int fd;

	if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		return -1;
	}

	write(fd, data, len);
	close(fd);

	return 0;
}

/**
 * _nv_file_read	Read data from file
 *
 * @param filename
 * @param data
 * @return Number of bytes read
 */
int _nv_file_read(char *filename, char **data)
{
	int fd;
	struct stat st;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		return -1;
	}

	fstat(fd, &st);

	*data = malloc(st.st_size);
	read(fd, *data, st.st_size);

	close(fd);

	return st.st_size;
}

/**
 * Le a configuracao da memoria nao volatil (flash) e armazena no arquivo 'filename'
 * Retorna o tamanho da configuracao, ou -1 em caso de erro
 *
 * @param filename
 * @return
 */
int librouter_nv_load_configuration(char *filename)
{
	char *data;
	int len;

	len = _nv_load(&data, MAGIC_CONFIG);

	_nv_file_write(filename, data, len + 1);

	free(data);

	return len;
}

/* FIXME previous config has same magic as current config !!!! */
int librouter_nv_load_previous_configuration(char *filename)
{
	int fd;
	char *config;
	struct _nv nv;

	if (_nv_fetch(&nv, NULL) < 0)
		return 0;

	if (nv.previousconfig.hdr.magic_number != MAGIC_CONFIG)
		return 0;

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	config = _nv_read(fd, &nv.previousconfig);
	close(fd);

	if (config == NULL)
		return 0;

	if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		free(config);
		return -1;
	}

	write(fd, config, nv.previousconfig.hdr.size + 1);
	close(fd);
	free(config);

	return nv.previousconfig.hdr.size;
}

int librouter_nv_load_ssh_secret(char *filename)
{
	char *data;
	int len;

	len = _nv_load(&data, MAGIC_SSH);

	_nv_file_write(filename, data, len);

	free(data);

	return len;
}

#ifdef OPTION_NTPD
int librouter_nv_load_ntp_secret(char *filename)
{
	char *data;
	int len;

	len = _nv_load(&data, MAGIC_NTP);

	_nv_file_write(filename, data, len);

	free(data);

	return len;
}
#endif /* OPTION_NTPD */

/**
 * librouter_nv_save_configuration
 *
 * Save configuration in non-volatile memory
 *
 * @param filename
 * @return
 */
int librouter_nv_save_configuration(char *filename)
{
	char *data;
	int len;

	len = _nv_file_read(filename, &data);
	_nv_save(data, len, MAGIC_CONFIG);

	free(data);
	return 0;
}

int librouter_nv_save_ssh_secret(char *filename)
{
	char *data;
	int len;

	len = _nv_file_read(filename, &data);
	_nv_save(data, len, MAGIC_SSH);

	free(data);
	return 0;
}

int librouter_nv_save_ntp_secret(char *filename)
{
	char *data;
	int len;

	len = _nv_file_read(filename, &data);
	_nv_save(data, len, MAGIC_CONFIG);

	free(data);
	return 0;
}

int librouter_nv_save_ipsec_secret(char *data)
{
	int len = strlen(data);

	return _nv_save(data, len, MAGIC_IPSEC);
}

int librouter_nv_load_ipsec_secret(char *data)
{
	return _nv_load(&data, MAGIC_IPSEC);
}

int librouter_nv_load_snmp_secret(char *filename)
{
	char *data;
	int len;

	len = _nv_load(&data, MAGIC_SNMP);

	_nv_file_write(filename, data, len);

	free(data);

	return len;
}

int librouter_nv_save_snmp_secret(char *filename)
{
	char *data;
	int len;

	len = _nv_file_read(filename, &data);
	_nv_save(data, len, MAGIC_SNMP);

	free(data);
	return 0;
}

int librouter_nv_load_banner_login(char *data)
{
	return _nv_load(&data, MAGIC_BANNER_LOGIN);
}

int librouter_nv_save_banner_login(char *data)
{
	int len = strlen(data + 1);

	return _nv_save(data, len, MAGIC_BANNER_LOGIN);
}

int librouter_nv_load_banner_system(char *data)
{
	return _nv_load(&data, MAGIC_BANNER_SYSTEM);
}

int librouter_nv_save_banner_system(char *data)
{
	int len = strlen(data + 1);

	return _nv_save(data, len, MAGIC_BANNER_SYSTEM);
}
