/*
 * ppcio.c
 *
 *  Created on: Jun 24, 2010
 *  Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nv.h"
#include "ppcio.h"
#include "typedefs.h"
#include "error.h"
#include "defines.h"

#define DEV_PPCIO "/dev/ppcio"

int librouter_ppcio_read(struct powerpc_gpio *gpio)
{
	int fd, ret;

	fd = open(DEV_PPCIO, O_RDONLY);

	if (fd < 0) {
		librouter_pr_error(1, "can't open %s", DEV_PPCIO);
		return (-1);
	}

	ret = ioctl(fd, PPCIO_READ, gpio);

	close(fd);

	if (ret < 0)
		librouter_logerr("Could not read GPIO. Port = %d Pin = %d\n",
				gpio->port, gpio->pin);


	return ret;
}

int librouter_ppcio_write(struct powerpc_gpio *gpio)
{
	int fd, ret;

	fd = open(DEV_PPCIO, O_WRONLY);

	if (fd < 0) {
		librouter_pr_error(1, "can't open %s", DEV_PPCIO);
		return (-1);
	}

	ret = write(fd, (void *)gpio, sizeof(struct powerpc_gpio));

	if (ret < 0)
		librouter_logerr("Could not write to GPIO. Port = %d Pin = %d\n",
				gpio->port, gpio->pin);

	close(fd);

	return ret;
}
