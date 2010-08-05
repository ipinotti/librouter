/*
 * config_mapper.c
 *
 *  Created on: Aug 3, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config_mapper.h"

#define ROUTER_CFG_FILE "/var/run/cfg"

static int _set_default_cfg(void)
{
	FILE *f;
	struct router_config cfg;

	memset(&cfg, 0, sizeof(cfg));

	f = fopen(ROUTER_CFG_FILE, "wb");

	if (!f) {
		librouter_pr_error(1, "Can't write configuration");
		return (-1);
	}

	fwrite(&cfg, sizeof(struct router_config), 1, f);
	fclose(f);

	return 0;
}

static int _check_cfg(void)
{
	struct stat st;

	if (stat(ROUTER_CFG_FILE, &st))
		return _set_default_cfg();

	return 0;
}

struct router_config* librouter_config_mmap_cfg(void)
{
	int fd;
	struct router_config *cfg = NULL;

	_check_cfg();

	if ((fd = open(ROUTER_CFG_FILE, O_RDWR)) < 0) {
		librouter_pr_error(1, "Could not open configuration");
		return NULL;
	}

	cfg = mmap(NULL, sizeof(struct router_config), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (cfg == ((void *) -1)) {
		librouter_pr_error(1, "Could not open configuration");
		return NULL;
	}

	close(fd);

	/* debug persistent */
	librouter_debug_recover_all();

	return cfg;
}

int librouter_config_munmap_cfg(struct router_config *cfg)
{
	return (munmap(cfg, sizeof(struct router_config)) < 0);
}
