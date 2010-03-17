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
#include <libconfig/nv.h>
#include <libconfig/ppcio.h>
#include <libconfig/typedefs.h>
#include <libconfig/error.h>
#include <libconfig/defines.h>

#if defined(CONFIG_DEVFS_FS)
#define DEV_PPCIO "/dev/misc/ppcio"
#else
#define DEV_PPCIO "/dev/ppcio"
#endif

int read_ppcio(ppcio_data *pd)
{
	int fd, ret;
	
	fd=open(DEV_PPCIO, O_RDONLY);
	if (fd < 0)
	{
		pr_error(1, "can't open %s", DEV_PPCIO);
		return (-1);
	}
	ret=ioctl(fd, PPCIO_READ, pd);
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
	ret=ioctl(fd, PPCIO_WRITE, pd);
	close(fd);
	return ret;
}
#ifdef CONFIG_BERLIN_SATROUTER
int get_board_hw_id(void)
{
	int fd, ret;
	ppcio_data pd;

#ifdef CONFIG_DEVELOPMENT
	return BOARD_HW_ID_0;
#endif

	if((fd = open(DEV_PPCIO, O_RDONLY)) < 0)
		return -1;
	ret = ioctl(fd, PPCIO_READ, &pd);
	close(fd);
	return ((ret < 0) ? ret : ((int) pd.hw_id));
}
#endif

