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
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <syslog.h>

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

int librouter_efm_get_mode(struct orionplus_conf *conf)
{
	if (_do_ioctl(ORIONPLUS_GETCONFIG, conf))
		return -1;

	return 0;
}

int librouter_efm_set_mode(struct orionplus_conf *conf)
{
	struct orionplus_stat stat[4];

	if (_do_ioctl(ORIONPLUS_GETSTATUS, &stat))
		return -1;

	if (stat[0].channel_st != CHANNEL_STATE_SHUTDOWN) {
		printf("(EFM) Interface must be shutdown first\n");
		syslog(LOG_ERR, "(EFM) Interface must be shutdown first\n");
		return -1;
	}

	if (conf->mode == GTI_CO) {
		int n, i;

		n = conf->linerate / 64;
		i = (conf->linerate%64) / 8;

		/* G991.2 Annex F */
		switch (conf->modulation) {
		case GTI_16_TCPAM_MODE:
			if ((n > 60) || (n == 60 && i > 0)) {
				printf("%% Invalid rate for 16-TCPAM\n");
				return -1;
			}
			break;
		case GTI_32_TCPAM_MODE:
			if ((n < 12) || (n == 89 && i > 0)) {
				printf("%% Invalid rate for 32-TCPAM\n");
				return -1;
			}
			break;
		default:
			break;
		}

	}

	return _do_ioctl(ORIONPLUS_SETMODE, conf);
}

int librouter_efm_get_force_bonding(void)
{
	struct orionplus_conf conf;

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	return conf.force_bonding;
}

int librouter_efm_set_force_bonding(int enable)
{
	struct orionplus_conf conf;

	conf.force_bonding = enable;

	return _do_ioctl(ORIONPLUS_FORCEBONDING, &conf);
}

int librouter_efm_get_retrain_criteria_msk(void)
{
	struct orionplus_conf conf;

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	return conf.retrain_criteria_mask;
}

int librouter_efm_set_retrain_criteria_msk(int msk)
{
	struct orionplus_conf conf;

	conf.retrain_criteria_mask = msk;

	return _do_ioctl(ORIONPLUS_SETRETRAINMSK, &conf);
}

int librouter_efm_enable(int enable)
{
	struct orionplus_conf conf;
	struct orionplus_stat stat[4];
	int i, ret;

	if (_do_ioctl(ORIONPLUS_GETCONFIG, &conf))
		return -1;

	if (_do_ioctl(ORIONPLUS_GETSTATUS, &stat))
		return -1;

	/* The channels states are the same, so we can
	 * test only the first one */
	if (enable && stat[0].channel_st != CHANNEL_STATE_SHUTDOWN)
		return 0; /* Already enabled */
#if 0
	else if (!enable && stat[0].channel_st == CHANNEL_STATE_SHUTDOWN)
		return 0; /* Already disabled */
#endif

	conf.action = enable ? GTI_STARTUP_REQ : GTI_ABORT_REQ;

	for (i = 0; i < 4; i++) {
		conf.channel = i;
		ret = _do_ioctl(ORIONPLUS_SET_PARAM, &conf);
		if (ret < 0)
			break;
#if 0 /* Delay between channel connection */
		if (enable)
			sleep(6);
#endif
	}

	return ret;
}

int librouter_efm_reset(void)
{
	if (_do_ioctl(ORIONPLUS_RESETDSP, NULL))
		return -1;

	return 0;
}

int librouter_efm_get_status(struct orionplus_stat *st)
{
	memset(st, 0, sizeof(struct orionplus_stat) * 4);

	return _do_ioctl(ORIONPLUS_GETSTATUS, st);
}

int librouter_efm_get_counters(struct orionplus_counters *cnt)
{
	memset(cnt, 0, sizeof(struct orionplus_counters));

	return _do_ioctl(ORIONPLUS_GETCOUNTERS, cnt);
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

/* MIB related functions */

static struct orionplus_stat *_get_channel_stat(int channel)
{
	struct orionplus_stat stat[4];
	struct orionplus_stat *ch_stat = malloc(sizeof(struct orionplus_stat));

	if (ch_stat == NULL) {
		printf("%s : Could not allocate memory\n", __FUNCTION__);
		return NULL;
	}

	if (librouter_efm_get_num_channels() < (channel + 1)) {
		printf("No such channel %d\n", channel);
		return NULL;
	}

	if (librouter_efm_get_status(&stat[0]) < 0)
		return NULL;

	memcpy(ch_stat, &stat[channel], sizeof(struct orionplus_stat));

	return ch_stat;
}

static struct orionplus_stat *_get_channel_cnt(int channel)
{
	struct orionplus_stat stat[4];
	struct orionplus_stat *ch_stat = malloc(sizeof(struct orionplus_stat));

	if (ch_stat == NULL) {
		printf("%s : Could not allocate memory\n", __FUNCTION__);
		return NULL;
	}

	if (librouter_efm_get_num_channels() < (channel + 1)) {
		printf("No such channel %d\n", channel);
		return NULL;
	}

	if (librouter_efm_get_status(&stat[0]) < 0)
		return NULL;

	memcpy(ch_stat, &stat[channel], sizeof(struct orionplus_stat));

	return ch_stat;
}

float librouter_efm_get_snr(int channel)
{
	struct orionplus_stat *st = _get_channel_stat(channel);
	float snr = 1.0;

	if (st == NULL)
		return -1;

	//snr = 58.4 - (10 * log10(st->mean_sq_err[0]));
	snr = 58.4 - (10 * log10(st->avg_mean_sq_err));

	free(st);
	return snr;
}

float librouter_efm_get_snr_margin(int channel)
{
	float snr = librouter_efm_get_snr(channel);

	snr -= 22.7;

	return snr;
}

float librouter_efm_get_data_mode_margin(int channel)
{
	struct orionplus_stat *st = _get_channel_stat(channel);
	float margin = 1.0;

	if (st == NULL)
		return -1;

	//margin = 10 * log10 (st->data_mode_margin[0]/st->data_mode_margin[1]);
	margin = 10 * log10 (st->avg_data_mode_margin);

	free(st);

	return margin;
}


float librouter_efm_get_loop_attn(int channel)
{
	struct orionplus_stat *st = _get_channel_stat(channel);
	float attn = 1.0, x;

	if (st == NULL)
		return -1;

	x = (float) st->loop_attn[0];
	attn = x/10;

	free(st);
	return attn;
}

float librouter_efm_get_xmit_power(int channel)
{
	struct orionplus_stat *st = _get_channel_stat(channel);
	float pwr = 1.0, x;

	if (st == NULL)
		return -1;

	x = (float) st->xmit_power[0];
	pwr = x/10;

	free(st);
	return pwr;
}

float librouter_efm_get_receiver_gain(int channel)
{
	struct orionplus_stat *st = _get_channel_stat(channel);
	float gain = 1.0, x;

	if (st == NULL)
		return -1;

	x = (float) st->rec_gain[0];
	gain = 20 * log10(x/0x800);

	x = (float) st->rec_gain[1];
	gain += 20 * log10(x/0x200);

	x = (float) st->rec_gain[2];
	gain += x/10;

	free(st);
	return gain;
}

int librouter_efm_get_los(int channel)
{
	struct orionplus_stat *st = _get_channel_stat(channel);

	return 0;
}

int librouter_efm_get_es(int channel)
{
	struct orionplus_counters cnt;

	if (librouter_efm_get_counters(&cnt) < 0) {
		syslog(LOG_ERR, "%s : Could not get EFM counters\n", __FUNCTION__);
		return -1;
	}

	return cnt.xcvr_cnt[channel].errored_sec;
}



#endif /* OPTION_EFM */
