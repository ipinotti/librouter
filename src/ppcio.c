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

int read_ppcio(ppcio_data *pd)
{
	int fd, ret;
	
	fd=open(DEV_PPCIO, O_RDONLY);
	if (fd < 0)
	{
		pr_error(1, "can't open %s", DEV_PPCIO);
		return (-1);
	}
#if 0
	ret=ioctl(fd, PPCIO_READ, pd);
#endif
	close(fd);
	return ret;
}

int write_ppcio(ppcio_data *pd)
{
	int fd, ret;
	
	fd=open(DEV_PPCIO, O_RDONLY);
	if (fd < 0)
	{
		pr_error(1, "can't open %s", DEV_PPCIO);
		return (-1);
	}
#if 0
	ret=ioctl(fd, PPCIO_WRITE, pd);
#endif
	close(fd);
	return ret;
}
