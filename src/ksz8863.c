/*
 * ksz8863.c
 *
 *  Created on: Nov 17, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/autoconf.h>

#include "options.h"

#ifdef OPTION_MANAGED_SWITCH

#include "i2c-dev.h"
#include "ksz8863.h"

/* type, port[NUMBER_OF_SWITCH_PORTS]*/
port_family_switch _switch_ksz_ports[] = {
	{ real_sw,  {0,1} },
	{ alias_sw, {1,2} },
	{ non_sw,   {0,0} }
};

/* Low level I2C functions */
static int _ksz8863_reg_read(__u8 reg, __u8 *buf, __u8 len)
{
	int dev;

	dev = open(KSZ8863_I2CDEV, O_NONBLOCK);
	if (dev < 0) {
		printf("error opening %s: %s\n", KSZ8863_I2CDEV, strerror(errno));
		return -1;
	}

	if (ioctl(dev, I2C_SLAVE, KSZ8863_I2CADDR) < 0) {
		printf("%% %s : Could not set slave address\n", __FUNCTION__);
		close(dev);
		return -1;
	}

	*buf = i2c_smbus_read_byte_data(dev, reg);

	ksz8863_dbg("addr = %02x data = %02x\n", reg, *buf)

	close(dev);
	return 0;
}

static int _ksz8863_reg_write(__u8 reg, __u8 *buf, __u8 len)
{
	int dev;

	ksz8863_dbg("addr = %02x data = %02x\n", reg, *buf)

	dev = open(KSZ8863_I2CDEV, O_NONBLOCK);
	if (dev < 0) {
		printf("error opening %s: %s\n", KSZ8863_I2CDEV, strerror(errno));
		return -1;
	}

	if (ioctl(dev, I2C_SLAVE, KSZ8863_I2CADDR) < 0) {
		printf("%% %s : Could not set slave address\n", __FUNCTION__);
		close(dev);
		return -1;
	}

	if (i2c_smbus_write_byte_data(dev, reg, *buf) < 0) {
		printf("%s : Error writing to device\n", __FUNCTION__);
		close(dev);
		return -1;
	}

	close(dev);
	return 0;
}

/******************/
/* For tests only */
/******************/
int librouter_ksz8863_read(__u8 reg)
{
	__u8 data;

	_ksz8863_reg_read(reg, &data, sizeof(data));

	return data;
}

int librouter_ksz8863_write(__u8 reg, __u8 data)
{
	return _ksz8863_reg_write(reg, &data, sizeof(data));
}

/******************************************************/
/********** Exported functions ************************/
/******************************************************/

/**
 * Função retorna porta switch "alias" utilizada no cish e web através da
 * porta switch real correspondente passada por parâmetro
 *
 * @param switch_port
 * @return alias_switch_port if ok, -1 if not
 */
int librouter_ksz8863_get_aliasport_by_realport(int switch_port)
{
	int i;

	for (i=0; i < NUMBER_OF_SWITCH_PORTS; i++){
		if ( _switch_ksz_ports[real_sw].port[i] == switch_port ){
			return _switch_ksz_ports[alias_sw].port[i];
		}
	}
	return -1;
}

/**
 * Função retorna porta switch correspondente através da porta "alias"
 * utilizada no cish e web
 *
 * @param switch_port
 * @return real_switch_port if ok, -1 if not
 */
int librouter_ksz8863_get_realport_by_aliasport(int switch_port)
{
	int i;

	for (i=0; i < NUMBER_OF_SWITCH_PORTS; i++){
		if ( _switch_ksz_ports[alias_sw].port[i] == switch_port ){
			return _switch_ksz_ports[real_sw].port[i];
		}
	}
	return -1;
}

/**
 * librouter_ksz8863_set_broadcast_storm_protect
 *
 * Enable or Disable Broadcast Storm protection
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_broadcast_storm_protect(int enable, int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863REG_ENABLE_BC_STORM_PROTECT_MSK;
	else
		data &= ~KSZ8863REG_ENABLE_BC_STORM_PROTECT_MSK;

	if (_ksz8863_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_broadcast_storm_protect
 *
 * Get if Broadcast Storm protection is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_broadcast_storm_protect(int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	return (data & KSZ8863REG_ENABLE_BC_STORM_PROTECT_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8863_set_storm_protect_rate
 *
 * Set maximum incoming rate for broadcast packets
 *
 * @param percentage: 0 to 100
 * @return 0 if success, -1 if failure
 */
int librouter_ksz8863_set_storm_protect_rate(unsigned int percentage)
{
	__u8 reg, data;
	__u16 tmp;

	if (percentage > 100) {
		printf("%% Invalid percentage value : %d\n", percentage);
		return -1;
	}

	/* 99 packets/interval corresponds to 1% - See Page 52 */
	tmp = percentage * 99;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL4, &data, sizeof(data)))
		return -1;

	data &= ~0x07; /* Clear old rate value - first 3 bits */
	data |= (tmp >> 8); /* Get the 3 most significant bits */

	if (_ksz8863_reg_write(KSZ8863REG_GLOBAL_CONTROL4, &data, sizeof(data)))
		return -1;

	/* Now write the less significant bits */
	data = (__u8) (tmp & 0x00ff);
	if (_ksz8863_reg_write(KSZ8863REG_GLOBAL_CONTROL5, &data, sizeof(data)))
	return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_storm_protect_rate
 *
 * Get maximum incoming rate for broadcast packets
 *
 * @return -1 if error, rate percentage otherwise
 */
int librouter_ksz8863_get_storm_protect_rate(void)
{
	__u8 rate_u, rate_l;
	int perc;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL4, &rate_u, sizeof(rate_u)))
		return -1;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL5, &rate_l, sizeof(rate_l)))
		return -1;

	rate_u &= 0x07; /* Get rate bits [2-0] */

	perc = ((rate_u << 8) | rate_l);
	perc /= 99;

	return perc;
}

/**
 * librouter_ksz8863_set_multicast_storm_protect
 *
 * Enable or Disable Multicast Storm protection
 *
 * @param enable
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_multicast_storm_protect(int enable)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL2, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863REG_ENABLE_MC_STORM_PROTECT_MSK;
	else
		data &= ~KSZ8863REG_ENABLE_MC_STORM_PROTECT_MSK;

	if (_ksz8863_reg_write(KSZ8863REG_GLOBAL_CONTROL2, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_multicast_storm_protect
 *
 * Get if Multicast Storm protection is enabled or disabled
 *
 * @param void
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_multicast_storm_protect(void)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL2, &data, sizeof(data)))
		return -1;

	return ((data & KSZ8863REG_ENABLE_MC_STORM_PROTECT_MSK) ? 1 : 0);
}

/**
 * librouter_ksz8863_set_replace_null_vid
 *
 * Enable or Disable NULL VID replacement with port VID
 *
 * @param enable
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_replace_null_vid(int enable)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL2, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863_ENABLE_REPLACE_NULL_VID_MSK;
	else
		data &= ~KSZ8863_ENABLE_REPLACE_NULL_VID_MSK;

	if (_ksz8863_reg_write(KSZ8863REG_GLOBAL_CONTROL4, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_replace_null_vid
 *
 * Get NULL VID replacement configuration
 *
 * @param void
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_replace_null_vid(void)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL4, &data, sizeof(data)))
		return -1;

	return ((data & KSZ8863_ENABLE_REPLACE_NULL_VID_MSK) ? 1 : 0);
}

/**
 * librouter_ksz8863_set_egress_rate_limit
 *
 * Set rate limit for a certain port and priority queue.
 * The rate will be rounded to multiples of 1Mbps or
 * multiples of 64kbps if values are lower than 1Mbps.
 *
 * @param port	From 0 to 2
 * @param prio	From 0 to 3
 * @param rate	In Kbps
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_egress_rate_limit(int port, int prio, int rate)
{
	__u8 reg, data;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8863REG_EGRESS_RATE_LIMIT;
	reg += 0x4 * port;
	reg += prio;

	if (!rate || rate == 100000) {
		data = (__u8) 0;
	} else if (rate >= 1000) {
		data = (__u8) (rate/1000);
	} else {
		data = 0x65;
		data += (__u8) (rate/64 - 1);
	}

	return _ksz8863_reg_write(reg, &data, sizeof(data));
}

		/**
		 * librouter_ksz8863_get_egress_rate_limit
		 *
		 * Get rate limit from a certain port and priority queue
		 *
		 * @param port
		 * @param prio
		 * @return The configured rate limit or -1 if error
		 */
int librouter_ksz8863_get_egress_rate_limit(int port, int prio)
{
	__u8 reg, data;
	int rate;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8863REG_EGRESS_RATE_LIMIT;
	reg += 0x4 * port;
	reg += prio;

	if (_ksz8863_reg_read(reg, &data, sizeof(data) < 0))
		return -1;
	if (!data)
		rate = 100000;
	else if (data < 0x65)
		rate = data * 1000;
	else
		rate = data * 64;

	return rate;
}

/**
 * librouter_ksz8863_set_ingress_rate_limit
 *
 * Set rate limit for a certain port and priority queue.
 * The rate will be rounded to multiples of 1Mbps or
 * multiples of 64kbps if values are lower than 1Mbps.
 *
 * @param port	From 0 to 2
 * @param prio	From 0 to 3
 * @param rate	In Kbps
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_ingress_rate_limit(int port, int prio, int rate)
{
	__u8 reg, data;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8863REG_INGRESS_RATE_LIMIT;
	reg += 0x10 * port;
	reg += prio;

	if (!rate || rate == 100000) {
		data = (__u8) 0;
	} else if (rate >= 1000) {
		data = (__u8) (rate/1000);
	} else {
		data = 0x65;
		data += (__u8) (rate/64 - 1);
	}

	return _ksz8863_reg_write(reg, &data, sizeof(data));
}

/**
 * librouter_ksz8863_get_ingress_rate_limit
 *
 * Get ingress rate limit from a certain port and priority queue
 *
 * @param port
 * @param prio
 * @return The configured rate limit or -1 if error
 */
int librouter_ksz8863_get_ingress_rate_limit(int port, int prio)
{
	__u8 reg, data;
	int rate;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8863REG_INGRESS_RATE_LIMIT;
	reg += 0x10 * port;
	reg += prio;

	if (_ksz8863_reg_read(reg, &data, sizeof(data) < 0))
		return -1;
	if (!data)
		rate = 100000;
	else if (data < 0x65)
		rate = data * 1000;
	else
		rate = data * 64;

	return rate;
}

/**
 * librouter_ksz8863_set_8021q	Enable/disable 802.1q (VLAN)
 *
 * @param enable
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_set_8021q(int enable)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL3, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863_ENABLE_8021Q_MSK;
	else
		data &= ~KSZ8863_ENABLE_8021Q_MSK;

	if (_ksz8863_reg_write(KSZ8863REG_GLOBAL_CONTROL3, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_8021q	Get if 802.1q is enabled or disabled
 *
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_8021q(void)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL3, &data, sizeof(data)))
		return -1;

	return ((data & KSZ8863_ENABLE_8021Q_MSK) ? 1 : 0);
}

/**
 * librouter_ksz8863_set_wfq	Enable/disable Weighted Fair Queueing
 *
 * @param enable
 * @return 0 if success, -1 otherwise
 */
int librouter_ksz8863_set_wfq(int enable)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL3, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863_ENABLE_WFQ_MSK;
	else
		data &= ~KSZ8863_ENABLE_WFQ_MSK;

	if (_ksz8863_reg_write(KSZ8863REG_GLOBAL_CONTROL3, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_wfq	Get if WFQ is enabled or disabled
 *
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_wfq(void)
{
	__u8 data;

	if (_ksz8863_reg_read(KSZ8863REG_GLOBAL_CONTROL3, &data, sizeof(data)))
		return -1;

	return ((data & KSZ8863_ENABLE_WFQ_MSK) ? 1 : 0);
}

/**
 * librouter_ksz8863_set_default_vid	Set port default 802.1q VID
 *
 * @param port
 * @param vid
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_set_default_vid(int port, int vid)
{
	__u8 vid_l, vid_u;
	__u8 reg, data;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (vid > 4095) {
		printf("%% Invalid 802.1q VID : %d\n", vid);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL3;
	reg += 0x10 * port;

	vid_l = (__u8) (vid & 0xff);
	vid_u = (__u8) ((vid >> 8) & KSZ8863REG_DEFAULT_VID_UPPER_MSK);

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
	return -1;

	/* Set upper part of VID */
	vid_u |= (data & (~KSZ8863REG_DEFAULT_VID_UPPER_MSK));

	if (_ksz8863_reg_write(reg, &vid_u, sizeof(vid_u)) < 0)
	return -1;

	if (_ksz8863_reg_write(reg+1, &vid_l, sizeof(vid_l)) < 0)
	return -1;

	return 0;
}

	/**
	 * librouter_ksz8863_get_default_vid	Get port's default 802.1q VID
	 *
	 * @param port
	 * @return Default VID if success, -1 if error
	 */
int librouter_ksz8863_get_default_vid(int port)
{
	__u8 vid_l, vid_u;
	__u8 reg, data;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL3;
	reg += 0x10 * port;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	vid_u = data & KSZ8863REG_DEFAULT_VID_UPPER_MSK;

	if (_ksz8863_reg_read(++reg, &data, sizeof(data)))
		return -1;

	vid_l = data;

	return ((int) (vid_u << 8 | vid_l));
}

/**
 * librouter_ksz8863_set_default_cos	Set port default 802.1p CoS
 *
 * @param port
 * @param cos
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_set_default_cos(int port, int cos)
{
	__u8 reg, data;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (cos > 7) {
		printf("%% Invalid 802.1p CoS : %d\n", cos);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL3;
	reg += 0x10 * port;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	data &= ~KSZ8863REG_DEFAULT_COS_MSK;
	data |= (cos << 5) & KSZ8863REG_DEFAULT_COS_MSK; /* Set bits [7-5] */

	if (_ksz8863_reg_write(reg, &data, sizeof(data)) < 0)
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_default_cos	Get port's default 802.1q CoS
 *
 * @param port
 * @return Default CoS if success, -1 if error
 */
int librouter_ksz8863_get_default_cos(int port)
{
	__u8 reg, data;

	if (port < 0 || port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL3;
	reg += 0x10 * port;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	return ((int) (data >> 5));
}

/**
 * librouter_ksz8863_set_8021p
 *
 * Enable or Disable 802.1p classification
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_8021p(int enable, int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863REG_ENABLE_8021P_MSK;
	else
		data &= ~KSZ8863REG_ENABLE_8021P_MSK;

	if (_ksz8863_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_8021p
 *
 * Get if 802.1p classification is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_8021p(int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	return (data & KSZ8863REG_ENABLE_8021P_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8863_set_diffserv
 *
 * Enable or Disable Diff Service classification
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_diffserv(int enable, int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863REG_ENABLE_DIFFSERV_MSK;
	else
		data &= ~KSZ8863REG_ENABLE_DIFFSERV_MSK;

	if (_ksz8863_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_diffserv
 *
 * Get if Diff Service classification is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_diffserv(int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	return (data & KSZ8863REG_ENABLE_DIFFSERV_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8863_set_taginsert
 *
 * Enable or Disable 802.1p/q tag insertion on untagged packets
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_taginsert(int enable, int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863REG_ENABLE_TAGINSERT_MSK;
	else
		data &= ~KSZ8863REG_ENABLE_TAGINSERT_MSK;

	if (_ksz8863_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_taginsert
 *
 * Get if 802.1p/q tag insertion on untagged packets is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_taginsert(int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	return (data & KSZ8863REG_ENABLE_TAGINSERT_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8863_set_txqsplit
 *
 * Enable or Disable TXQ Split into 4 queues
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8863_set_txqsplit(int enable, int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	if (enable)
		data |= KSZ8863REG_ENABLE_TXQSPLIT_MSK;
	else
		data &= ~KSZ8863REG_ENABLE_TXQSPLIT_MSK;

	if (_ksz8863_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_txqsplit
 *
 * Get if TXQ split configuration
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8863_get_txqsplit(int port)
{
	__u8 reg, data;

	if (port > 2) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8863REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	return (data & KSZ8863REG_ENABLE_TXQSPLIT_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8863_set_dscp_prio
 *
 * Set the packet priority based on DSCP value
 *
 * @param dscp
 * @param prio
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_set_dscp_prio(int dscp, int prio)
{
	__u8 reg, data, mask;
	int offset;

	if (dscp < 0 || dscp > 63) {
		printf("%% Invalid DSCP value : %d\n", dscp);
		return -1;
	}

	if (prio < 0 || prio > 3) {
		printf("%% Invalid priority : %d\n", prio);
		return -1;
	}

	reg = KSZ8863REG_TOS_PRIORITY_CONTROL;
	reg += dscp / 4;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	/*
	 * See KSZ8863 Datasheet Page 65  for the register description
	 * (DSCP mod 4) will give the offset:
	 * 	- rest 0: bits [1-0]
	 * 	- rest 1: bits [3-2]
	 * 	- rest 2: bits [5-4]
	 * 	- rest 3: bits [7-6]
	 */
	offset = (dscp % 4) * 2;
	mask = 0x3 << offset;
	data &= ~mask; /* Clear current config */
	data |= (prio << offset);

	if (_ksz8863_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_dscp_prio
 *
 * Get the packet priority based on DSCP value
 *
 * @param dscp
 * @return priority value if success, -1 if error
 */
int librouter_ksz8863_get_dscp_prio(int dscp)
{
	__u8 reg, data;
	int prio;

	if (dscp < 0 || dscp > 63) {
		printf("%% Invalid DSCP value : %d\n", dscp);
		return -1;
	}

	reg = KSZ8863REG_TOS_PRIORITY_CONTROL;
	reg += dscp / 4;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	prio = (int) ((data >> (dscp % 4) * 2) & 0x3);

	return prio;
}

/**
 * librouter_ksz8863_set_cos_prio
 *
 * Set the packet priority based on DSCP value
 *
 * @param cos
 * @param prio
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_set_cos_prio(int cos, int prio)
{
	__u8 reg, data, mask;
	int offset;

	if (cos < 0 || cos > 7) {
		printf("%% Invalid CoS value : %d\n", cos);
		return -1;
	}

	if (prio < 0 || prio > 3) {
		printf("%% Invalid priority : %d\n", prio);
		return -1;
	}

	reg = KSZ8863REG_GLOBAL_CONTROL10;
	reg += cos / 4;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	/*
	 * See KSZ8863 Datasheet Page 53  for the register description
	 * (CoS mod 4) will give the offset:
	 * 	- mod 0: bits [1-0]
	 * 	- mod 1: bits [3-2]
	 * 	- mod 2: bits [5-4]
	 * 	- mod 3: bits [7-6]
	 */
	offset = (cos % 4) * 2;
	mask = 0x3 << offset;
	data &= ~mask; /* Clear current config */
	data |= (prio << offset);

	if (_ksz8863_reg_write(reg, &data, sizeof(data)))
		return -1;

	return 0;
}

/**
 * librouter_ksz8863_get_dscp_prio
 *
 * Get the packet priority based on CoS value
 *
 * @param cos
 * @return priority value if success, -1 if error
 */
int librouter_ksz8863_get_cos_prio(int cos)
{
	__u8 reg, data;
	int prio;

	if (cos < 0 || cos > 7) {
		printf("%% Invalid DSCP value : %d\n", cos);
		return -1;
	}

	reg = KSZ8863REG_GLOBAL_CONTROL10;
	reg += cos / 4;

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
		return -1;

	prio = (int) ((data >> (cos % 4) * 2) & 0x3);

	return prio;
}

/***********************************************/
/********* VLAN table manipulation *************/
/***********************************************/

/**
 * _get_vlan_table	Fetch VLAN table from switch
 *
 * When successful, data is written to the structure pointed by t
 *
 * @param table
 * @param t
 * @return 0 if success, -1 if failure
 */
static int _get_vlan_table(unsigned int entry, struct vlan_table_t *t)
{
	__u8 data;

	if (entry > 15) {
		printf("%% Invalid VLAN table : %d\n", entry);
		return -1;
	}

	if (t == NULL) {
		printf("%% NULL table pointer\n");
		return -1;
	}

	data = KSZ8863REG_READ_OPERATION | KSZ8863REG_VLAN_TABLE_SELECT;
	if (_ksz8863_reg_write(KSZ8863REG_INDIRECT_ACCESS_CONTROL0, &data, sizeof(data)))
		return -1;

	data = (__u8) entry;
	if (_ksz8863_reg_write(KSZ8863REG_INDIRECT_ACCESS_CONTROL1, &data, sizeof(data)))
		return -1;


	if (_ksz8863_reg_read(KSZ8863REG_INDIRECT_DATA0, &data, sizeof(data)))
		return -1;
	t->vid = data;

	if (_ksz8863_reg_read(KSZ8863REG_INDIRECT_DATA1, &data, sizeof(data)))
		return -1;
	t->vid |= (data & 0x0F) << 8;
	t->fid = (data & 0xF0) >> 4;

	if (_ksz8863_reg_read(KSZ8863REG_INDIRECT_DATA2, &data, sizeof(data)))
		return -1;
	t->membership = data & 0x07;
	t->valid = (data & 0x08) >> 3;

	return 0;
}

/**
 * _set_vlan_table	Configure a VLAN table in the switch
 *
 * Configuration is present in the structure pointed by t
 *
 * @param table
 * @param t
 * @return 0 if success, -1 if failure
 */
static int _set_vlan_table(unsigned int table, struct vlan_table_t *t)
{
	__u8 data;

	if (table > 15) {
		printf("%% Invalid VLAN table : %d\n", table);
		return -1;
	}

	if (t == NULL) {
		printf("%% NULL table pointer\n");
		return -1;
	}

	data = t->vid;
	if (_ksz8863_reg_write(KSZ8863REG_INDIRECT_DATA0, &data, sizeof(data)))
		return -1;

	data = t->vid >> 8;
	data |= t->fid << 4;
	if (_ksz8863_reg_write(KSZ8863REG_INDIRECT_DATA1, &data, sizeof(data)))
		return -1;

	data = t->membership;
	data |= t->valid << 3;
	if (_ksz8863_reg_write(KSZ8863REG_INDIRECT_DATA2, &data, sizeof(data)))
		return -1;

	data = KSZ8863REG_WRITE_OPERATION | KSZ8863REG_VLAN_TABLE_SELECT;
	if (_ksz8863_reg_write(KSZ8863REG_INDIRECT_ACCESS_CONTROL0, &data, sizeof(data)))
		return -1;

	data = (__u8) table;
	if (_ksz8863_reg_write(KSZ8863REG_INDIRECT_ACCESS_CONTROL1, &data, sizeof(data)))
	return -1;

	return 0;
}

/**
 * _init_vlan_table
 *
 * Make all VLAN entries invalid
 *
 * @return 0 if success, -1 if failure
 */
static int _init_vlan_table(void)
{
	int i;
	struct vlan_table_t vlan;

	vlan.valid = 0;
	vlan.vid = 0;

	/* Search for the same VID in a existing entry */
	for (i = 0; i < KSZ8863_NUM_VLAN_TABLES; i++)
		_set_vlan_table(i, &vlan);

	return 0;
}

/**
 * librouter_ksz8863_add_table
 *
 * Create/Modify a VLAN in the switch
 *
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_add_table(struct vlan_config_t *vconfig)
{
	struct vlan_table_t new_t, exist_t;
	int active = 0, i;

	if (vconfig == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	/* Initiate table data */
	new_t.valid = 1;
	new_t.membership = vconfig->membership;
	new_t.vid = vconfig->vid;

	/* Search for the same VID in a existing entry */
	for (i = 0; i < KSZ8863_NUM_VLAN_TABLES; i++) {
		_get_vlan_table(i, &exist_t);

		if (!exist_t.valid)
			continue;

		if (new_t.vid == exist_t.vid) {
			/* Found the same VID, just update this entry */
			new_t.fid = i;
			return _set_vlan_table(i, &new_t);
		}

		active++;
	}

	/* No more free entries ? */
	if (active == KSZ8863_NUM_VLAN_TABLES) {
		printf("%% Already reached maximum number of entries\n");
		return -1;
	}

	/* The same VID was not found, just add in the first entry available */
	for (i = 0; i < KSZ8863_NUM_VLAN_TABLES; i++) {
		if (_get_vlan_table(i, &exist_t) < 0)
			return -1;

		if (exist_t.valid)
			continue;

		new_t.fid = i;
		return _set_vlan_table(i, &new_t);
	}

	/* Should not be reached */
	return -1;
}

/**
 * librouter_ksz8863_del_table
 *
 * Delete a table entry for a certain VID
 *
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_del_table(struct vlan_config_t *vconfig)
{
	struct vlan_table_t t;
	int i, vid;

	if (vconfig == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	vid = vconfig->vid;

	/* Search for an existing entry */
	for (i = 0; i < KSZ8863_NUM_VLAN_TABLES; i++) {
		if (_get_vlan_table(i, &t) < 0)
			return -1;

		if (!t.valid)
			continue;

		if (t.vid == vid) {
			t.valid = 0; /* Make this entry invalid */
			return _set_vlan_table(i, &t);
		}
	}

	return 0;
}

/**
 * librouter_ksz8863_get_table
 *
 * Get a table entry and fill the data in vconfig
 *
 * @param entry
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_get_table(int entry, struct vlan_table_t *v)
{
	if (v == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	if (_get_vlan_table(entry, v) < 0)
		return -1;

	ksz8863_dbg("Getting entry %d\n", entry);
	ksz8863_dbg(" -- > valid : %d\n", v->valid);
	ksz8863_dbg(" -- > membership : %d\n", v->membership);
	ksz8863_dbg(" -- > vid : %d\n", v->vid);
	ksz8863_dbg(" -- > fid : %d\n", v->fid);

	return 0;
}

/*********************************************/
/******* Initialization functions ************/
/*********************************************/

/**
 * librouter_ksz8863_probe	Probe for the KSZ8863 Chip
 *
 * Read ID registers to determine if chip is present
 *
 * @return 1 if chip was detected, 0 if not, -1 if error
 */
int librouter_ksz8863_probe(void)
{
	__u8 reg = 0x0;
	__u8 id_u, id_l;
	__u16 id;

	if (_ksz8863_reg_read(reg, &id_u, sizeof(id_u)))
		return -1;

	if (_ksz8863_reg_read(reg + 1, &id_l, sizeof(id_l)))
		return -1;

	id_l &= 0xf0; /* Take only bits 7-4 for the lower part */
	id = (id_u << 8) | id_l;

	if (id == KSZ8863_ID)
		return 1;

	return 0;
}

/**
 * librouter_ksz8863_set_default_config
 *
 * Configure switch to system default
 *
 * @return 0 if success, -1 if error
 */
int librouter_ksz8863_set_default_config(void)
{
	if (librouter_ksz8863_probe())
		_init_vlan_table();

	return 0;
}


static void _dump_port_config(FILE *out, int port)
{
	int i;
	int port_alias = librouter_ksz8863_get_aliasport_by_realport(port);

	fprintf(out, " switch-port %d\n", port_alias);

	if (librouter_ksz8863_get_8021p(port))
		fprintf(out, "  802.1p\n");

	if (librouter_ksz8863_get_diffserv(port))
		fprintf(out, "  diffserv\n");

	for (i = 0; i < 4; i++) {
		int rate = librouter_ksz8863_get_ingress_rate_limit(port, i);
		if (rate != 100000)
			fprintf(out, "  rate-limit %d %d\n", i, rate);
	}

	for (i = 0; i < 4; i++) {
		int rate = librouter_ksz8863_get_egress_rate_limit(port, i);
		if (rate != 100000)
			fprintf(out, "  traffic-shape %d %d\n", i, rate);
	}

	if (librouter_ksz8863_get_txqsplit(port))
		fprintf(out, "  txqueue-split\n");

	i = librouter_ksz8863_get_default_vid(port);
	if (i)
		fprintf(out, "  vlan-default %d\n", i);

}

int librouter_ksz8863_dump_config(FILE *out)
{
	int i;

	/* Is device present ? */
	if (librouter_ksz8863_probe() == 0)
		return 0;

	if (librouter_ksz8863_get_8021q())
		fprintf(out, " switch-config 802.1q\n");

	i = librouter_ksz8863_get_storm_protect_rate();
	if (i != 1)
		fprintf(out, " switch-config storm-protect-rate %d\n", i);

	for (i = 0; i < 8; i++) {
		int prio = librouter_ksz8863_get_cos_prio(i);
		if (prio != i / 2)
			fprintf(out, " switch-config cos-prio %d %d\n", i, prio);
	}

	for (i = 0; i < 64; i++) {
		int prio = librouter_ksz8863_get_dscp_prio(i);
		if (prio)
			fprintf(out, " switch-config dscp-prio %d %d\n", i, prio);
	}

	if (librouter_ksz8863_get_multicast_storm_protect())
		fprintf(out, " switch-config multicast-storm-protect\n");

	if (librouter_ksz8863_get_replace_null_vid())
		fprintf(out, " switch-config replace-null-vid\n");

	if (librouter_ksz8863_get_wfq())
		fprintf(out, " switch-config wfq\n");

	for (i = 0; i < KSZ8863_NUM_VLAN_TABLES; i++) {
		struct vlan_table_t v;

		librouter_ksz8863_get_table(i, &v);
		if (v.valid)
			fprintf(out, " switch-config vlan %d %s%s%s\n",
				v.vid, (v.membership & KSZ8863REG_VLAN_MEMBERSHIP_PORT1_MSK) ? "port-1 " : "",
				(v.membership & KSZ8863REG_VLAN_MEMBERSHIP_PORT2_MSK) ? "port-2 " : "",
				(v.membership & KSZ8863REG_VLAN_MEMBERSHIP_PORT3_MSK) ? "internal" : "");
	}

	for (i = 0; i < 2; i++)
		_dump_port_config(out, i);
}

#endif /* OPTION_MANAGED_SWITCH */
