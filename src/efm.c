/*
 * efm.c
 *
 *  Created on: Nov 10, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>

#include "options.h"

#ifdef OPTION_EFM
#include <ikanos/orionplus.h>

static int _do_ioctl(unsigned long int cmd, struct orionplus_conf *conf)
{
	int dev_fd;
	int ret = 0;

	dev_fd = open(ORIONPLUS_DEV, O_NONBLOCK);
	if (dev_fd < 0) {
		printf("error opening %s: %s\n", ORIONPLUS_DEV, strerror(errno));
		return -1;
	}

	if (ioctl(dev_fd, cmd, (void *) conf) < 0) {
		printf("ioctl error : %s\n", strerror(errno));
		ret = -1;
	}

	close(dev_fd);

	return ret;
}

int librouter_efm_init_chip(int num_of_channels)
{
	struct orionplus_conf conf;

	conf.num_channels = num_of_channels;

	return _do_ioctl(ORIONPLUS_INITCHIP, &conf);
}

int librouter_efm_get_num_channels(void)
{
	struct orionplus_conf conf;

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	return conf.num_channels;
}

int librouter_efm_set_mode(int mode)
{
	struct orionplus_conf conf;

	conf.mode = mode;

	return _do_ioctl(ORIONPLUS_SETMODE, &conf);
}

int librouter_efm_get_mode(void)
{
	struct orionplus_conf conf;

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	return conf.mode;
}

#endif /* OPTION_EFM */
