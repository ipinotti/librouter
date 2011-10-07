/*
 * ksz8895.c
 *
 *  Created on: Oct 4, 2011
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 *
 *      Strongly based on ksz8863.c
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
#include <sys/socket.h>

#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/mii.h>

#include "options.h"

#ifdef OPTION_MANAGED_SWITCH

#include "data.h"
#include "ksz8895.h"

/* From gianfar.h */
#define MICREL_SMI_GETREG		(SIOCDEVPRIVATE + 14)
#define MICREL_SMI_SETREG		(SIOCDEVPRIVATE + 15)

/* type, port[NUMBER_OF_SWITCH_PORTS] */
static port_family_switch _switch_ksz_ports[] = {
		{ real_sw, { 0, 1, 2, 3 } },
		{ alias_sw, { 1, 2, 3, 4 } },
                { non_sw, { 0, 0, 0, 0 } }
};

/* Low level I2C functions */
static int _ksz8895_reg_read(__u8 reg, __u8 *buf)
{
	int fd;
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (void *) &ifr.ifr_data;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Could not open socket\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");

	mii->reg_num = (__u16) reg;

	if (ioctl(fd, MICREL_SMI_GETREG, &ifr) < 0) {
		close(fd);
		perror("MICREL_SMI_GETREG\n");
		return -1;
	}

	*buf = (__u8) mii->val_out;

	close(fd);

	return 0;
}

static int _ksz8895_reg_write(__u8 reg, __u8 *buf)
{
	int fd;
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (void *) &ifr.ifr_data;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Could not open socket\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");

	mii->reg_num = (__u16) reg;
	mii->val_in = (__u16) *buf;

	if (ioctl(fd, MICREL_SMI_SETREG, &ifr) < 0) {
		close(fd);
		perror("MICREL_SMI_SETREG\n");
		return -1;
	}

	close(fd);
	return 0;
}

	/******************/
	/* For tests only */
	/******************/
int librouter_ksz8895_read(__u8 reg)
{
	__u8 data;

	_ksz8895_reg_read(reg, &data);

	return data;
}

int librouter_ksz8895_write(__u8 reg, __u8 data)
{
	return _ksz8895_reg_write(reg, &data);
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
int librouter_ksz8895_get_aliasport_by_realport(int switch_port)
{
	int i;

	for (i = 0; i < NUMBER_OF_SWITCH_PORTS; i++) {
		if (_switch_ksz_ports[real_sw].port[i] == switch_port) {
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
int librouter_ksz8895_get_realport_by_aliasport(int switch_port)
{
	int i;

	for (i = 0; i < NUMBER_OF_SWITCH_PORTS; i++) {
		if (_switch_ksz_ports[alias_sw].port[i] == switch_port) {
			return _switch_ksz_ports[real_sw].port[i];
		}
	}
	return -1;
}

/**
 * librouter_ksz8895_set_broadcast_storm_protect
 *
 * Enable or Disable Broadcast Storm protection
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8895_set_broadcast_storm_protect(int enable, int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	if (enable)
		data |= KSZ8895REG_ENABLE_BC_STORM_PROTECT_MSK;
	else
		data &= ~KSZ8895REG_ENABLE_BC_STORM_PROTECT_MSK;

	if (_ksz8895_reg_write(reg, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_broadcast_storm_protect
 *
 * Get if Broadcast Storm protection is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_broadcast_storm_protect(int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	return (data & KSZ8895REG_ENABLE_BC_STORM_PROTECT_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8895_set_storm_protect_rate
 *
 * Set maximum incoming rate for broadcast packets
 *
 * @param percentage: 0 to 100
 * @return 0 if success, -1 if failure
 */
int librouter_ksz8895_set_storm_protect_rate(unsigned int percentage)
{
	__u8 reg, data;
	__u16 tmp;

	if (percentage > 100) {
		printf("%% Invalid percentage value : %d\n", percentage);
		return -1;
	}

	/* 99 packets/interval corresponds to 1% - See Page 35 */
	tmp = percentage * 74;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL4, &data))
		return -1;

	data &= ~0x07; /* Clear old rate value - first 3 bits */
	data |= (tmp >> 8); /* Get the 3 most significant bits */

	if (_ksz8895_reg_write(KSZ8895REG_GLOBAL_CONTROL4, &data))
		return -1;

	/* Now write the less significant bits */
	data = (__u8) (tmp & 0x00ff);
	if (_ksz8895_reg_write(KSZ8895REG_GLOBAL_CONTROL5, &data))
	return -1;

	return 0;
}

	/**
	 * librouter_ksz8895_get_storm_protect_rate
	 *
	 * Get maximum incoming rate for broadcast packets
	 *
	 * @return -1 if error, rate percentage otherwise
	 */
int librouter_ksz8895_get_storm_protect_rate(void)
{
	__u8 rate_u, rate_l;
	int perc;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL4, &rate_u))
		return -1;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL5, &rate_l))
		return -1;

	rate_u &= 0x07; /* Get rate bits [2-0] */

	ksz8895_dbg("Storm Protect register value is %04x\n", ((rate_u << 8) | rate_l));

	perc = ((rate_u << 8) | rate_l);
	perc /= 74;

	return perc;
}

/**
 * librouter_ksz8895_set_multicast_storm_protect
 *
 * Enable or Disable Multicast Storm protection
 *
 * @param enable
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8895_set_multicast_storm_protect(int enable)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL2, &data))
		return -1;

	/* Careful here, the logic is inverse */
	if (enable)
		data &= ~KSZ8895REG_ENABLE_MC_STORM_PROTECT_MSK;
	else
		data |= KSZ8895REG_ENABLE_MC_STORM_PROTECT_MSK;

	if (_ksz8895_reg_write(KSZ8895REG_GLOBAL_CONTROL2, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_multicast_storm_protect
 *
 * Get if Multicast Storm protection is enabled or disabled
 *
 * @param void
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_multicast_storm_protect(void)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL2, &data))
		return -1;

	ksz8895_dbg("Multicast Storm Protect bit value is %d\n",
	            (data & KSZ8895REG_ENABLE_MC_STORM_PROTECT_MSK) ? 1 : 0);

	/* Careful here, the logic is inverse */
	return ((data & KSZ8895REG_ENABLE_MC_STORM_PROTECT_MSK) ? 0 : 1);
}

/**
 * librouter_ksz8895_set_replace_null_vid
 *
 * Enable or Disable NULL VID replacement with port VID
 *
 * @param enable
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8895_set_replace_null_vid(int enable)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL4, &data))
		return -1;

	if (enable)
		data |= KSZ8895_ENABLE_REPLACE_NULL_VID_MSK;
	else
		data &= ~KSZ8895_ENABLE_REPLACE_NULL_VID_MSK;

	if (_ksz8895_reg_write(KSZ8895REG_GLOBAL_CONTROL4, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_replace_null_vid
 *
 * Get NULL VID replacement configuration
 *
 * @param void
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_replace_null_vid(void)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL4, &data))
		return -1;

	ksz8895_dbg("Replace NULL VID bit value is %d\n",
			(data & KSZ8895_ENABLE_REPLACE_NULL_VID_MSK) ? 1 : 0);

	return ((data & KSZ8895_ENABLE_REPLACE_NULL_VID_MSK) ? 1 : 0);
}

/**
 * librouter_ksz8895_set_egress_rate_limit
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
int librouter_ksz8895_set_egress_rate_limit(int port, int prio, int rate)
{
	__u8 reg, data;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8895REG_EGRESS_RATE_LIMIT;
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

	return _ksz8895_reg_write(reg, &data);
}

		/**
		 * librouter_ksz8895_get_egress_rate_limit
		 *
		 * Get rate limit from a certain port and priority queue
		 *
		 * @param port
		 * @param prio
		 * @return The configured rate limit or -1 if error
		 */
int librouter_ksz8895_get_egress_rate_limit(int port, int prio)
{
	__u8 reg, data;
	int rate;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8895REG_EGRESS_RATE_LIMIT;
	reg += 0x4 * port;
	reg += prio;

	if (_ksz8895_reg_read(reg, &data) < 0)
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
 * librouter_ksz8895_set_ingress_rate_limit
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
int librouter_ksz8895_set_ingress_rate_limit(int port, int prio, int rate)
{
	__u8 reg, data;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8895REG_INGRESS_RATE_LIMIT;
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

	return _ksz8895_reg_write(reg, &data);
}

/**
 * librouter_ksz8895_get_ingress_rate_limit
 *
 * Get ingress rate limit from a certain port and priority queue
 *
 * @param port
 * @param prio
 * @return The configured rate limit or -1 if error
 */
int librouter_ksz8895_get_ingress_rate_limit(int port, int prio)
{
	__u8 reg, data;
	int rate;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (prio > 3) {
		printf("%% No such priority : %d\n", prio);
		return -1;
	}

	/* Calculate reg offset */
	reg = KSZ8895REG_INGRESS_RATE_LIMIT;
	reg += 0x10 * port;
	reg += prio;

	if (_ksz8895_reg_read(reg, &data) < 0)
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
 * librouter_ksz8895_set_8021q	Enable/disable 802.1q (VLAN)
 *
 * @param enable
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_set_8021q(int enable)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL3, &data))
		return -1;

	if (enable)
		data |= KSZ8895_ENABLE_8021Q_MSK;
	else
		data &= ~KSZ8895_ENABLE_8021Q_MSK;

	if (_ksz8895_reg_write(KSZ8895REG_GLOBAL_CONTROL3, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_8021q	Get if 802.1q is enabled or disabled
 *
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_8021q(void)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL3, &data))
		return -1;

	return ((data & KSZ8895_ENABLE_8021Q_MSK) ? 1 : 0);
}

/**
 * librouter_ksz8895_set_wfq	Enable/disable Weighted Fair Queueing
 *
 * @param enable
 * @return 0 if success, -1 otherwise
 */
int librouter_ksz8895_set_wfq(int enable)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL3, &data))
		return -1;

	if (enable)
		data |= KSZ8895_ENABLE_WFQ_MSK;
	else
		data &= ~KSZ8895_ENABLE_WFQ_MSK;

	if (_ksz8895_reg_write(KSZ8895REG_GLOBAL_CONTROL3, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_wfq	Get if WFQ is enabled or disabled
 *
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_wfq(void)
{
	__u8 data;

	if (_ksz8895_reg_read(KSZ8895REG_GLOBAL_CONTROL3, &data))
		return -1;

	return ((data & KSZ8895_ENABLE_WFQ_MSK) ? 1 : 0);
}

/**
 * librouter_ksz8895_set_default_vid	Set port default 802.1q VID
 *
 * @param port
 * @param vid
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_set_default_vid(int port, int vid)
{
	__u8 vid_l, vid_u;
	__u8 reg, data;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (vid > 4095) {
		printf("%% Invalid 802.1q VID : %d\n", vid);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL3;
	reg += 0x10 * port;

	vid_l = (__u8) (vid & 0xff);
	vid_u = (__u8) ((vid >> 8) & KSZ8895REG_DEFAULT_VID_UPPER_MSK);

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	/* Set upper part of VID */
	vid_u |= (data & (~KSZ8895REG_DEFAULT_VID_UPPER_MSK));

	if (_ksz8895_reg_write(reg, &vid_u) < 0)
		return -1;

	if (_ksz8895_reg_write(reg+1, &vid_l) < 0)
		return -1;

	return 0;
}

	/**
	 * librouter_ksz8895_get_default_vid	Get port's default 802.1q VID
	 *
	 * @param port
	 * @return Default VID if success, -1 if error
	 */
int librouter_ksz8895_get_default_vid(int port)
{
	__u8 vid_l, vid_u;
	__u8 reg, data;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL3;
	reg += 0x10 * port;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	vid_u = data & KSZ8895REG_DEFAULT_VID_UPPER_MSK;

	if (_ksz8895_reg_read(++reg, &data))
		return -1;

	vid_l = data;

	return ((int) (vid_u << 8 | vid_l));
}

/**
 * librouter_ksz8895_set_default_cos	Set port default 802.1p CoS
 *
 * @param port
 * @param cos
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_set_default_cos(int port, int cos)
{
	__u8 reg, data;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	if (cos > 7) {
		printf("%% Invalid 802.1p CoS : %d\n", cos);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL3;
	reg += 0x10 * port;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	data &= ~KSZ8895REG_DEFAULT_COS_MSK;
	data |= (cos << 5) & KSZ8895REG_DEFAULT_COS_MSK; /* Set bits [7-5] */

	if (_ksz8895_reg_write(reg, &data) < 0)
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_default_cos	Get port's default 802.1q CoS
 *
 * @param port
 * @return Default CoS if success, -1 if error
 */
int librouter_ksz8895_get_default_cos(int port)
{
	__u8 reg, data;

	if (port < 0 || port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL3;
	reg += 0x10 * port;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	return ((int) (data >> 5));
}

/**
 * librouter_ksz8895_set_8021p
 *
 * Enable or Disable 802.1p classification
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8895_set_8021p(int enable, int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	if (enable)
		data |= KSZ8895REG_ENABLE_8021P_MSK;
	else
		data &= ~KSZ8895REG_ENABLE_8021P_MSK;

	if (_ksz8895_reg_write(reg, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_8021p
 *
 * Get if 802.1p classification is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_8021p(int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	return (data & KSZ8895REG_ENABLE_8021P_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8895_set_diffserv
 *
 * Enable or Disable Diff Service classification
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8895_set_diffserv(int enable, int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	if (enable)
		data |= KSZ8895REG_ENABLE_DIFFSERV_MSK;
	else
		data &= ~KSZ8895REG_ENABLE_DIFFSERV_MSK;

	if (_ksz8895_reg_write(reg, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_diffserv
 *
 * Get if Diff Service classification is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_diffserv(int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	return (data & KSZ8895REG_ENABLE_DIFFSERV_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8895_set_taginsert
 *
 * Enable or Disable 802.1p/q tag insertion on untagged packets
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8895_set_taginsert(int enable, int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	if (enable)
		data |= KSZ8895REG_ENABLE_TAGINSERT_MSK;
	else
		data &= ~KSZ8895REG_ENABLE_TAGINSERT_MSK;

	if (_ksz8895_reg_write(reg, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_taginsert
 *
 * Get if 802.1p/q tag insertion on untagged packets is enabled or disabled
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_taginsert(int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	return (data & KSZ8895REG_ENABLE_TAGINSERT_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8895_set_txqsplit
 *
 * Enable or Disable TXQ Split into 4 queues
 *
 * @param enable
 * @param port:	from 0 to 2
 * @return 0 on success, -1 otherwise
 */
int librouter_ksz8895_set_txqsplit(int enable, int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	if (enable)
		data |= KSZ8895REG_ENABLE_TXQSPLIT_MSK;
	else
		data &= ~KSZ8895REG_ENABLE_TXQSPLIT_MSK;

	if (_ksz8895_reg_write(reg, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_txqsplit
 *
 * Get if TXQ split configuration
 *
 * @param port: from 0 to 2
 * @return 1 if enabled, 0 if disabled, -1 if error
 */
int librouter_ksz8895_get_txqsplit(int port)
{
	__u8 reg, data;

	if (port > NUMBER_OF_SWITCH_PORTS) {
		printf("%% No such port : %d\n", port);
		return -1;
	}

	reg = KSZ8895REG_PORT_CONTROL;
	reg += port * 0x10;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	return (data & KSZ8895REG_ENABLE_TXQSPLIT_MSK) ? 1 : 0;
}

/**
 * librouter_ksz8895_set_dscp_prio
 *
 * Set the packet priority based on DSCP value
 *
 * @param dscp
 * @param prio
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_set_dscp_prio(int dscp, int prio)
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

	reg = KSZ8895REG_TOS_PRIORITY_CONTROL;
	reg += dscp / 4;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	/*
	 * See KSZ8895 Datasheet Page 65  for the register description
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

	if (_ksz8895_reg_write(reg, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_dscp_prio
 *
 * Get the packet priority based on DSCP value
 *
 * @param dscp
 * @return priority value if success, -1 if error
 */
int librouter_ksz8895_get_dscp_prio(int dscp)
{
	__u8 reg, data;
	int prio;

	if (dscp < 0 || dscp > 63) {
		printf("%% Invalid DSCP value : %d\n", dscp);
		return -1;
	}

	reg = KSZ8895REG_TOS_PRIORITY_CONTROL;
	reg += dscp / 4;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	prio = (int) ((data >> (dscp % 4) * 2) & 0x3);

	return prio;
}

/**
 * librouter_ksz8895_set_cos_prio
 *
 * Set the packet priority based on DSCP value
 *
 * @param cos
 * @param prio
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_set_cos_prio(int cos, int prio)
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

	reg = KSZ8895REG_GLOBAL_CONTROL12;
	reg += cos / 4;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	/*
	 * See KSZ8895 Datasheet Page 53  for the register description
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

	if (_ksz8895_reg_write(reg, &data))
		return -1;

	return 0;
}

/**
 * librouter_ksz8895_get_dscp_prio
 *
 * Get the packet priority based on CoS value
 *
 * @param cos
 * @return priority value if success, -1 if error
 */
int librouter_ksz8895_get_cos_prio(int cos)
{
	__u8 reg, data;
	int prio;

	if (cos < 0 || cos > 7) {
		printf("%% Invalid DSCP value : %d\n", cos);
		return -1;
	}

	reg = KSZ8895REG_GLOBAL_CONTROL12;
	reg += cos / 4;

	if (_ksz8895_reg_read(reg, &data))
		return -1;

	ksz8895_dbg("802.1p priority mapping register %02x has value %02x\n", reg, data);

	prio = (int) ((data >> (cos % 4) * 2) & 0x3);

	return prio;
}

/***********************************************/
/********* VLAN table manipulation *************/
/***********************************************/

/**
 * _trigger_vlan_table_read	Trigger a VLAN table read on KSZ8895 Switch
 *
 * See page 91 of datasheet for a better description
 *
 * @param vid	VID to be fetched
 * @return 0 if success, -1 if failure
 */
static int _trigger_vlan_table_read(unsigned int vid, __u64 *data)
{
	__u8 data8;
	int addr = vid/4;
	int i;

	data8 = KSZ8895REG_READ_OPERATION | KSZ8895REG_VLAN_TABLE_SELECT;
	data8 |= (addr >> 8) & 0x3; /* This two bits are the MSB for the VID */

	if (_ksz8895_reg_write(KSZ8895REG_INDIRECT_ACCESS_CONTROL0,  &data8) < 0)
		return -1;

	data8 = (__u8) addr;

	if (_ksz8895_reg_write(KSZ8895REG_INDIRECT_ACCESS_CONTROL1, &data8) < 0)
		return -1;

	for (i = 0, *data = 0; i < 8; i++) {
		if (_ksz8895_reg_read(KSZ8895REG_INDIRECT_DATA0 - i, &data8) < 0)
			return -1;
		*data |= data8 << (i * 8);
	}

	return 0;
}

/**
 * _trigger_vlan_table_write	Trigger a VLAN table write on KSZ8895 Switch
 *
 * See page 91 of datasheet for a better description
 *
 * @param vid	VID to be written
 * @return 0 if success, -1 if failure
 */
static int _trigger_vlan_table_write(unsigned int vid, __u64 data)
{
	__u8 data8;
	int addr = vid/4;
	int i;

	for (i = 0; i < 8; i++) {
		data8 = (__u8) (data >> (i * 8));
		ksz8895_dbg("Writing %02x in reg %02x\n", data8, KSZ8895REG_INDIRECT_DATA0 - i);
		if (_ksz8895_reg_write(KSZ8895REG_INDIRECT_DATA0 - i, &data8) < 0)
			return -1;
	}

	ksz8895_dbg("1\n");
	data8 = KSZ8895REG_WRITE_OPERATION | KSZ8895REG_VLAN_TABLE_SELECT;
	data8 |= (addr >> 8) & 0x3; /* This two bits are the MSB for the VID */
	if (_ksz8895_reg_write(KSZ8895REG_INDIRECT_ACCESS_CONTROL0, &data8) < 0)
		return -1;


	ksz8895_dbg("2\n");
	data8 = (__u8) addr;
	if (_ksz8895_reg_write(KSZ8895REG_INDIRECT_ACCESS_CONTROL1, &data8) < 0)
		return -1;

	ksz8895_dbg("3\n");
	return 0;
}

/**
 * _get_vlan_table	Fetch VLAN table from switch
 *
 * When successful, data is written to the structure pointed by t
 *
 * @param table
 * @param t
 * @return 0 if success, -1 if failure
 */
static int _get_vlan_table(int vid, struct vlan_table_t *t)
{
	__u8 data8;
	__u32 data;
	__u64 acc_data;
	int addr = vid/4;
	int mask = 0x1fff;
	int offset = (vid % 4) * 13;

	ksz8895_dbg("Calculated register offset for VID %d: %d bits\n", vid, offset);

	if (t == NULL) {
		printf("%% NULL table pointer\n");
		return -1;
	}

	memset(t, 0, sizeof(struct vlan_table_t));

	if (_trigger_vlan_table_read(vid, &acc_data) < 0) {
		printf("%% Failed to trigger VLAN table read operation\n");
		return -1;
	}

	data = (__u32) ((acc_data >> offset) & mask);

	ksz8895_dbg("Read table for VID %d : reg value is %08x\n", vid, data);

	/* Finally, data is in the format expected by struct vlan_table_t */
	memcpy(t, &data, sizeof(struct vlan_table_t));

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
static int _set_vlan_table(int vid, struct vlan_table_t *t)
{
	__u32 data;
	__u64 acc_data;

	int addr = vid/4;
	int mask = 0x1fff;
	int offset = (vid % 4) * 13;

	ksz8895_dbg("Calculated register offset for VID %d: %d bits\n", vid, offset);

	if (t == NULL) {
		printf("%% NULL table pointer\n");
		return -1;
	}

	/* Trigger a read operation to load the registers */
	if (_trigger_vlan_table_read(vid, &acc_data) < 0) {
		printf("%% Failed to trigger VLAN table read operation\n");
		return -1;
	}

	ksz8895_dbg("Read table for VID %d : data is %16x\n", vid, acc_data);

	/* Now the registers can be changed */
	acc_data &= ~((__u64) (mask << offset)); /* Clear entry */
	data = (*(__u32 *)t) & mask;
	acc_data |= (__u64)(data << offset);

	ksz8895_dbg(" -- > valid : %d\n", t->valid);
	ksz8895_dbg(" -- > membership : %d\n", t->membership);
	ksz8895_dbg(" -- > fid : %d\n", t->fid);
	ksz8895_dbg(" -- > Serialized table : %08x\n", data);
	ksz8895_dbg("Table change for VID %d : data is %llX\n", vid, acc_data);

	/* Finally, trigger the write operation */
	if (_trigger_vlan_table_write(vid, acc_data) < 0) {
		printf("%% Failed to trigger VLAN table write operation\n");
		return -1;
	}

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
	vlan.membership = 0x1f;
	vlan.fid = 0;

	/* Search for the same VID in a existing entry */
	for (i = 0; i < KSZ8895_NUM_VLAN_TABLES; i++)
		_set_vlan_table(i, &vlan);

	return 0;
}

/**
 * librouter_ksz8895_add_table
 *
 * Create/Modify a VLAN in the switch
 *
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_add_table(struct vlan_config_t *vconfig)
{
	struct vlan_table_t new, exist;
	int active = 0, i;

	if (vconfig == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	/* Get number of existing tables */
	if (librouter_data_load(KSZ8895_NUM_VLAN_OBJ, (char *)&i, sizeof(i)) < 0)
		i = 0;

	/* If no such VLAN existed before and if we already have
	 * the maximum number of VLAN tables, just quit */
	if (i == KSZ8895_MAX_VLAN_ENTRIES) {
		printf("%% VLAN table is full, cannot add more\n");
		return -1;
	}

	/* Initiate table data */
	new.valid = 1;
	new.membership = vconfig->membership;
	new.fid = ++i;

	/* Save number of active VLANs */
	if (librouter_data_save(KSZ8895_NUM_VLAN_OBJ, (char *)&i, sizeof(i)))
		return -1;

	return _set_vlan_table(vconfig->vid, &new);
}

/**
 * librouter_ksz8895_del_table
 *
 * Delete a table entry for a certain VID
 *
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_del_table(struct vlan_config_t *vconfig)
{
	struct vlan_table_t t;
	int vid, i;

	if (vconfig == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	vid = vconfig->vid;

	if (_get_vlan_table(vid, &t) < 0)
		return -1;

	/* If such VID was already not valid, just exit */
	if (!t.valid)
		return 0;

	/* Get number of existing tables */
	if (librouter_data_load(KSZ8895_NUM_VLAN_OBJ, (char *)&i, sizeof(i)) < 0)
		return 0;

	i--;

	/* Save number of active VLANs */
	if (librouter_data_save(KSZ8895_NUM_VLAN_OBJ, (char *)&i, sizeof(i)))
		return -1;


	t.valid = 0; /* Make this entry invalid */
	return _set_vlan_table(vconfig->vid, &t);
}

/**
 * librouter_ksz8895_get_table
 *
 * Get a table entry and fill the data in vconfig
 *
 * @param entry
 * @param vconfig
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_get_table(int entry, struct vlan_table_t *v)
{
	if (v == NULL) {
		printf("%% Invalid VLAN config\n");
		return -1;
	}

	if (_get_vlan_table(entry, v) < 0)
		return -1;

	if (v->valid) {
		ksz8895_dbg("Got valid entry %d!\n", entry);
		ksz8895_dbg(" -- > valid : %d\n", v->valid);
		ksz8895_dbg(" -- > membership : %d\n", v->membership);
		ksz8895_dbg(" -- > vid : %d\n", entry);
		ksz8895_dbg(" -- > fid : %d\n", v->fid);
	}

	return 0;
}

/*********************************************/
/******* Initialization functions ************/
/*********************************************/

/**
 * librouter_ksz8895_probe	Probe for the KSZ8895 Chip
 *
 * Read ID registers to determine if chip is present
 *
 * @return 1 if chip was detected, 0 if not, -1 if error
 */
int librouter_ksz8895_probe(void)
{
	__u8 reg = 0x0;
	__u8 id_u, id_l;
	__u16 id;

	if (_ksz8895_reg_read(reg, &id_u))
		return -1;

	if (_ksz8895_reg_read(reg + 1, &id_l))
		return -1;

	id_l &= 0xf0; /* Take only bits 7-4 for the lower part */
	id = (id_u << 8) | id_l;

	if (id == KSZ8895_ID)
		return 1;

	return 0;
}

/**
 * librouter_ksz8895_set_default_config
 *
 * Configure switch to system default
 *
 * @return 0 if success, -1 if error
 */
int librouter_ksz8895_set_default_config(void)
{
	if (librouter_ksz8895_probe())
		_init_vlan_table();

	return 0;
}

static void _dump_port_config(FILE *out, int port)
{
	int i;
	int port_alias = librouter_ksz8895_get_aliasport_by_realport(port);

	fprintf(out, " switch-port %d\n", port_alias);

	if (librouter_ksz8895_get_8021p(port))
		fprintf(out, "  802.1p\n");

	if (librouter_ksz8895_get_diffserv(port))
		fprintf(out, "  diffserv\n");

	for (i = 0; i < NUMBER_OF_SWITCH_PORTS; i++) {
		int rate = librouter_ksz8895_get_ingress_rate_limit(port, i);
		if (rate != 100000)
			fprintf(out, "  rate-limit %d %d\n", i, rate);
	}

	for (i = 0; i < NUMBER_OF_SWITCH_PORTS; i++) {
		int rate = librouter_ksz8895_get_egress_rate_limit(port, i);
		if (rate != 100000)
			fprintf(out, "  traffic-shape %d %d\n", i, rate);
	}

	if (librouter_ksz8895_get_txqsplit(port))
		fprintf(out, "  txqueue-split\n");

	i = librouter_ksz8895_get_default_vid(port);
	if (i)
		fprintf(out, "  vlan-default %d\n", i);

}

int librouter_ksz8895_dump_config(FILE *out)
{
	int i;

	/* Is device present ? */
	if (librouter_ksz8895_probe() == 0)
		return 0;

	if (librouter_ksz8895_get_8021q())
		fprintf(out, " switch-config 802.1q\n");

	i = librouter_ksz8895_get_storm_protect_rate();
	if (i != 1)
		fprintf(out, " switch-config storm-protect-rate %d\n", i);

	for (i = 0; i < 8; i++) {
		int prio = librouter_ksz8895_get_cos_prio(i);
		if (prio != i / 2)
			fprintf(out, " switch-config cos-prio %d %d\n", i, prio);
	}

	for (i = 0; i < 64; i++) {
		int prio = librouter_ksz8895_get_dscp_prio(i);
		if (prio)
			fprintf(out, " switch-config dscp-prio %d %d\n", i, prio);
	}

	if (librouter_ksz8895_get_multicast_storm_protect())
		fprintf(out, " switch-config multicast-storm-protect\n");

	if (librouter_ksz8895_get_replace_null_vid())
		fprintf(out, " switch-config replace-null-vid\n");

	if (librouter_ksz8895_get_wfq())
		fprintf(out, " switch-config wfq\n");

	for (i = 0; i < KSZ8895_NUM_VLAN_TABLES; i++) {
		struct vlan_table_t v;

		librouter_ksz8895_get_table(i, &v);
		if (v.valid)
			fprintf(
			out,
			" switch-config vlan %d %s%s%s\n",
			i,
			(v.membership & KSZ8895REG_VLAN_MEMBERSHIP_PORT1_MSK) ? "port-1 " : "",
			(v.membership & KSZ8895REG_VLAN_MEMBERSHIP_PORT2_MSK) ? "port-2 " : "",
			(v.membership & KSZ8895REG_VLAN_MEMBERSHIP_PORT3_MSK) ? "port-3 " : "",
			(v.membership & KSZ8895REG_VLAN_MEMBERSHIP_PORT4_MSK) ? "port-4 " : "",
			(v.membership & KSZ8895REG_VLAN_MEMBERSHIP_PORT5_MSK) ? "internal" : "");
	}

	for (i = 0; i < NUMBER_OF_SWITCH_PORTS; i++)
		_dump_port_config(out, i);

	return 1;
}

#endif /* OPTION_MANAGED_SWITCH */
