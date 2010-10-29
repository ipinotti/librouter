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

#include <linux/config.h>
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

char * librouter_nv_get_product_name(char * product_define){

	if (!strcmp(product_define,"CONFIG_DIGISTAR_3G"))
		return "DIGISTAR-3G";

	if (!strcmp(product_define,"CONFIG_DIGISTAR_EFM"))
		return "DIGISTAR-EFM";

	return;
}




static int _nv_search(struct _nv *nv, char *startup_config_path)
{
	FILE *f;
	cfg_header header;
	int i;

	if ((f = fopen(DEV_STARTUP_CONFIG, "r")) == NULL) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	memset(nv, 0, sizeof(struct _nv));
	nv->next_header = -1;

	while (1) {
		if (fread(&header, sizeof(cfg_header), 1, f) == 0) {
#ifdef DEBUG
			printf("EOF\n");
#endif
			break; /* EOF */
		}

#ifdef DEBUG
		printf("header content : \n");
		for (i = 0; i < sizeof(cfg_header); i++) {
			printf("%02x ", ((char *) &header)[i]);
		}
		printf("\n\n");
#endif

		if (header.magic_number == MAGIC_UNUSED) {
			nv->next_header = ftell(f) - sizeof(cfg_header);
			break; /* empty offset flash! */
		}

		switch (header.magic_number) {
		case MAGIC_CONFIG:
			if (nv->config.hdr.magic_number == MAGIC_CONFIG) {
				memcpy(&nv->previousconfig.hdr,
				                &nv->config.hdr,
				                sizeof(cfg_header));
				nv->previousconfig.offset = nv->config.offset;
			}

			memcpy(&nv->config.hdr, &header, sizeof(cfg_header));
			nv->config.offset = ftell(f);
			break;
#ifdef OPTION_IPSEC
		case MAGIC_SECRET:
			memcpy(&nv->secret.hdr, &header, sizeof(cfg_header));
			nv->secret.offset = ftell(f);
			break;
#else
			case MAGIC_SECRET:
			break;
#endif
#if 1
		case MAGIC_SLOT0:
		case MAGIC_SLOT1:
		case MAGIC_SLOT2:
		case MAGIC_SLOT3:
		case MAGIC_SLOT4:
			memcpy(&nv->slot[header.magic_number - MAGIC_SLOT0].hdr,
			                &header, sizeof(cfg_header));
			nv->slot[header.magic_number - MAGIC_SLOT0].offset = ftell(f);
			break;
#endif
		case MAGIC_SSH:
			memcpy(&nv->ssh.hdr, &header, sizeof(cfg_header));
			nv->ssh.offset = ftell(f);
			break;
		case MAGIC_NTP:
			memcpy(&nv->ntp.hdr, &header, sizeof(cfg_header));
			nv->ntp.offset = ftell(f);
			break;
		case MAGIC_SNMP:
			memcpy(&nv->snmp.hdr, &header, sizeof(cfg_header));
			nv->snmp.offset = ftell(f);
			break;
		default:
			librouter_pr_error(0, "bad magic number on startup configuration");
#ifdef DEBUG
			printf("bad magic:0x%x at 0x%lx\n", header.magic_number, ftell(f) - sizeof(cfg_header));
#endif
#if 1 /* continue to read loop after bad magic! */
			break;
#else
			fclose(f);
			return -1;
#endif
		}
		fseek(f, header.size, SEEK_CUR);
	}
	fclose(f);
#ifdef DEBUG
	if (nv->previousconfig.offset)
		printf("previousconfig:0x%lx\n", nv->previousconfig.offset);

	if (nv->config.offset)
		printf("config:0x%lx\n", nv->config.offset);

#ifdef OPTION_IPSEC
	if (nv->secret.offset)
		printf("secret:0x%lx\n", nv->secret.offset);
#endif
#if 1
	if (nv->slot[0].offset)
		printf("slot0:0x%lx\n", nv->slot[0].offset);

	if (nv->slot[1].offset)
		printf("slot1:0x%lx\n", nv->slot[1].offset);

	if (nv->slot[2].offset)
		printf("slot2:0x%lx\n", nv->slot[2].offset);

	if (nv->slot[3].offset)
		printf("slot3:0x%lx\n", nv->slot[3].offset);
#endif
	if (nv->ssh.offset)
		printf("ssh:0x%lx\n", nv->ssh.offset);

	if (nv->ntp.offset)
		printf("ntp:0x%lx\n", nv->ntp.offset);

	if (nv->snmp.offset)
		printf("snmp:0x%lx\n", nv->snmp.offset);

	printf("next_header:0x%lx\n", nv->next_header);
#endif
	return 0;
}

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

static int _nv_clean(int fd, long size, struct _nv *nv)
{
	int regcount;
	mtd_info_t meminfo;
	char *config = NULL;
#ifdef OPTION_IPSEC
	char *secret = NULL;
#endif
#if 1
	int i;
	char *slot[5];
#endif
	char *ssh = NULL;
	char *ntp = NULL;
	char *snmp = NULL;

#if 1
	for (i = 0; i < 5; i++)
		slot[i] = NULL;
#endif

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

#ifdef OPTION_IPSEC
		if (nv->secret.hdr.magic_number == MAGIC_SECRET)
			secret = _nv_read(fd, &nv->secret);
#endif

#if 1
		for (i = 0; i < 5; i++)
			if (nv->slot[i].hdr.magic_number == MAGIC_SLOT0 + i)
				slot[i] = _nv_read(fd, &nv->slot[i]);
#endif

		if (nv->ssh.hdr.magic_number == MAGIC_SSH)
			ssh = _nv_read(fd, &nv->ssh);

		if (nv->ntp.hdr.magic_number == MAGIC_NTP)
			ntp = _nv_read(fd, &nv->ntp);

		if (nv->snmp.hdr.magic_number == MAGIC_SNMP)
			snmp = _nv_read(fd, &nv->snmp);
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
			for (erase.start = 0; erase.start < meminfo.size; erase.start += meminfo.erasesize) {
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

#ifdef OPTION_IPSEC
	if (secret != NULL) {
		write(fd, &nv->secret.hdr, sizeof(cfg_header));
		write(fd, secret, nv->secret.hdr.size);
		free(secret);
	}
#endif

#if 1
	for (i = 0; i < 5; i++) {
		if (slot[i] != NULL) {
			write(fd, &nv->slot[i].hdr, sizeof(cfg_header));
			write(fd, slot[i], nv->slot[i].hdr.size);
			free(slot[i]);
		}
	}
#endif

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

	return 0;
error:
	if (config != NULL)
		free(config);
#ifdef OPTION_IPSEC
	if (secret != NULL)
		free(secret);
#endif
#if 1
	for (i = 0; i < 5; i++)
		free(slot[i]);
#endif
	if (ssh != NULL)
		free(ssh);

	if (ntp != NULL)
		free(ntp);

	if (snmp != NULL)
		free(snmp);

	return -1;
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
	int fd;
	char *config;
	struct _nv nv;

	if (_nv_search(&nv, NULL) < 0)
		return 0;

	if (nv.config.hdr.magic_number != MAGIC_CONFIG)
		return 0;

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	config = _nv_read(fd, &nv.config);
	close(fd);

	if (config == NULL)
		return 0;

	if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		free(config);
		return -1;
	}

	write(fd, config, nv.config.hdr.size + 1);
	close(fd);
	free(config);

	return nv.config.hdr.size;
}

int librouter_nv_load_previous_configuration(char *filename)
{
	int fd;
	char *config;
	struct _nv nv;

	if (_nv_search(&nv, NULL) < 0)
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
	int fd;
	char *secret;
	struct _nv nv;

	if (_nv_search(&nv, NULL) < 0)
		return -1;

	if (nv.ssh.hdr.magic_number != MAGIC_SSH)
		return -1;

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	secret = _nv_read(fd, &nv.ssh);
	close(fd);

	if (secret == NULL)
		return -1;

	if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		free(secret);
		return -1;
	}

	write(fd, secret, nv.ssh.hdr.size);
	close(fd);
	free(secret);

	return nv.ssh.hdr.size;
}

#ifdef OPTION_NTPD
int librouter_nv_load_ntp_secret(char *filename)
{
	int fd;
	char *secret;
	struct _nv nv;

	if (_nv_search(&nv, NULL) < 0)
		return -1;

	if (nv.ntp.hdr.magic_number != MAGIC_NTP)
		return -1;

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	secret = _nv_read(fd, &nv.ntp);
	close(fd);

	if (secret == NULL)
		return -1;

	if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		free(secret);
		return -1;
	}

	write(fd, secret, nv.ntp.hdr.size);
	close(fd);
	free(secret);

	return nv.ntp.hdr.size;
}
#endif /* OPTION_NTPD */

/**
 * Salva na  memoria nao volatil (flash) a configuracao armazenada no arquivo 'filename'
 *
 * @param filename
 * @return
 */
int librouter_nv_save_configuration(char *filename)
{
	int fd;
	long size;
	struct stat st;
	unsigned char *config;
	cfg_header header;
	struct _nv nv;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		return -1;
	}

	fstat(fd, &st);
	size = st.st_size;

	if ((config = malloc(size + 1)) == NULL) {
		librouter_pr_error(1, "unable to allocate memory for startup configuration");
		close(fd);
		return -1;
	}

	read(fd, config, size);
	close(fd);
	config[size] = 0;

	if (_nv_search(&nv, NULL) < 0)
		return -1;

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDWR)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(config);
		return -1;
	}

	if (_nv_clean(fd, size, &nv) < 0) {
		close(fd);
		free(config);
		return -1;
	}

	header.magic_number = MAGIC_CONFIG;

	if (nv.config.hdr.magic_number != MAGIC_CONFIG)
		header.id = 1;
	else
		header.id = nv.config.hdr.id + 1;

	header.size = size;
	header.crc = librouter_calculate_crc32_checksum(config, size);

	/* allow empty config with size == 0 */
	write(fd, &header, sizeof(cfg_header));

	if (size > 0)
		write(fd, config, size);

	close(fd);
	free(config);

	return 0;
}

int librouter_nv_save_ssh_secret(char *filename)
{
	int fd;
	long size;
	struct _nv nv;
	struct stat st;
	cfg_header header;
	unsigned char *secret;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		return -1;
	}

	fstat(fd, &st);
	size = st.st_size;

	if ((secret = malloc(size + 1)) == NULL) {
		librouter_pr_error(1, "unable to allocate memory for ssh secret");
		close(fd);
		return -1;
	}

	read(fd, secret, size);
	close(fd);
	secret[size] = 0;

	_nv_search(&nv, NULL);

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDWR)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(secret);
		return -1;
	}

	if (_nv_clean(fd, size, &nv) < 0) {
		close(fd);
		free(secret);
		return -1;
	}

	header.magic_number = MAGIC_SSH;

	if (nv.ssh.hdr.magic_number != MAGIC_SSH)
		header.id = 1;
	else
		header.id = nv.ssh.hdr.id + 1;

	header.size = size;
	header.crc = librouter_calculate_crc32_checksum(secret, size);

	/* allow empty config with size == 0 */
	write(fd, &header, sizeof(cfg_header));

	if (size > 0)
		write(fd, secret, size);

	close(fd);
	free(secret);

	return 0;
}

#ifdef OPTION_NTPD
int librouter_nv_save_ntp_secret(char *filename)
{
	int fd;
	long size;
	struct _nv nv;
	struct stat st;
	cfg_header header;
	unsigned char *secret;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		return -1;
	}

	fstat(fd, &st);
	size = st.st_size;

	if ((secret = malloc(size + 1)) == NULL) {
		librouter_pr_error(1, "unable to allocate memory for ntp secret");
		close(fd);
		return -1;
	}

	read(fd, secret, size);
	close(fd);
	secret[size] = 0;

	_nv_search(&nv, NULL);

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDWR)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(secret);
		return -1;
	}

	if (_nv_clean(fd, size, &nv) < 0) {
		close(fd);
		free(secret);
		return -1;
	}

	header.magic_number = MAGIC_NTP;

	if (nv.ntp.hdr.magic_number != MAGIC_NTP)
		header.id = 1;
	else
		header.id = nv.ntp.hdr.id + 1;

	header.size = size;
	header.crc = librouter_calculate_crc32_checksum(secret, size);

	/* allow empty config with size == 0 */
	write(fd, &header, sizeof(cfg_header));

	if (size > 0)
		write(fd, secret, size);

	close(fd);
	free(secret);

	return 0;
}
#endif /* OPTION_NTPD */

#ifdef OPTION_IPSEC
int librouter_nv_save_ipsec_secret(char *secret)
{
	int fd;
	long size;
	cfg_header header;
	struct _nv nv;

	_nv_search(&nv, NULL);

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDWR)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	size = strlen(secret);

	if (_nv_clean(fd, size, &nv) < 0) {
		close(fd);
		return -1;
	}

	header.magic_number = MAGIC_SECRET;

	if (nv.secret.hdr.magic_number != MAGIC_SECRET)
		header.id = 1;
	else
		header.id = nv.secret.hdr.id + 1;

	header.size = size;
	header.crc = librouter_calculate_crc32_checksum((unsigned char *) secret, size);

	if (header.size > 0) {
		write(fd, &header, sizeof(cfg_header));
		write(fd, secret, size);
	}

	close(fd);

	return 0;
}

char *librouter_nv_get_rsakeys(void)
{
	int fd;
	char *secret;
	struct _nv nv;

	if (_nv_search(&nv, NULL) < 0)
		return NULL;

	if (nv.secret.hdr.magic_number != MAGIC_SECRET)
		return NULL;

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return NULL;
	}

	secret = _nv_read(fd, &nv.secret);
	close(fd);

	return secret;
}
#endif /* OPTION_IPSEC */

int librouter_nv_load_snmp_secret(char *filename)
{
	int fd;
	char *secret;
	struct _nv nv;

	if (_nv_search(&nv, NULL) < 0)
		return -1;

	if (nv.snmp.hdr.magic_number != MAGIC_SNMP)
		return -1;

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}

	secret = _nv_read(fd, &nv.snmp);
	close(fd);

	if (secret == NULL)
		return -1;

	if ((fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		free(secret);
		return -1;
	}

	write(fd, secret, nv.snmp.hdr.size);
	close(fd);

	free(secret);

	return nv.snmp.hdr.size;
}

int librouter_nv_save_snmp_secret(char *filename)
{
	int fd;
	long size;
	unsigned char *secret;
	struct _nv nv;
	struct stat st;
	cfg_header header;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		librouter_pr_error(1, "could not open %s", filename);
		return -1;
	}

	fstat(fd, &st);
	size = st.st_size;

	if ((secret = malloc(size + 1)) == NULL) {
		librouter_pr_error(1, "unable to allocate memory for snmp secret");
		close(fd);
		return -1;
	}

	read(fd, secret, size);
	close(fd);
	secret[size] = 0;

	_nv_search(&nv, NULL);

	if ((fd = open(DEV_STARTUP_CONFIG, O_RDWR)) < 0) {
		librouter_pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(secret);
		return -1;
	}

	if (_nv_clean(fd, size, &nv) < 0) {
		close(fd);
		free(secret);
		return -1;
	}

	header.magic_number = MAGIC_SNMP;

	if (nv.snmp.hdr.magic_number != MAGIC_SNMP)
		header.id = 1;
	else
		header.id = nv.snmp.hdr.id + 1;

	header.size = size;
	header.crc = librouter_calculate_crc32_checksum(secret, size);

	/* allow empty config with size == 0 */
	write(fd, &header, sizeof(cfg_header));

	if (size > 0)
		write(fd, secret, size);

	close(fd);
	free(secret);

	return 0;
}

