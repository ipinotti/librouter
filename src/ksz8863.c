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
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "options.h"

//#include "i2c-dev.h"
#include "ksz8863.h"

/* Low level I2C functions */
static int _ksz8863_reg_read(__u8 reg, __u8 *buf, __u8 len)
{
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg msg;
	int dev;

	dev = open(KSZ8863_I2CDEV, O_NONBLOCK);
	if (dev < 0) {
		ksz8863_dbg("error opening %s: %s\n", KSZ8863_I2CDEV, strerror(errno));
		return -1;
	}

	data.msgs = &msg;
	data.nmsgs = 1;

	msg.addr = KSZ8863_I2CADDR;
	msg.buf = buf;
	msg.len = len;
	msg.flags = I2C_M_RD;
	msg.command = reg; /* address to read from */

	if (ioctl(dev, I2C_RDWR, &data) < 0) {
		ksz8863_dbg("Could not read from device : %s\n", strerror(errno));
		close(dev);
		return -1;
	}

	close(dev);
	return 0;
}

static int _ksz8863_reg_write(__u8 reg, __u8 *buf, __u8 len)
{
	struct i2c_rdwr_ioctl_data data;
	struct i2c_msg msg;
	int dev;

	dev = open(KSZ8863_I2CDEV, O_NONBLOCK);
	if (dev < 0) {
		printf("error opening %s: %s\n", KSZ8863_I2CDEV, strerror(errno));
		return -1;
	}

	data.msgs = &msg;
	data.nmsgs = 1;

	msg.addr = KSZ8863_I2CADDR;
	msg.buf = buf;
	msg.len = len;
	msg.flags = 0;
	msg.command = reg; /* address to write to */

	if (ioctl(dev, I2C_RDWR, &data) < 0) {
		printf("%% %s : Could not write to device\n", __FUNCTION__);
		close(dev);
		return -1;
	}

	close(dev);
	return 0;
}

/******************************************************/
/********** Exported functions ************************/
/******************************************************/
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

	return (data | KSZ8863REG_ENABLE_BC_STORM_PROTECT_MSK) ? 1 : 0;
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

	return ((data | KSZ8863REG_ENABLE_MC_STORM_PROTECT_MSK) ? 1 : 0);
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
	reg = KSZ8863REG_INGRESS_RATE_LIMIT;
	reg += 0x4 * port;
	reg += prio;

	if (!rate) {
		data = (__u8) 0;
	} else if (rate >= 1000) {
		data = (__u8) rate/1000;
	} else {
		data = 0x65;
		data += (__u8) (rate/64 - 1);
	}

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

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
	reg = KSZ8863REG_INGRESS_RATE_LIMIT;
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

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

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
	reg = KSZ8863REG_EGRESS_RATE_LIMIT;
	reg += 0x10 * port;
	reg += prio;

	if (!rate) {
		data = (__u8) 0;
	} else if (rate >= 1000) {
		data = (__u8) rate/1000;
	} else {
		data = 0x65;
		data += (__u8) (rate/64 - 1);
	}

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

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
	reg = KSZ8863REG_EGRESS_RATE_LIMIT;
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

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

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

	ksz8863_dbg("addr = %02x data = %02x\n", KSZ8863REG_GLOBAL_CONTROL3, data)

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

	ksz8863_dbg("addr = %02x data = %02x\n", KSZ8863REG_GLOBAL_CONTROL3, data)

	return ((data | KSZ8863_ENABLE_8021Q_MSK) ? 1 : 0);
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

	ksz8863_dbg("addr = %02x data = %02x\n", KSZ8863REG_GLOBAL_CONTROL3, data)

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

	ksz8863_dbg("addr = %02x data = %02x\n", KSZ8863REG_GLOBAL_CONTROL3, data)

	return ((data | KSZ8863_ENABLE_WFQ_MSK) ? 1 : 0);
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

	vid_l = (__u8) (vid | 0xff);
	vid_u = (__u8) ((vid >> 8) | KSZ8863REG_DEFAULT_VID_UPPER_MSK);

	if (_ksz8863_reg_read(reg, &data, sizeof(data)))
	return -1;

	/* Set upper part of VID */
	vid_u |= (data & (~KSZ8863REG_DEFAULT_VID_UPPER_MSK));

	ksz8863_dbg("addr = %02x data = %02x\n", KSZ8863REG_GLOBAL_CONTROL3, data)

	if (_ksz8863_reg_write(reg, &vid_u, sizeof(vid_u)) < 0)
		return -1;

	ksz8863_dbg("addr = %02x data = %02x\n", KSZ8863REG_GLOBAL_CONTROL3, data)

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

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

	vid_u = data & KSZ8863REG_DEFAULT_VID_UPPER_MSK;

	if (_ksz8863_reg_read(++reg, &data, sizeof(data)))
		return -1;

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

	vid_l = data;

	return ((int) (vid_u << 8 | vid_l));
}

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


	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

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

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

	return (data | KSZ8863REG_ENABLE_8021P_MSK) ? 1 : 0;
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

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

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

	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

	return (data | KSZ8863REG_ENABLE_DIFFSERV_MSK) ? 1 : 0;
}

/******************/
/* For tests only */
/******************/
int librouter_ksz8863_read(__u8 reg)
{
	__u8 data;

	_ksz8863_reg_read(reg, &data, sizeof(data));
	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)

	return data;
}

int librouter_ksz8863_write(__u8 reg, __u8 data)
{
	ksz8863_dbg("addr = %02x data = %02x\n", reg, data)
	return _ksz8863_reg_write(reg, &data, sizeof(data));
}
