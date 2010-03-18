#include <linux/config.h>
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
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <cish/options.h>
#include <libconfig/nv.h>
#include <libconfig/args.h>
#include <libconfig/crc.h>
#include <libconfig/error.h>
#include <libconfig/time.h>
#include <libconfig/defines.h>
#include <libconfig/flashsave.h>
#include <libconfig/exec.h>
#include <libconfig/crc32.h>
#include "../../u-boot/include/image.h"

#undef DEBUG

static int search_nv(struct _nv *nv, char *startup_config_path)
{
	FILE *f;
	cfg_header header;

#ifdef CONFIG_BERLIN_SATROUTER
	if( !startup_config_path )
		return -1;
	if ((f=fopen(startup_config_path, "r")) == NULL)
#else
	if ((f=fopen(DEV_STARTUP_CONFIG, "r")) == NULL)
#endif
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	memset(nv, 0, sizeof(struct _nv));
	nv->next_header=-1;
	while (1)
	{
		if (fread(&header, sizeof(cfg_header), 1, f) == 0)
		{
#ifdef DEBUG
			printf("EOF\n");
#endif
			break; /* EOF */
		}
		if (header.magic_number == MAGIC_UNUSED)
		{
			nv->next_header=ftell(f)-sizeof(cfg_header);
			break; /* empty offset flash! */
		}
		switch(header.magic_number)
		{
			case MAGIC_CONFIG:
				if (nv->config.hdr.magic_number == MAGIC_CONFIG)
				{
					memcpy(&nv->previousconfig.hdr, &nv->config.hdr, sizeof(cfg_header));
					nv->previousconfig.offset=nv->config.offset;
				}
				memcpy(&nv->config.hdr, &header, sizeof(cfg_header));
				nv->config.offset=ftell(f);
				break;
#ifdef OPTION_IPSEC
			case MAGIC_SECRET:
				memcpy(&nv->secret.hdr, &header, sizeof(cfg_header));
				nv->secret.offset=ftell(f);
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
				memcpy(&nv->slot[header.magic_number-MAGIC_SLOT0].hdr, &header, sizeof(cfg_header));
				nv->slot[header.magic_number-MAGIC_SLOT0].offset=ftell(f);
				break;
#endif
			case MAGIC_SSH:
				memcpy(&nv->ssh.hdr, &header, sizeof(cfg_header));
				nv->ssh.offset=ftell(f);
				break;
			case MAGIC_NTP:
				memcpy(&nv->ntp.hdr, &header, sizeof(cfg_header));
				nv->ntp.offset=ftell(f);
				break;
			case MAGIC_SNMP:
				memcpy(&nv->snmp.hdr, &header, sizeof(cfg_header));
				nv->snmp.offset=ftell(f);
				break;
			default:
				pr_error(0, "bad magic number on startup configuration");
#ifdef DEBUG
				printf("bad magic:0x%x at 0x%lx\n", header.magic_number, ftell(f));
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
	if (nv->previousconfig.offset) printf("previousconfig:0x%lx\n", nv->previousconfig.offset);
	if (nv->config.offset) printf("config:0x%lx\n", nv->config.offset);
#ifdef OPTION_IPSEC
	if (nv->secret.offset) printf("secret:0x%lx\n", nv->secret.offset);
#endif
#if 1
	if (nv->slot[0].offset) printf("slot0:0x%lx\n", nv->slot[0].offset);
	if (nv->slot[1].offset) printf("slot1:0x%lx\n", nv->slot[1].offset);
	if (nv->slot[2].offset) printf("slot2:0x%lx\n", nv->slot[2].offset);
	if (nv->slot[3].offset) printf("slot3:0x%lx\n", nv->slot[3].offset);
#endif
	if (nv->ssh.offset) printf("ssh:0x%lx\n", nv->ssh.offset);
	if (nv->ntp.offset) printf("ntp:0x%lx\n", nv->ntp.offset);
	if (nv->snmp.offset) printf("snmp:0x%lx\n", nv->snmp.offset);
	printf("next_header:0x%lx\n", nv->next_header);
#endif
	return 0;
}

static char *read_nv(int fd, cfg_pack *pack)
{
	unsigned char *buf;

	if ((buf=malloc(pack->hdr.size+1)) == NULL)
	{
		pr_error(1, "unable to allocate memory for startup configuration");
		return NULL;
	}
	lseek(fd, pack->offset, SEEK_SET);
	read(fd, buf, pack->hdr.size);
	buf[pack->hdr.size]=0;
	if (pack->hdr.crc != CalculateCRC32Checksum(buf, pack->hdr.size))
	{
		pr_error(0, "bad CRC on startup configuration");
		free(buf);
		return NULL;
	}
	pack->valid=1;
	return (char *)buf;
}

static int clean_nv(int fd, long size, struct _nv *nv)
{
	int regcount;
	mtd_info_t meminfo;
	char *config=NULL;
#ifdef OPTION_IPSEC
	char *secret=NULL;
#endif
#if 1
	int i;
	char *slot[5];
#endif
	char *ssh=NULL;
	char *ntp=NULL;
	char *snmp=NULL;

#if 1
	for(i=0; i < 5; i++) slot[i]=NULL;
#endif
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0)
	{
		pr_error(1, "unable to get MTD device info");
		return -1;
	}
	if (nv->next_header >= 0)
	{
		if ((nv->next_header+size+2*sizeof(cfg_header)) < meminfo.size)
		{
			lseek(fd, nv->next_header, SEEK_SET); /* continue to fill sector! */
			return 0;
		}
		if (nv->config.hdr.magic_number == MAGIC_CONFIG) config=read_nv(fd, &nv->config);
#ifdef OPTION_IPSEC
		if (nv->secret.hdr.magic_number == MAGIC_SECRET) secret=read_nv(fd, &nv->secret);
#endif
#if 1
		for(i=0; i < 5; i++) if (nv->slot[i].hdr.magic_number == MAGIC_SLOT0+i) slot[i]=read_nv(fd, &nv->slot[i]);
#endif
		if (nv->ssh.hdr.magic_number == MAGIC_SSH) ssh=read_nv(fd, &nv->ssh);
		if (nv->ntp.hdr.magic_number == MAGIC_NTP) ntp=read_nv(fd, &nv->ntp);
		if (nv->snmp.hdr.magic_number == MAGIC_SNMP) snmp=read_nv(fd, &nv->snmp);
	}
#ifdef DEBUG
	printf("erase config! (0x%x)\n", meminfo.size);
#endif
	if (ioctl(fd, MEMGETREGIONCOUNT, &regcount) == 0)
	{
		if (regcount > 0)
		{
			int i, start;
			region_info_t *reginfo;

			reginfo=calloc(regcount, sizeof(region_info_t));
			for(i=0; i < regcount; i++)
			{
				reginfo[i].regionindex = i;
				if (ioctl(fd, MEMGETREGIONINFO, &(reginfo[i])) != 0)
				{
					free(reginfo);
					goto error;
				}
			}
			for(start=0, i=0; i < regcount;)
			{
				erase_info_t erase;
				region_info_t *r=&(reginfo[i]);

				erase.start=start;
				erase.length=r->erasesize;
				if (ioctl(fd, MEMERASE, &erase) != 0)
				{
					free(reginfo);
					pr_error(1, "MTD Erase failure");
					goto error;
				}
				start+=erase.length;
				if (start >= (r->offset + r->numblocks*r->erasesize))
				{
					i++; /* We finished region i so move to region i+1 */
				}
			}
			free(reginfo);
		}
		else
		{
			erase_info_t erase;

			erase.length = meminfo.erasesize;
			for (erase.start=0; erase.start < meminfo.size; erase.start += meminfo.erasesize)
			{
				if (ioctl(fd, MEMERASE, &erase) != 0)
				{
					pr_error(1, "MTD Erase failure");
					goto error;
				}
			}
		}
	}
	/* config erased, rewrite last info! */
	lseek(fd, 0, SEEK_SET); /* re-start from begining! */
	if (config != NULL)
	{
		write(fd, &nv->config.hdr, sizeof(cfg_header));
		write(fd, config, nv->config.hdr.size);
		free(config);
	}
#ifdef OPTION_IPSEC
	if (secret != NULL)
	{
		write(fd, &nv->secret.hdr, sizeof(cfg_header));
		write(fd, secret, nv->secret.hdr.size);
		free(secret);
	}
#endif
#if 1
	for(i=0; i < 5; i++)
	{
		if (slot[i] != NULL)
		{
			write(fd, &nv->slot[i].hdr, sizeof(cfg_header));
			write(fd, slot[i], nv->slot[i].hdr.size);
			free(slot[i]);
		}
	}
#endif
	if (ssh != NULL)
	{
		write(fd, &nv->ssh.hdr, sizeof(cfg_header));
		write(fd, ssh, nv->ssh.hdr.size);
		free(ssh);
	}
	if (ntp != NULL)
	{
		write(fd, &nv->ntp.hdr, sizeof(cfg_header));
		write(fd, ntp, nv->ntp.hdr.size);
		free(ntp);
	}
	if (snmp != NULL)
	{
		write(fd, &nv->snmp.hdr, sizeof(cfg_header));
		write(fd, snmp, nv->snmp.hdr.size);
		free(snmp);
	}
	return 0;
error:
	if (config != NULL) free(config);
#ifdef OPTION_IPSEC
	if (secret != NULL) free(secret);
#endif
#if 1
	for(i=0; i < 5; i++) free(slot[i]);
#endif
	if (ssh != NULL) free(ssh);
	if (ntp != NULL) free(ntp);
	if (snmp != NULL) free(snmp);
	return -1;
}

/* Le a configuracao da memoria nao volatil (flash) e armazena no arquivo 'filename' 
 * Retorna o tamanho da configuracao, ou -1 em caso de erro */
int load_configuration(char *filename)
{
	int fd;
	char *config;
	struct _nv nv;

	if (search_nv(&nv, NULL) < 0) return 0;
	if (nv.config.hdr.magic_number != MAGIC_CONFIG) return 0;

	if ((fd=open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	config=read_nv(fd, &nv.config);
	close(fd);
	if (config == NULL) return 0;

	if ((fd=open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		free(config);
		return -1;
	}
	write(fd, config, nv.config.hdr.size+1);
	close(fd);
	free(config);
	return nv.config.hdr.size;
}

int load_previous_configuration(char *filename)
{
	int fd;
	char *config;
	struct _nv nv;

	if (search_nv(&nv, NULL) < 0) return 0;
	if (nv.previousconfig.hdr.magic_number != MAGIC_CONFIG) return 0;

	if ((fd=open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	config=read_nv(fd, &nv.previousconfig);
	close(fd);
	if (config == NULL) return 0;

	if ((fd=open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		free(config);
		return -1;
	}
	write(fd, config, nv.previousconfig.hdr.size+1);
	close(fd);
	free(config);
	return nv.previousconfig.hdr.size;
}

#if 0
#ifdef CONFIG_PROTOTIPO
int load_slot_configuration(char *filename, int slot)
{
	int fd;
	char *config;
	struct _nv nv;

	if (slot > (MAGIC_SLOT4-MAGIC_SLOT0)) return 0;
	if (search_nv(&nv, NULL) < 0) return 0;
	if (nv.slot[slot].hdr.magic_number != MAGIC_SLOT0+slot) return 0;

	if ((fd=open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	config=read_nv(fd, &nv.slot[slot]);
	close(fd);
	if (config == NULL) return 0;

	if ((fd=open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		free(config);
		return -1;
	}
	write(fd, config, nv.slot[slot].hdr.size+1);
	close(fd);
	free(config);
	return nv.slot[slot].hdr.size;
}
#endif
#endif

int load_ssh_secret(char *filename)
{
	int fd;
	char *secret;
	struct _nv nv;

	if (search_nv(&nv, NULL) < 0) return -1;
	if (nv.ssh.hdr.magic_number != MAGIC_SSH) return -1;

	if ((fd=open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	secret=read_nv(fd, &nv.ssh);
	close(fd);
	if (secret == NULL) return -1;

	if ((fd=open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		free(secret);
		return -1;
	}
	write(fd, secret, nv.ssh.hdr.size);
	close(fd);
	free(secret);
	return nv.ssh.hdr.size;
}

#ifdef OPTION_NTPD
int load_ntp_secret(char *filename)
{
	int fd;
	char *secret;
	struct _nv nv;

	if (search_nv(&nv, NULL) < 0) return -1;
	if (nv.ntp.hdr.magic_number != MAGIC_NTP) return -1;

	if ((fd=open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	secret=read_nv(fd, &nv.ntp);
	close(fd);
	if (secret == NULL) return -1;

	if ((fd=open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		free(secret);
		return -1;
	}
	write(fd, secret, nv.ntp.hdr.size);
	close(fd);
	free(secret);
	return nv.ntp.hdr.size;
}

#endif /* OPTION_NTPD */

/* Salva na  memoria nao volatil (flash) a configuracao armazenada no arquivo 'filename' */
int save_configuration(char *filename)
{
	int fd;
	long size;
	struct stat st;
	unsigned char *config;
	cfg_header header;
	struct _nv nv;

	if ((fd=open(filename, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		return -1;
	}
	fstat(fd, &st);
	size=st.st_size;
	if ((config=malloc(size+1)) == NULL)
	{
		pr_error(1, "unable to allocate memory for startup configuration");
		close(fd);
		return -1;
	}
	read(fd, config, size);
	close(fd);
	config[size]=0;

	search_nv(&nv, NULL);
	if ((fd=open(DEV_STARTUP_CONFIG, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(config);
		return -1;
	}
	if (clean_nv(fd, size, &nv) < 0)
	{
		close(fd);
		free(config);
		return -1;
	}
	header.magic_number=MAGIC_CONFIG;
	if (nv.config.hdr.magic_number != MAGIC_CONFIG) header.id=1;
		else header.id=nv.config.hdr.id+1;
	header.size=size;
	header.crc=CalculateCRC32Checksum(config, size);
	write(fd, &header, sizeof(cfg_header)); /* allow empty config with size == 0 */
	if (size > 0) write(fd, config, size);
	close(fd);
	free(config);
	return 0;
}

#if 0
#ifdef CONFIG_PROTOTIPO
int save_slot_configuration(char *filename, int slot)
{
	int fd;
	long size;
	struct stat st;
	char *config;
	cfg_header header;
	struct _nv nv;

	if (slot > (MAGIC_SLOT4-MAGIC_SLOT0)) return -1;
	if ((fd=open(filename, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		return -1;
	}
	fstat(fd, &st);
	size=st.st_size;
	if ((config=malloc(size+1)) == NULL)
	{
		pr_error(1, "unable to allocate memory for startup configuration");
		close(fd);
		return -1;
	}
	read(fd, config, size);
	close(fd);
	config[size]=0;

	search_nv(&nv, NULL);
	if ((fd=open(DEV_STARTUP_CONFIG, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(config);
		return -1;
	}
	if (clean_nv(fd, size, &nv) < 0)
	{
		close(fd);
		free(config);
		return -1;
	}
	header.magic_number=MAGIC_SLOT0+slot;
	if (nv.slot[slot].hdr.magic_number != MAGIC_SLOT0+slot) header.id=1;
		else header.id=nv.slot[slot].hdr.id+1;
	header.size=size;
	header.crc=CalculateCRC32Checksum(config, size);
	write(fd, &header, sizeof(cfg_header)); /* allow empty config with size == 0 */
	if (size > 0) write(fd, config, size);
	close(fd);
	free(config);
	return 0;
}
#endif
#endif

int save_ssh_secret(char *filename)
{
	int fd;
	long size;
	struct _nv nv;
	struct stat st;
	cfg_header header;
	unsigned char *secret;

	if ((fd=open(filename, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		return -1;
	}
	fstat(fd, &st);
	size=st.st_size;
	if ((secret=malloc(size+1)) == NULL)
	{
		pr_error(1, "unable to allocate memory for ssh secret");
		close(fd);
		return -1;
	}
	read(fd, secret, size);
	close(fd);
	secret[size]=0;

	search_nv(&nv, NULL);
	if ((fd=open(DEV_STARTUP_CONFIG, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(secret);
		return -1;
	}
	if (clean_nv(fd, size, &nv) < 0)
	{
		close(fd);
		free(secret);
		return -1;
	}
	header.magic_number=MAGIC_SSH;
	if (nv.ssh.hdr.magic_number != MAGIC_SSH) header.id=1;
		else header.id=nv.ssh.hdr.id+1;
	header.size=size;
	header.crc=CalculateCRC32Checksum(secret, size);
	write(fd, &header, sizeof(cfg_header)); /* allow empty config with size == 0 */
	if (size > 0) write(fd, secret, size);
	close(fd);

	free(secret);
	return 0;
}

#ifdef OPTION_NTPD
int save_ntp_secret(char *filename)
{
	int fd;
	long size;
	struct _nv nv;
	struct stat st;
	cfg_header header;
	unsigned char *secret;

	if ((fd=open(filename, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		return -1;
	}
	fstat(fd, &st);
	size=st.st_size;
	if ((secret=malloc(size+1)) == NULL)
	{
		pr_error(1, "unable to allocate memory for ntp secret");
		close(fd);
		return -1;
	}
	read(fd, secret, size);
	close(fd);
	secret[size]=0;

	search_nv(&nv, NULL);
	if ((fd=open(DEV_STARTUP_CONFIG, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(secret);
		return -1;
	}
	if (clean_nv(fd, size, &nv) < 0)
	{
		close(fd);
		free(secret);
		return -1;
	}
	header.magic_number=MAGIC_NTP;
	if (nv.ntp.hdr.magic_number != MAGIC_NTP) header.id=1;
		else header.id=nv.ntp.hdr.id+1;
	header.size=size;
	header.crc=CalculateCRC32Checksum(secret, size);
	write(fd, &header, sizeof(cfg_header)); /* allow empty config with size == 0 */
	if (size > 0) write(fd, secret, size);
	close(fd);

	free(secret);
	return 0;
}
#endif /* OPTION_NTPD */

#ifdef OPTION_IPSEC
int set_stored_secret(char *secret)
{
	int fd;
	long size;
	cfg_header header;
	struct _nv nv;

	search_nv(&nv, NULL);
	if ((fd=open(DEV_STARTUP_CONFIG, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	size=strlen(secret);
	if (clean_nv(fd, size, &nv) < 0)
	{
		close(fd);
		return -1;
	}
	header.magic_number=MAGIC_SECRET;
	if (nv.secret.hdr.magic_number != MAGIC_SECRET) header.id=1;
		else header.id=nv.secret.hdr.id+1;
	header.size=size;
	header.crc=CalculateCRC32Checksum((unsigned char *)secret, size);
	if (header.size > 0)
	{
		write(fd, &header, sizeof(cfg_header));
		write(fd, secret, size);
	}
	close(fd);

	return 0;
}

char *get_rsakeys_from_nv(void)
{
	int fd;
	char *secret;
	struct _nv nv;

	if (search_nv(&nv, NULL) < 0) return NULL;
	if (nv.secret.hdr.magic_number != MAGIC_SECRET) return NULL;

	if ((fd=open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return NULL;
	}
	secret=read_nv(fd, &nv.secret);
	close(fd);
	return secret;
}
#endif /* OPTION_IPSEC */

#ifdef FEATURES_ON_FLASH
int load_features(void *buf, long size)
{
	int fd;

	memset(buf, 0, size);
	if ((fd = open(FILE_FEATURES, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open "FILE_FEATURES);
		return -1;
	}
	read(fd, buf, size);
	close(fd);
	
	return 0;
}

int save_features(void *buf, long size)
{
	int fd;
	int regcount;
	mtd_info_t meminfo;
	
	// Open and size the device
	if ((fd = open(FILE_FEATURES, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "FILE_FEATURES);
		return -1;
	}

	if(ioctl(fd, MEMGETINFO, &meminfo) != 0)
	{
		pr_error(1, "unable to get MTD device info");
		free(buf);
		return -1;
	}
	
	if (size > meminfo.size)
	{
		pr_error(0, "features size is too big (> %d bytes)", meminfo.size);
		return -1;
	}

	if (ioctl(fd, MEMGETREGIONCOUNT, &regcount) == 0)
	{
		if (regcount > 0)
		{
			int i, start;
			region_info_t *reginfo;

			reginfo = calloc(regcount, sizeof(region_info_t));
			for(i = 0; i < regcount; i++)
			{
				reginfo[i].regionindex = i;
				if (ioctl(fd, MEMGETREGIONINFO, &(reginfo[i])) != 0)
				{
					free(reginfo);
					return -1;
				}
			}
			for(start=0, i=0; i < regcount;)
			{
				erase_info_t erase;
				region_info_t *r = &(reginfo[i]);
	
				erase.start = start;
				erase.length = r->erasesize;
				if (ioctl(fd, MEMERASE, &erase) != 0)
				{
					close(fd);
					free(reginfo);
					pr_error(1, "MTD Erase failure");
					return -1;
				}
				start += erase.length;
				if (start >= (r->offset + r->numblocks*r->erasesize))
				{
					i++; // We finished region i so move to region i+1
				}
			}
			free(reginfo);
		}
		else
		{
			erase_info_t erase;
			
			erase.length = meminfo.erasesize;
			for (erase.start=0x0; erase.start < meminfo.size; erase.start += meminfo.erasesize)
			{
				if(ioctl(fd, MEMERASE, &erase) != 0)
				{
					close(fd);
					pr_error(1, "MTD Erase failure");
					return -1;
				}
			}
		}
	}
	write(fd, buf, size);
	close(fd);
	return 0;
}

#else

int load_feature(struct _saved_feature *feature)
{
	int fd, ret=0;
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif

	if ((fd=open(device, O_RDWR)) < 0) return -1;
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_FEATURE_ADDR+0x10*feature->pos, 16, (unsigned char *)feature->key) < 0) goto error;
exit_now:
	close(fd);
	return ret;
error:
	ret=-1;
	goto exit_now;
}

int save_feature(struct _saved_feature *new_feature)
{
	int fd, i, j, ret=0;
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
	unsigned char mac[16], serial[16], feature[2][16];

	if ((fd=open(device, O_RDWR)) < 0) return -1;
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_FEATURE_ADDR+0x10*new_feature->pos, 16, feature[new_feature->pos]) < 0) goto error;
	for (i=0; i < 16; i++) if (feature[new_feature->pos][i] != 255) break;
	if (i < 16)
	{
		if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_MAC_ADDR, 16, mac) < 0) goto error;
		if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_SERIAL_ADDR, 16, serial) < 0) goto error;
		for (j=0; j < 2; j++)
		{
			if (new_feature->pos == j) continue;
			if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_FEATURE_ADDR+0x10*j, 16, feature[j]) < 0) goto error;
		}
		if (i2c_write_block_data(fd, I2C_HC08_ADDR, I2C_HC08_ERASE_ADDR, 0, feature[new_feature->pos]) < 0) goto error;
		sleep(1); /* clear 0x80-0xb0 */
		if (i2c_write_block_data(fd, I2C_HC08_ADDR, I2C_HC08_MAC_ADDR, 16, mac) < 0) goto error;
		sleep(1);
		if (i2c_write_block_data(fd, I2C_HC08_ADDR, I2C_HC08_SERIAL_ADDR, 16, serial) < 0) goto error;
		sleep(1);
		for (j=0; j < 2; j++)
		{
			if (new_feature->pos == j) continue;
			if (i2c_write_block_data(fd, I2C_HC08_ADDR, I2C_HC08_FEATURE_ADDR+0x10*j, 16, feature[j]) < 0) goto error;
			sleep(1);
		}
	}
	if (i2c_write_block_data(fd, I2C_HC08_ADDR, I2C_HC08_FEATURE_ADDR+0x10*new_feature->pos, 16, (unsigned char *)new_feature->key) < 0) goto error;
exit_now:
	close(fd);
	return ret;
error:
	ret=-1;
	goto exit_now;
}

#ifdef I2C_HC08_SERIAL_ADDR
char *get_serial_number(void)
{
	int fd, i;
	static char buf[33];
	unsigned char serial[16];
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
	if ((fd = open(device, O_RDWR)) < 0)
	{
		return NULL;
	}
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_SERIAL_ADDR, 16, serial) < 0)
	{
		close(fd);
		return NULL;
	}
	close(fd);
	for (i=0; i < 16; i++) {
		if (serial[i] != 0xff) break;
	}
	if (i != 16) {
#if 1
		if (((unsigned int *)serial)[3] == 0xFFFFFFFF) /* Old serial number! */
			sprintf(buf, "%u%u%u", ((unsigned int *)serial)[0], ((unsigned int *)serial)[1], ((unsigned int *)serial)[2]);
		else
			sprintf(buf, "%08X%08X%08X%08X", ((unsigned int *)serial)[0], ((unsigned int *)serial)[1], ((unsigned int *)serial)[2], ((unsigned int *)serial)[3]);
#else
		sprintf(buf, "%u%u%u%u", ((unsigned int *)serial)[0], ((unsigned int *)serial)[1], ((unsigned int *)serial)[2], ((unsigned int *)serial)[3]);
#endif
		return buf;
	}
	return NULL;
}
#endif

#ifdef I2C_HC08_ID_ADDR
char *get_system_ID(int mode) /* mode 0: binary; mode 1: ascii; */
{
	int fd, i, j, k;
	static unsigned char buf[33];
	const char bin2asc[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
	if ((fd = open(device, O_RDWR)) < 0)
	{
		return NULL;
	}
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_ID_ADDR, 16, buf) < 0)
	{
		close(fd);
		return NULL;
	}
	close(fd);
	if (mode) /* bin2asc conversion! */
	{
		for(i=15, j=30; i >= 0; i--, j-=2)
		{
			k=buf[i];
			buf[j]=bin2asc[k>>4];
			buf[j+1]=bin2asc[k&0x0F];
		}
		buf[32]=0;
	}
	return (char *)buf;
}
#endif

#ifdef I2C_HC08_PRODUCT
char *get_product_name(void)
{
	int fd;
	static char buf[16];
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
	if ((fd = open(device, O_RDWR)) < 0) {
		return NULL;
	}
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_PRODUCT, 16, buf) < 0) {
		close(fd);
		return NULL;
	}
	close(fd);
	return buf;
}
#endif

#ifdef I2C_HC08_OWNER
char *get_product_owner(void)
{
	int fd;
	static char buf[16];
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
	if ((fd = open(device, O_RDWR)) < 0) {
		return NULL;
	}
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_OWNER, 16, buf) < 0) {
		close(fd);
		return NULL;
	}
	close(fd);
	return buf;
}
#endif

#ifdef I2C_HC08_LICENSED
char *get_product_licensed(void)
{
	int fd;
	static char buf[16];
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
	if ((fd = open(device, O_RDWR)) < 0)
		return NULL;
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_LICENSED, 16, buf) < 0) {
		close(fd);
		return NULL;
	}
	close(fd);
	return buf;
}
#endif

#ifdef I2C_HC08_SERIAL
int get_hc08_serial(void)
{
	int fd;
	static char buf[16];
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
	if ((fd = open(device, O_RDWR)) < 0)
		return 0;
	if (i2c_read_block_data(fd, I2C_HC08_ADDR, I2C_HC08_SERIAL, 16, buf) < 0) {
		close(fd);
		return 0;
	}
	close(fd);
	return *(int *)&buf[0];
}
#endif
#endif

#ifdef CONFIG_BERLIN_SATROUTER

#define	UBOOT_ENV	"/dev/uboot_env"
#define	UBOOT_STARTS	"/dev/uboot_starts"
#define	UBOOT_TRIAL	"/dev/uboot_trial"
#define	SYSTEM_IMAGE	"/dev/system_image"

int get_starts_counter(char *store, unsigned int max_len)
{
	int fd;
	unsigned char data;
	char local_buf[20];
	unsigned int i, fulls, total;

	if((fd = open(UBOOT_STARTS, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	read(fd, &fulls, sizeof(unsigned int));
	total = fulls * (CFG_STARTS_SIZE - sizeof(unsigned int)) * 8;
	for(i=sizeof(unsigned int); i < CFG_STARTS_SIZE; i++)
	{
		read(fd, &data, sizeof(unsigned char));
		if(data == 0x00)	total += 8;
		else
		{
			int j;
			unsigned char subtotal=0;

			for(j=7; j >= 0; j--)
			{
				if(((data >> j) & 0x01) == 0x00)
					subtotal++;
				else
					break;
			}
			total += subtotal;
			break;
		}
	}
	close(fd);
	snprintf(local_buf, 20, "%d", total);
	if(strlen(local_buf) < max_len)	strcpy(store, local_buf);
	return 0;
}

int get_uboot_env(char *name, char *store, int max_len)
{
	int fd, found;
	char search[196];
	unsigned char *p, *init, *limit, *env;

	if( (name == NULL) || (store == NULL) || (max_len == 0) )
		return -1;
	if( strlen(name) > 126 )
		return -1;
	memset(store, 0, max_len);
	sprintf(search, "%s=", name);

	if( (fd = open(UBOOT_ENV, O_RDONLY)) < 0 ) {
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	if( (env = malloc(CFG_ENV_SIZE-sizeof(unsigned long))) == NULL ) {
		pr_error(1, "unable to allocate memory for startup configuration");
		close(fd);
		return -1;
	}
	read(fd, env, sizeof(unsigned long));	/* Descarta CRC no inicio da regiao */
	read(fd, env, CFG_ENV_SIZE - sizeof(unsigned long));
	close(fd);

	p = init = env;
	limit = env + ((CFG_ENV_SIZE - sizeof(unsigned long)) - 1);
	for( ; p < limit; ) {
		for( found=0; p < limit; p++ ) {
			if( *p == '=' ) {
				found++;
				break;
			}
			if(*p == 0)
				break;
		}
		if( found > 0 ) {
			if( (init+strlen(search)) < limit ) {
				if( memcmp(init, search, strlen(search)) == 0 ) {
					p++;
					if( *p != 0 ) {
						strncpy(store, (char *)p, max_len-1);
						store[max_len-1] = 0;
						if( strcmp(name, "serial#") == 0 ) {
							char *local;
							int i, ok = 1;

							for( i=0, local=store; i < strlen(store); i++, local++ ) {
								if( isdigit(*local) == 0 ) {
									ok = 0;
									store[0] = 0;
									break;
								}
							}
							if( ok == 0 )
								break;
							if( strlen(store) < (max_len - 1) ) {
								int k, l, l_max = (max_len < (SATR_SN_LEN+1)) ? max_len : (SATR_SN_LEN+1);

								if( (local = malloc(l_max)) != NULL ) {
									for( k=0, l=0; k < ((l_max-1) - strlen(store)); k++ )
										local[l++] = '0';
									for( k=0; k < strlen(store); k++ )
										local[l++] = store[k];
									local[l] = 0;
									strcpy(store, local);
									free(local);
								}
							}
						}
					}
					break;
				}
			}
			for( ; p < limit; p++ ) {
				if( *p == 0 )
					break;
			}
		}
		p++;
		if( *p == 0 )
			break;
		init = p;
	}
	free(env);
	return strlen(store);
}

int get_trialminutes_counter(char *store, unsigned int max_len)
{
	int fd;
	unsigned char data;
	char local_buf[20];
	unsigned int i, total=0;

	if(!store || !max_len)	return -1;
	store[0] = 0;
	
	if((fd = open(UBOOT_TRIAL, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	for(i=0; i < CFG_TRIAL_SIZE; i++)
	{
		read(fd, &data, sizeof(unsigned char));
		if(data == 0x00)	total += 8;
		else
		{
			int j;
			unsigned char subtotal=0;
			
			for(j=7; j >= 0; j--)
			{
				if(((data >> j) & 0x01) == 0x00)	subtotal++;
				else					break;
			}
			total += subtotal;
			break;
		}
	}
	close(fd);
	snprintf(local_buf, 20, "%d", total);
	if(strlen(local_buf) < max_len)	strcpy(store, local_buf);
	return 0;
}

int get_release_date(unsigned char *buf, unsigned int max_len)
{
	int fd;
	char year[10];
	time_t timestamp;
	struct rtc_time tm;
	struct image_header *hdr;
	unsigned char header[64];

	memset(buf, '\0', max_len);
	if(max_len < 11)	return -1;

	hdr = (struct image_header *) header;
	if((fd = open(SYSTEM_IMAGE, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	read(fd, header, sizeof(struct image_header));	/* Descarta CRC no inicio da regiao */
	close(fd);

	timestamp = (time_t) ntohl(hdr->ih_time);
	to_tm(timestamp, &tm);
	sprintf(year, "%04d", tm.tm_year);
	if(strlen(year) != 4)	return -1;
	sprintf((char *)buf, "%s%02d%02d%02d%02d", &year[2], tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min);
	return 0;
}

/*
 *  Altera uma variavel de ambiente do setor com o eviroment do u-boot.
 *  O u-boot guarda as variaveis de ambiente no formato:
 *	CRC (unsigned long)
 *	"bootargs=data\0"
 *	"bootcmd=data\0"
 *		:
 *		:
 *		:
 *	"bootdelay=0\0"
 *	"\0"
 */
int change_uboot_env(char *name, char *new_value)
{
	char search[128];
	int fd, ok, found;
	erase_info_t erase;
	unsigned long crc, len;
	unsigned char *p, *init, *limit, *end, *env, *tmp = NULL;

	if( !name )
		return -1;
	if( strlen(name) > 126 )
		return -1;
	sprintf(search, "%s=", name);

	if( (fd = open(UBOOT_ENV, O_RDONLY)) < 0 ) {
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	if( !(env = malloc(CFG_ENV_SIZE - sizeof(unsigned long))) ) {
		pr_error(1, "unable to allocate memory for startup configuration");
		close(fd);
		return -1;
	}
	read(fd, env, sizeof(unsigned long)); /* Descarta CRC no inicio da regiao */
	read(fd, env, CFG_ENV_SIZE - sizeof(unsigned long));
	close(fd);

	limit = env + ((CFG_ENV_SIZE - sizeof(unsigned long)) - 1);

	/* Calcula tamanho utilizado */
	for( p=env; p < limit; p++ ) {
		if( (*p == 0) && (*(p + 1) == 0) )
			break;
	}
	if( ((p - env + 1) + (strlen(new_value) + 1)) > (CFG_ENV_SIZE - sizeof(unsigned long) - 1) ) {
		free(env);
		return -1;
	}

	/* Substitui o conteudo */
	for( p=init=env, ok=0; p < limit; ) {
		for( found=0; p < limit; p++ ) {
			if( *p == '=' ) {
				found++;
				break;
			}
			if( *p == '\0' )
				break;
		}
		if( found ) {
			if( (init + strlen(search)) < limit ) {
				if( memcmp(init, search, strlen(search)) == 0 ) {
					for( end=p+1; *end != 0; end++ );
					end++;
					len = CFG_ENV_SIZE - sizeof(unsigned long) - (end - env);
					if( len > 0 ) {
						if( (tmp = malloc(len)) == NULL ) {
							free(env);
							return -1;
						}
						memcpy(tmp, end, len);
					}
					p++;
					strcpy((char *)p, new_value);
					p += (strlen(new_value) + 1);
					if( len > 0 ) {
						memcpy(p, tmp, CFG_ENV_SIZE - sizeof(unsigned long) - (p - env));
						free(tmp);
					}
					ok = 1;
					break;
				}
			}
			for( ; p < limit; p++ ) {
				if( *p == '\0' )
					break;
			}
		}
		p++;
		if( *p == '\0' )
			break;
		init = p;
	}
	if( ok == 0 ) {
		free(env);
		return -1;
	}

	if( (fd = open(UBOOT_ENV, O_RDWR)) < 0 ) {
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(env);
		return -1;
	}

	erase.start = 0;
	erase.length = CFG_ENV_SIZE;
	if( ioctl(fd, MEMERASE, &erase) != 0 ) {
		pr_error(1, "MTD Erase failure");
		close(fd);
		free(env);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);

	crc = crc32(0, env, CFG_ENV_SIZE - sizeof(unsigned long)); /* Update CRC for new data */
	write(fd, &crc, sizeof(unsigned long));
	write(fd, env, CFG_ENV_SIZE - sizeof(unsigned long));
	close(fd);
	free(env);
	return 0;
}

#if 0
int save_board_factory_info(unsigned char **store)
{
	int fd;
	unsigned char *tmp;
	unsigned int len=0;
	
	*store = NULL;
	
	/* Salva regiao da flash com as variaveis de ambiente do u-boot */
	if((fd = open(UBOOT_ENV, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	if( !(tmp = realloc(*store, len+CFG_ENV_SIZE)) )
	{
		pr_error(1, "unable to allocate memory");
		close(fd);
		return -1;
	}
	*store = tmp;
	read(fd, *store+len, CFG_ENV_SIZE);
	len += CFG_ENV_SIZE;
	close(fd);
	
	/* Salva regiao da flash com a contagem de resets */
	if((fd = open(UBOOT_STARTS, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_STARTS);
		free(*store);
		return -1;
	}
	if( !(tmp = realloc(*store, len + CFG_STARTS_SIZE)) )
	{
		pr_error(1, "unable to allocate memory");
		close(fd);
		free(*store);
		return -1;
	}
	*store = tmp;
	read(fd, *store+len, CFG_STARTS_SIZE);
	len += CFG_STARTS_SIZE;
	close(fd);
	
	/* Salva regiao da flash com as informacoes de TRIAL */
	if((fd = open(UBOOT_TRIAL, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_TRIAL);
		free(*store);
		return -1;
	}
	if( !(tmp = realloc(*store, len + CFG_TRIAL_SIZE)) )
	{
		pr_error(1, "unable to allocate memory");
		close(fd);
		free(*store);
		return -1;
	}
	*store = tmp;
	read(fd, *store+len, CFG_TRIAL_SIZE);
	len += CFG_TRIAL_SIZE;
	close(fd);
	
	return len;
}

int restore_board_factory_info(unsigned char **store, unsigned int total_len)
{
	int fd;
	erase_info_t erase;
	unsigned int len=0;
	
	/* Restaura regiao da flash com as variaveis de ambiente do u-boot */
	if((fd = open(UBOOT_ENV, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "UBOOT_ENV);
		return -1;
	}
	erase.start = 0;
	erase.length = CFG_ENV_SIZE;
	if(ioctl(fd, MEMERASE, &erase) != 0)
	{
		pr_error(1, "MTD Erase failure");
		close(fd);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	if( (len + CFG_ENV_SIZE) > total_len )
	{
		close(fd);
		return -1;
	}
	write(fd, *store+len, CFG_ENV_SIZE);
	len += CFG_ENV_SIZE;
	close(fd);
	
	/* Restaura regiao da flash com a contagem de resets */
	if((fd = open(UBOOT_STARTS, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "UBOOT_STARTS);
		return -1;
	}
	erase.start = 0;
	erase.length = CFG_STARTS_SIZE;
	if(ioctl(fd, MEMERASE, &erase) != 0)
	{
		pr_error(1, "MTD Erase failure");
		close(fd);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	if( (len + CFG_STARTS_SIZE) > total_len )
	{
		close(fd);
		return -1;
	}
	write(fd, *store+len, CFG_STARTS_SIZE);
	len += CFG_STARTS_SIZE;
	close(fd);
	
	/* Restaura regiao da flash com as informacoes de TRIAL */
	if((fd = open(UBOOT_TRIAL, O_RDWR)) < 0)
	{
		pr_error(1, "could not open "UBOOT_TRIAL);
		return -1;
	}
	erase.start = 0;
	erase.length = CFG_TRIAL_SIZE;
	if(ioctl(fd, MEMERASE, &erase) != 0)
	{
		pr_error(1, "MTD Erase failure");
		close(fd);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	if( (len + CFG_TRIAL_SIZE) > total_len )
	{
		close(fd);
		return -1;
	}
	write(fd, *store+len, CFG_TRIAL_SIZE);
	len += CFG_TRIAL_SIZE;
	close(fd);
	
	return len;
}
#endif

int board_change_to_licensed(void)
{
	char buf[10];
	unsigned int days;
	
	kill_daemon(TRIAL_DAEMON);
	if(get_uboot_env("trialdays", buf, 9) > 0)
	{
		int fd;
		erase_info_t erase;
		
		sscanf(buf, "%x", &days);
		if(days == 0x0000)	return 0;
		
		if((fd = open(UBOOT_TRIAL, O_RDWR)) < 0)
		{
			pr_error(1, "could not open %s", UBOOT_TRIAL);
			return -1;
		}
		erase.start = 0;
		erase.length = CFG_TRIAL_SIZE;
		if(ioctl(fd, MEMERASE, &erase) != 0)
		{
			pr_error(1, "MTD Erase failure");
			close(fd);
			return -1;
		}
		close(fd);
		
		return (change_uboot_env("trialdays", "0000")<0 ? -1 : 0);
	}
	return -1;
}

void check_starts_counter_region(void)
{
	int fd;
	unsigned int fulls;
	erase_info_t erase;
	
	if((fd = open(UBOOT_STARTS, O_RDWR)) < 0)
		return;
	
	read(fd, &fulls, sizeof(unsigned int));
	if(fulls == 0xFFFFFFFF)
	{	/* Temos uma regiao que foi apagada. Vamos reinicializa-la. */
		lseek(fd, 0, SEEK_SET);
		erase.start = 0;
		erase.length = CFG_STARTS_SIZE;
		if(ioctl(fd, MEMERASE, &erase) != 0)
		{
			pr_error(1, "MTD Erase failure");
			close(fd);
			return;
		}
		lseek(fd, 0, SEEK_SET);
		fulls = 0;
		write(fd, &fulls, sizeof(unsigned int));
	}

#if 0	/* DEBUG */
	{
		int k, start=0;
		unsigned char *p, *data;
		
		lseek(fd, 0, SEEK_SET);
		if( (data = malloc(CFG_STARTS_SIZE)) )
		{
			if( read(fd, data, CFG_STARTS_SIZE) == CFG_STARTS_SIZE )
			{
				printf("Estouros: 0x%08x\n", *((unsigned int *) data));
				for( p=data+sizeof(unsigned int), k=sizeof(unsigned int); k < CFG_STARTS_SIZE; k++, p++ )
				{
					if(*p == 0xFF)
					{
						if(!start)
						{
							start++;
							printf("Byte %d: 0x%02x\n", k, *p);
						}
					}
					else
						printf("Byte %d: 0x%02x\n", k, *p);
				}
				printf("Byte %d(0x%08x): 0x%02x\n", k-1, k-1, *(p-1));
			}
			free(data);
		}
	}
#endif

	close(fd);
}

int inc_starts_counter(void)
{
	int j, fd;
	erase_info_t erase;
	unsigned int i, fulls;
	unsigned char data, new_data;

	check_starts_counter_region();

	if((fd = open(UBOOT_STARTS, O_RDWR)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	read(fd, &fulls, sizeof(unsigned int));
	for(i=sizeof(unsigned int); i < CFG_STARTS_SIZE; i++)
	{
		read(fd, &data, sizeof(unsigned char));
		if(data != 0x00)
		{
			for(j=7; j >= 0; j--)
			{
				if(((data >> j) & 0x01) == 0x01)
				{
					new_data = data & ~(0x80 >> (7-j));
					break;
				}
			}
			break;
		}
	}
	if(i >= CFG_STARTS_SIZE)
	{	/* estouro de contagem */
		lseek(fd, 0, SEEK_SET);
		if(fulls < 0xFFFFFFFF)
			fulls++;
		else
			fulls = 0;
		erase.start = 0;
		erase.length = CFG_STARTS_SIZE;
		if(ioctl(fd, MEMERASE, &erase) != 0)
		{
			pr_error(1, "MTD Erase failure");
			close(fd);
			return -1;
		}
		lseek(fd, 0, SEEK_SET);
		write(fd, &fulls, sizeof(unsigned int));
	}
	else
	{
		lseek(fd, i, SEEK_SET);
		write(fd, &new_data, sizeof(unsigned char));
	}
	close(fd);
	return 0;
}

int board_change_to_trial(unsigned int days)
{
	int fd;
	char tmp[20];
	erase_info_t erase;
	
	exec_daemon(TRIAL_DAEMON);
	if(days > 364)
	{
		pr_error(1, "number of days too long");
		return -1;
	}
	sprintf(tmp, "%04x", days);
	
	if((fd = open(UBOOT_TRIAL, O_RDWR)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_TRIAL);
		return -1;
	}
	erase.start = 0;
	erase.length = CFG_TRIAL_SIZE;
	if(ioctl(fd, MEMERASE, &erase) != 0)
	{
		pr_error(1, "MTD Erase failure");
		close(fd);
		return -1;
	}
	close(fd);
	
	return (change_uboot_env("trialdays", tmp)<0 ? -1 : 0);
}

unsigned int get_mb_info(unsigned int specific, char *store, unsigned int max_len)
{
	FILE *f;
	arg_list argl=NULL;
	char *p, line[256];
	unsigned int i, n, ret = 0;
	unsigned char found_line = 0;

	if( !store || !max_len )
		return 0;
	store[0] = 0;
	if( (f = fopen(MOTHERBOARD_INFO_FILE, "r")) ) {
		while( !feof(f) ) {
			fgets(line, 127, f);
			line[127] = 0;
			if( (n = parse_args_din(line, &argl)) ) {
				switch( specific ) {
					case MBINFO_VENDOR:
						if( strncmp(argl[0], "vendor=", strlen("vendor=")) == 0 )
							found_line = 1;
						break;
					case MBINFO_PRODUCTNAME:
						if( strncmp(argl[0], "product_name=", strlen("product_name=")) == 0 )
							found_line = 1;
						break;
					case MBINFO_COMPLETEDESCR:
						if( strncmp(argl[0], "complete_descr=", strlen("complete_descr=")) == 0 )
							found_line = 1;
						break;
					case MBINFO_PRODUCTCODE:
						if( strncmp(argl[0], "product_code=", strlen("product_code=")) == 0 )
							found_line = 1;
						break;
					case MBINFO_FWVERSION:
						if( strncmp(argl[0], "fw_version=", strlen("fw_version=")) == 0 )
							found_line = 1;
						break;
					case MBINFO_SN:
						if( strncmp(argl[0], "sn=", strlen("sn=")) == 0 )
							found_line = 1;
						break;
					case MBINFO_RELEASEDATE:
						if( strncmp(argl[0], "release_date=", strlen("release_date=")) == 0 )
							found_line = 1;
						break;
					case MBINFO_RESETS:
						if( strncmp(argl[0], "resets=", strlen("resets=")) == 0 )
							found_line = 1;
						break;
				}
				if( found_line ) {
					if( (p = strchr(argl[0], '=')) ) {
						p++;
						if( (ret = strlen(p)) ) {
							if( ret < max_len ) {
								strcpy(store, p);
								for( i=1; i < n; i++ ) {
									if( (strlen(store) + strlen(argl[i]) + 1) < max_len ) {
										strcat(store, " ");
										strcat(store, argl[i]);
									}
								}
							}
							else {
								ret = max_len - 1;
								strncpy(store, p, ret);
								store[ret] = 0;
							}
							free_args_din(&argl);
							fclose(f);
							/* Verifica a necessidade de expandir algum dado */
							switch( specific ) {
								case MBINFO_SN:
									if( ret > 0 ) {
										if( ret < (max_len - 1) ) {
											char local[MODEM_SN_LEN+1];
											int k, l, l_max = (max_len < (MODEM_SN_LEN+1)) ? max_len : (MODEM_SN_LEN+1);

											for( k=0, l=0; k < ((l_max-1) - strlen(store)); k++ )
												local[l++] = '0';
											for( k=0; k < strlen(store); k++ )
												local[l++] = store[k];
											local[l] = 0;
											strcpy(store, local);
										}
									}
									break;
							}
							return ret;
						}
					}
				}
				free_args_din(&argl);
			}
		}
		fclose(f);
	}
	return ret;
}

int inc_trialminutes_counter(void)
{
	int j, fd;
	unsigned int i;
	unsigned char data, new_data;
	
	if((fd = open(UBOOT_TRIAL, O_RDWR)) < 0)
	{
		pr_error(1, "could not open %s", UBOOT_ENV);
		return -1;
	}
	for(i=0; i < CFG_TRIAL_SIZE; i++)
	{
		read(fd, &data, sizeof(unsigned char));
		if(data != 0x00)
		{
			for(j=7; j >= 0; j--)
			{
				if(((data >> j) & 0x01) == 0x01)
				{
					new_data = data & ~(0x80 >> (7-j));
					break;
				}
			}
			break;
		}
	}
	if(i < CFG_STARTS_SIZE)
	{
		lseek(fd, i, SEEK_SET);
		write(fd, &new_data, sizeof(unsigned char));
	}
	close(fd);
	return 0;
}

int print_image_date(void)
{
	int fd;
	time_t timestamp;
	struct rtc_time tm;
	unsigned char header[64];
	struct image_header *hdr = (struct image_header *) header;

	if((fd = open(SYSTEM_IMAGE, O_RDONLY)) < 0)
		return -1;
	read(fd, header, sizeof(struct image_header));
	close(fd);

	timestamp = (time_t) hdr->ih_time;
	to_tm(timestamp, &tm);
	printf("Date: %4d-%02d-%02d  %2d:%02d:%02d UTC\n", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	return 0;
}

#endif	/* CONFIG_BERLIN_SATROUTER */

int load_snmp_secret(char *filename)
{
	int fd;
	char *secret;
	struct _nv nv;

	if( search_nv(&nv, NULL) < 0 )
		return -1;
	if( nv.snmp.hdr.magic_number != MAGIC_SNMP )
		return -1;

	if( (fd = open(DEV_STARTUP_CONFIG, O_RDONLY)) < 0 ) {
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		return -1;
	}
	secret = read_nv(fd, &nv.snmp);
	close(fd);
	if( secret == NULL )
		return -1;

	if( (fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR)) < 0 ) {
		pr_error(1, "could not open %s", filename);
		free(secret);
		return -1;
	}
	write(fd, secret, nv.snmp.hdr.size);
	close(fd);
	free(secret);
	return nv.snmp.hdr.size;
}

int save_snmp_secret(char *filename)
{
	int fd;
	long size;
	unsigned char *secret;
	struct _nv nv;
	struct stat st;
	cfg_header header;

	if( (fd = open(filename, O_RDONLY)) < 0 ) {
		pr_error(1, "could not open %s", filename);
		return -1;
	}
	fstat(fd, &st);
	size = st.st_size;
	if( (secret = malloc(size+1)) == NULL ) {
		pr_error(1, "unable to allocate memory for snmp secret");
		close(fd);
		return -1;
	}
	read(fd, secret, size);
	close(fd);
	secret[size] = 0;

	search_nv(&nv, NULL);
	if( (fd = open(DEV_STARTUP_CONFIG, O_RDWR)) < 0 ) {
		pr_error(1, "could not open "DEV_STARTUP_CONFIG);
		free(secret);
		return -1;
	}
	if( clean_nv(fd, size, &nv) < 0 ) {
		close(fd);
		free(secret);
		return -1;
	}
	header.magic_number = MAGIC_SNMP;
	if( nv.snmp.hdr.magic_number != MAGIC_SNMP )
		header.id = 1;
	else
		header.id = nv.snmp.hdr.id + 1;
	header.size = size;
	header.crc = CalculateCRC32Checksum(secret, size);
	write(fd, &header, sizeof(cfg_header)); /* allow empty config with size == 0 */
	if( size > 0 )
		write(fd, secret, size);
	close(fd);
	free(secret);
	return 0;
}

