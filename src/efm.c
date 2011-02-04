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
#include "efm.h"

static int _do_ioctl(unsigned long int cmd, void *arg)
{
	int dev_fd;
	int ret = 0;

	dev_fd = open(ORIONPLUS_DEV, O_NONBLOCK);
	if (dev_fd < 0) {
		printf("error opening %s: %s\n", ORIONPLUS_DEV, strerror(errno));
		return -1;
	}

	if (ioctl(dev_fd, cmd, arg) < 0) {
		printf("ioctl error : %s\n", strerror(errno));
		ret = -1;
	}

	close(dev_fd);

	return ret;
}

int librouter_efm_get_mode(void)
{
	struct orionplus_conf conf;

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	return conf.mode;
}

int librouter_efm_set_mode(int mode)
{
	struct orionplus_conf conf;
	struct orionplus_stat stat[4];

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	if (_do_ioctl(ORIONPLUS_GETSTATUS, &stat))
		return -1;

	if (stat[0].channel_st != CHANNEL_STATE_SHUTDOWN) {
		printf("(EFM) Interface must be shutdown first\n");
		return -1;
	}

	conf.mode = mode;

	return _do_ioctl(ORIONPLUS_SETMODE, &conf);
}

int librouter_efm_enable(int enable)
{
	struct orionplus_conf conf;
	struct orionplus_stat stat[4];

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	if (_do_ioctl(ORIONPLUS_GETSTATUS, &stat))
		return -1;

	/* The channels states are the same, so we can
	 * test only the first one */
	if (enable && stat[0].channel_st != CHANNEL_STATE_SHUTDOWN)
		return 0; /* Already enabled */
	else if (!enable && stat[0].channel_st == CHANNEL_STATE_SHUTDOWN)
		return 0; /* Already disabled */

	conf.action = enable ? GTI_STARTUP_REQ : GTI_ABORT_REQ;
	return _do_ioctl(ORIONPLUS_SET_PARAM, &conf);
}

int librouter_efm_get_status(struct orionplus_stat *st)
{
	memset(st, 0, sizeof(struct orionplus_stat));

return _do_ioctl(ORIONPLUS_GETSTATUS, st);
}

int librouter_efm_get_channel_state_string(enum channel_state st, char *buf, int len)
{
	switch (st) {
	case CHANNEL_STATE_CONNECTED:
		snprintf(buf, len, "Connected");
		break;
	case CHANNEL_STATE_DISCONNECTED:
		snprintf(buf, len, "Disconnected");
		break;
	case CHANNEL_STATE_SHUTDOWN:
		snprintf(buf, len, "Shutdown");
		break;
	case CHANNEL_STATE_HANDSHAKING:
		snprintf(buf, len, "Handshaking");
		break;
	default:
		break;
	}

	return 0;
}

int librouter_efm_get_num_channels(void)
{
	int n = 0;

	_do_ioctl(ORIONPLUS_GETNUMCHANNELS, &n);

	return n;
}

#endif /* OPTION_EFM */
