/*
 * lan.c
 *
 *  Created on: Jun 24, 2010
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/hdlc.h>
#include <linux/sockios.h>
#include <linux/mii.h>
#include <linux/ethtool.h>

#include "typedefs.h"
#include "args.h"
#include "error.h"
#include "ppp.h"
#include "dev.h"
#include "defines.h"
#include "ppcio.h"
#include "lan.h"

/**
 * _set_ethtool		Configure a interface via EthTool
 *
 * @param ifname
 * @param cmd
 * @return 0 if success, -1 if error
 */
static int _set_ethtool(char *ifname, struct ethtool_cmd *cmd)
{
	int fd;
	struct ifreq ifr;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_get_status: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);

	cmd->cmd = ETHTOOL_SSET;
	ifr.ifr_data = (void *) cmd;

	lan_dbg("autoneg %d speed %d duplex %d\n", cmd->autoneg, cmd->speed, cmd->duplex);

	if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
		perror("ETHTOOL");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

/**
 * _get_ethtool		Get an interface configuration via EthTool
 *
 * @param ifname
 * @param cmd
 * @return 0 if success, -1 if error
 */
static int _get_ethtool(char *ifname,  struct ethtool_cmd *cmd)
{
	int fd;
	struct ifreq ifr;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_get_status: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);

	cmd->cmd = ETHTOOL_GSET;
	ifr.ifr_data = (void *) cmd;

	if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
		perror("ETHTOOL");
		close(fd);
		return -1;
	}

	lan_dbg("autoneg %d speed %d duplex %d\n", cmd->autoneg, cmd->speed, cmd->duplex);

	close(fd);
	return 0;
}

/**
 * _get_ethtool		Get an interface configuration via EthTool
 *
 * @param ifname
 * @param cmd
 * @return 0 if success, -1 if error
 */
static int _ethtool_get_link(char *ifname)
{
	int fd;
	struct ifreq ifr;
	struct ethtool_value data;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_get_status: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);

	data.cmd = ETHTOOL_GLINK;
	ifr.ifr_data = (void *) &data;

	if (ioctl(fd, SIOCETHTOOL, &ifr) < 0) {
		perror("ETHTOOL");
		close(fd);
		return -1;
	}

	lan_dbg("link %d\n", data.data);

	close(fd);
	return data.data;
}

/**
 * librouter_fec_autonegotiate_link	Enable autonegotiation on an interface
 *
 * @param dev
 * @return 0 if success, -1 if error
 */
int librouter_fec_autonegotiate_link(char *dev)
{
	struct ethtool_cmd cmd;

	_get_ethtool(dev, &cmd);

	cmd.autoneg = AUTONEG_ENABLE;

	_set_ethtool(dev, &cmd);

	return 0;
}

/**
 * librouter_fec_config_link	Set a fixed speed and duplex on an interface
 *
 * @param dev
 * @param speed
 * @param duplex
 * @return 0 if success, -1 if error
 */
int librouter_fec_config_link(char *dev, int speed, int duplex)
{
	struct ethtool_cmd cmd;

	_get_ethtool(dev, &cmd);

	cmd.autoneg = AUTONEG_DISABLE;
	ethtool_cmd_speed_set(&cmd, speed);
	cmd.duplex = (duplex ? DUPLEX_FULL : DUPLEX_HALF);

	_set_ethtool(dev, &cmd);

	return 0;
}

/**
 * librouter_lan_get_status	Get status of an ethernet interface
 *
 * @param ifname
 * @param st
 * @return 0 if success, -1 if error
 */
int librouter_lan_get_status(char *ifname, struct lan_status *st)
{
	struct ethtool_cmd cmd;
	char *p, *real_iface;

	/* VLAN uses ethernetX status */
	real_iface = strdup(ifname);
	if ((p = strchr(real_iface, '.')) != NULL)
		*p = 0;

	_get_ethtool(ifname, &cmd);

	st->autoneg = cmd.autoneg;
	st->speed = cmd.speed;
	st->duplex = cmd.duplex;
	st->link = _ethtool_get_link(ifname);

	free(real_iface);

	return 0;
}

/**
 * librouter_lan_get_phy_reg	Read a PHY register
 *
 * Use ethtool functions whenever possible
 *
 * @param ifname
 * @param regnum
 * @return 0 if success, -1 if error
 */
int librouter_lan_get_phy_reg(char *ifname, u16 regnum)
{
	int fd;
	char *p;
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (void *) &ifr.ifr_data;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_get_phy_reg: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);

	/* vlan uses ethernetX status! */
	if ((p = strchr(ifr.ifr_name, '.')) != NULL)
		*p = 0;

	mii->reg_num = regnum;

	if (ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		close(fd);
		librouter_pr_error(1, "Error reading PHY register for %s: SIOCGMIIPHY", ifname);
		return -1;
	}

	lan_dbg("SIOCGMIIPHY for %s --> (0x%02x=0x%04x)\n", ifname, mii->reg_num, mii->val_out);

	close(fd);

	return (mii->val_out);
}

/**
 * librouter_lan_set_phy_reg	Set a PHY register
 *
 * Use ethtool functions whenever possible
 *
 * @param ifname
 * @param regnum
 * @param data
 * @return 0 if success, -1 if error
 */
int librouter_lan_set_phy_reg(char *ifname, u16 regnum, u16 data)
{
	int fd;
	char *p;
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (void *) &ifr.ifr_data;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "lan_set_phy_reg: socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);

	/* vlan uses ethernetX status! */
	if ((p = strchr(ifr.ifr_name, '.')) != NULL)
		*p = 0;

	mii->reg_num = regnum;

	if (ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		close(fd);
		librouter_pr_error(1, "lan_set_phy_reg: SIOCGMIIPHY");
		return -1;
	}

	lan_dbg("SIOCGMIIPHY for %s --> (0x%02x=0x%04x)\n", ifname, mii->reg_num, mii->val_out);

	mii->val_in = data;

	if (ioctl(fd, SIOCSMIIREG, &ifr) < 0) {
		close(fd);
		librouter_pr_error(1, "lan_set_phy_reg: SIOCSMIIREG");
		return -1;
	}

	lan_dbg("SIOCSMIIREG for %s --> (0x%02x=0x%04x)\n", ifname, mii->reg_num, mii->val_in);

	close(fd);
	return 0;
}

/**
 * librouter_phy_probe	Probe for a PHY
 *
 * @param ifname
 * @return 1 if PHY found, 0 otherwise
 */
int librouter_phy_probe(char *ifname)
{
	u32 id = 0;

	id = librouter_lan_get_phy_reg(ifname, MII_PHYSID2) & 0xffff;
	id |= ((librouter_lan_get_phy_reg(ifname, MII_PHYSID1) & 0xffff) << 16);
	id &= 0xfffffff0; /* Clear silicom revision nibble */

	lan_dbg("PHY ID : %08x\n", id);

#if defined(CONFIG_DIGISTAR_EFM)
	if (id == 0x02430c50) /* Micrel KSZ8863 */
		return 1;
#elif defined(CONFIG_DIGISTAR_3G)
	if (id == 0x002060C0) /* Broadcom BCM5461S */
		return 1;
#endif

	return 0;
}
