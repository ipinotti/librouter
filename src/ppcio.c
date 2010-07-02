/*
 * ppcio.c
 *
 *  Created on: Jun 24, 2010
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

int librouter_ppcio_read(ppcio_data *pd)
{
	int fd, ret;

	fd = open(DEV_PPCIO, O_RDONLY);

	if (fd < 0) {
		librouter_pr_error(1, "can't open %s", DEV_PPCIO);
		return (-1);
	}
#if 0
	ret=ioctl(fd, PPCIO_READ, pd);
#endif
	close(fd);

	return ret;
}

int librouter_ppcio_write(ppcio_data *pd)
{
	int fd, ret;

	fd = open(DEV_PPCIO, O_RDONLY);

	if (fd < 0) {
		librouter_pr_error(1, "can't open %s", DEV_PPCIO);
		return (-1);
	}
#if 0
	ret=ioctl(fd, PPCIO_WRITE, pd);
#endif
	close(fd);

	return ret;
}
