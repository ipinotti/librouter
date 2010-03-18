#include <linux/config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>
#include <linux/hdlc.h>
#include <linux/sockios.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <libconfig/typedefs.h>
#include <libconfig/args.h>
#include <libconfig/error.h>
#include <libconfig/ppp.h>
#include <libconfig/dev.h>
#include <libconfig/defines.h>
#include <libconfig/ppcio.h>
#include <libconfig/lan.h>

int lan_get_status(char *ifname)
{
	int fd, err; //, status;
	char *p;
	struct ifreq ifr;

	/* Create a socket to the INET kernel. */
	if ((fd=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		pr_error(1, "lan_get_status: socket");
		return(-1);
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);
	if ((p=strchr(ifr.ifr_name, '.')) != NULL) *p=0; /* vlan uses ethernetX status! */

#if 0 
	err=ioctl(fd, SIOCGPHYSTATUS, &ifr);
	close(fd);

	if (err < 0) {
 		if (errno != ENODEV) return 0;
		pr_error(1, "SIOCGPHYSTATUS");
		return(-1);
	}
#endif
	return ifr.ifr_ifru.ifru_ivalue;
}

int lan_get_phy_reg(char *ifname, u16 regnum)
{
	int fd;
	char *p;
	struct ifreq ifr;
	struct mii_ioctl_data mii;

	/* Create a socket to the INET kernel. */
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		pr_error(1, "lan_get_phy_reg: socket");
		return -1;
	}
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);
	if((p = strchr(ifr.ifr_name, '.')) != NULL)
		*p = 0;	/* vlan uses ethernetX status! */
	ifr.ifr_data = (char *)&mii;
	mii.reg_num = regnum;
	if(ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		close(fd);
		pr_error(1, "lan_get_phy_reg: SIOCGMIIPHY");
		return -1;
	}
#if 0
	printf("SIOCGMIIPHY (0x%02x=0x%04x)\n", mii.reg_num, mii.val_out);
#endif
	close(fd);
	return (mii.val_out);
}

int lan_set_phy_reg(char *ifname, u16 regnum, u16 data)
{
	int fd;
	char *p;
	struct ifreq ifr;
	struct mii_ioctl_data mii;

	/* Create a socket to the INET kernel. */
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		pr_error(1, "lan_set_phy_reg: socket");
		return -1;
	}
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ifname);
	if((p = strchr(ifr.ifr_name, '.')) != NULL)
		*p = 0;	/* vlan uses ethernetX status! */

	ifr.ifr_data=(char *)&mii;
	mii.reg_num = regnum;
	if(ioctl(fd, SIOCGMIIPHY, &ifr) < 0) {
		close(fd);
		pr_error(1, "lan_set_phy_reg: SIOCGMIIPHY");
		return -1;
	}
	mii.val_in = data;
	if(ioctl(fd, SIOCSMIIREG, &ifr) < 0) {
		close(fd);
		pr_error(1, "lan_set_phy_reg: SIOCSMIIREG");
		return -1;
	}
	close(fd);
	return 0;
}

#ifdef CONFIG_BERLIN_SATROUTER

void configure_auto_mdix(char *dev)
{
	int result;
	unsigned short data;

	switch( get_board_hw_id() )
	{
		case BOARD_HW_ID_0:
		case BOARD_HW_ID_1:
			if( (result = lan_get_phy_reg(dev, MII_100BTX_PHY_C)) >= 0 )
			{
				data = (unsigned short) result;
				data &= ~0x2000;
				lan_set_phy_reg(dev, MII_100BTX_PHY_C, data);
			}
			break;
		case BOARD_HW_ID_2:
		case BOARD_HW_ID_3:
		case BOARD_HW_ID_4:
			if( !strcmp( dev, "ethernet0" ) )
			{
				if( (result = lan_get_phy_reg(dev, MII_100BTX_PHY_C)) >= 0 )
				{
					data = (unsigned short) result;
					data &= ~0x2000;
					lan_set_phy_reg(dev, MII_100BTX_PHY_C, data);
				}
			}
			break;
	}
}

static unsigned int is_link_up(char *dev)
{
	int result;
	unsigned short data;

	if( (result = lan_get_phy_reg(dev, MII_BMSR)) >= 0 )
	{
		data = (unsigned short) result;
		return ( (data & 0x0004) ? 1 : 0 );
	}
	return 0;
}

/* Procedimento para forcar renegociacao do link fisico com o remoto */
int force_fec_restart_link(char *dev)
{
	int i, fd;
	struct ifreq ifr;
	struct ethtool_cmd ecmd;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	/* Open control socket. */
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;

	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t) &ecmd;
	if(ioctl(fd, SIOCETHTOOL, &ifr) < 0)
	{
		close(fd);
		return -1;
	}
	for( i=0; i < 2; i++ )
	{
		ecmd.cmd = ETHTOOL_SSET;
		ecmd.speed = ( (ecmd.speed == SPEED_10) ? SPEED_100 : SPEED_10 );
		ifr.ifr_data = (caddr_t) &ecmd;
		if(ioctl(fd, SIOCETHTOOL, &ifr) < 0)
		{
			close(fd);
			return -1;
		}
		if( i == 0 )
			usleep(100000);	/* delay de 100ms */
	}
	close(fd);

	return 0;
}

int autonegotiate_link(char *dev, unsigned short advertise)
{
	int fd;
	struct ifreq ifr;
	struct ethtool_cmd ecmd;

	if( !dev )
		return -1;

	switch( get_board_hw_id() )
	{
		case BOARD_HW_ID_0:
			break;
		case BOARD_HW_ID_1:
			break;
		case BOARD_HW_ID_2:
		case BOARD_HW_ID_3:
		case BOARD_HW_ID_4:
			if( !strcmp(dev, "ethernet1") )
			{
				int result;
				unsigned short bmcr;

				if( (result = lan_get_phy_reg(dev, MII_BMCR)) < 0 )
					return -1;
				bmcr = (unsigned short) result;
				bmcr |= 0x1000;	/* enable autonegotiation */
				bmcr |= 0x0200;	/* restart autonegotiation */
				return eth_switch_ports_set(dev, (int) bmcr);
			}
			break;
	}

	configure_auto_mdix(dev);

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	/* Open control socket. */
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;

	memset(&ecmd, 0, sizeof(struct ethtool_cmd));
	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t) &ecmd;
	if(ioctl(fd, SIOCETHTOOL, &ifr) < 0)
	{
		close(fd);
		return -1;
	}

	ecmd.cmd = ETHTOOL_SSET;
	ecmd.advertising = advertise;
	ecmd.autoneg = AUTONEG_ENABLE;
	ifr.ifr_data = (caddr_t) &ecmd;
	if(ioctl(fd, SIOCETHTOOL, &ifr) < 0)
	{
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

int fec_autonegotiate_link(char *dev)
{
	int ret, force_on = ( dev_get_link(dev) ? 0 : 1);

	if( force_on )
		dev_set_link_up(dev);
	ret = autonegotiate_link(dev, (ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full | ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full));
	if( force_on )
		dev_set_link_down(dev);

	return ret;
}

int fec_config_link(char *dev, int speed100, int duplex)
{
	struct ifreq ifr;
	struct ethtool_cmd ecmd;
	unsigned short lpa, bmsr;
	int i, fd, result, impossible = 0;

	switch( get_board_hw_id() )
	{
		case BOARD_HW_ID_0:
			break;
		case BOARD_HW_ID_1:
			break;
		case BOARD_HW_ID_2:
		case BOARD_HW_ID_3:
		case BOARD_HW_ID_4:
			if( !strcmp(dev, "ethernet1") )
			{
				unsigned short bmcr;
				int ret, force_on = ( dev_get_link(dev) ? 0 : 1);

				if( force_on )
					dev_set_link_up(dev);
				if( (result = lan_get_phy_reg(dev, MII_BMCR)) < 0 )
				{
					if( force_on )
						dev_set_link_down(dev);
					return -1;
				}
				bmcr = (unsigned short) result;
				bmcr &= ~0x1000;	/* disable autonegotiation */
				bmcr &= ~0x2100;	/* configure 10M and half-duplex */
				if( speed100 )
					bmcr |= 0x2000; /* 100M */
				if( duplex )
					bmcr |= 0x0100; /* full-duplex */
				ret = eth_switch_ports_set(dev, (int) bmcr);
				if( force_on )
					dev_set_link_down(dev);
				else
				{
					eth_switch_extport_powerdown(dev, 1);
					usleep(100000);
					eth_switch_extport_powerdown(dev, 0);
				}
				return ret;
			}
			break;
	}

	configure_auto_mdix(dev);

	/* Para o teste da capacidade do link eh necessario que o cabo esteja conectado */
	if( !is_link_up(dev) )
	{
		printf("%% Link is down. Not possible to test link partner abilities\n");
		printf("%% Forcing mode anyway\n");
	}
	else
	{
		if(fec_autonegotiate_link(dev) < 0)
			return -1;

		for( i=0; i < 50; i++ )
		{
			usleep(100000);	/* 100ms */
			if( (result = lan_get_phy_reg(dev, MII_BMSR)) < 0 )
				continue;
			bmsr = (unsigned short) result;
			if( bmsr & BMSR_ANEGCOMPLETE )
				break;
		}
		if( i >= 50 )
			return -1;

		if( (result = lan_get_phy_reg(dev, MII_LPA)) < 0 )
			return -1;

		lpa = (unsigned short) result;
		if( speed100 )
		{
			if( !(lpa & LPA_100) )
				impossible++;
			else
			{
				if( duplex )
				{
					if( !(lpa & LPA_100FULL) )
						impossible++;
				}
			}
		}
		else
		{
			if( duplex )
			{
				if( !(lpa & LPA_10FULL) )
					impossible++;
			}
		}

		if( impossible )
		{
			printf("%% Link partner abilities are not enough for this mode\n");
			printf("%% Forcing mode anyway\n");
		}
		else
		{
			#if 0
			if( !speed100 )
			{
				/* Caso especial:
				*  Se a velocidade a ser configurada eh 10 Mbps entao
				*  configuramos ela atraves do auto-negotiation.
				*/
				return autonegotiate_link(dev, ( duplex ? ADVERTISED_10baseT_Full : ADVERTISED_10baseT_Half ));
			}
			#endif
		}
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	/* Open control socket. */
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;

	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t) &ecmd;
	if(ioctl(fd, SIOCETHTOOL, &ifr) < 0)
	{
		close(fd);
		return -1;
	}
	ecmd.cmd = ETHTOOL_SSET;
	ecmd.speed = speed100 ? SPEED_100 : SPEED_10;
	ecmd.duplex = duplex ? DUPLEX_FULL : DUPLEX_HALF;
	ecmd.autoneg = AUTONEG_DISABLE;
	ifr.ifr_data = (caddr_t) &ecmd;
	if(ioctl(fd, SIOCETHTOOL, &ifr) < 0)
	{
		close(fd);
		return -1;
	}
	close(fd);

	return force_fec_restart_link(dev);
}

#if 0
unsigned int set_phy_conf(char *dev, unsigned int speed, unsigned int duplex)
{
	FILE *f;
	char buf[100];
	
	if( !dev )
		return 0;
	if( !strcmp(dev, "ethernet0") )
		f = fopen(FILE_PHY_0_CFG, "w");
	else if( !strcmp(dev, "ethernet1") )
		f = fopen(FILE_PHY_1_CFG, "w");
	else
		return 0;
	if( !f )
		return 0;
	sprintf(buf, "dev: %s speed: %d duplex: %d\n", dev, speed ? 1 : 0, duplex ? 1 : 0);
	fwrite(buf, 1, strlen(buf), f);
	fclose(f);
	return 1;
}

unsigned int get_phy_conf(char *dev, unsigned int *speed, unsigned int *duplex)
{
	FILE *f;
	char *p, buf[100];
	arg_list argl=NULL;
	
	if( !dev )
		return 0;
	if( !strcmp(dev, "ethernet0") )
		f = fopen(FILE_PHY_0_CFG, "r");
	else if( !strcmp(dev, "ethernet1") )
		f = fopen(FILE_PHY_1_CFG, "r");
	else
		return 0;
	if( !f )
		return 0;
	
	while( !feof(f) )
	{
		fgets(buf, 99, f);
		buf[99] = 0;
		if( (p = strchr(buf, '\n')) )
			*p = 0;
		if(parse_args_din(buf, &argl) == 6)
		{
			if( !strcmp(argl[0], "dev:") && !strcmp(argl[1], dev) && !strcmp(argl[2], "speed:") && !strcmp(argl[4], "duplex:") )
			{
				*speed = (argl[3][0]=='1' ? 1 : 0);
				*duplex = (argl[5][0]=='1' ? 1 : 0);
				free_args_din(&argl);
				fclose(f);
				return 1;
			}
		}
		free_args_din(&argl);
	}
	fclose(f);
	return 0;
}
#endif

int eth_switch_port_get_status(char *ifname, int *status)
{
	char *p;
	int fd, err;
	struct ifreq ifr;

	if( !ifname || !status )
		return -1;

	/* Create a socket to the INET kernel. */
	if( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		pr_error(1, "eth_switch_ports_get_status: socket");
		return -1;
	}
	strcpy(ifr.ifr_name, ifname);
	if( (p = strchr(ifr.ifr_name, '.')) != NULL )
		*p=0; /* vlan uses ethernetX status! */
	ifr.ifr_data = (char *) status;
	err = ioctl(fd, SIOGSWITCHIFPORTST, &ifr);
	close(fd);
	if(err < 0)
	{
		if(errno != ENODEV)
			return -1;
		pr_error(1, "SIOGSWITCHIFPORTST");
		return -1;
	}
	return 0;
}

int eth_switch_ports_set(char *ifname, int conf)
{
	char *p;
	int fd, err;
	struct ifreq ifr;

	if( !ifname )
		return -1;

	/* Create a socket to the INET kernel. */
	if( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		pr_error(1, "eth_switch_ports_get_status: socket");
		return -1;
	}
	strcpy(ifr.ifr_name, ifname);
	if( (p = strchr(ifr.ifr_name, '.')) != NULL )
		*p=0; /* vlan uses ethernetX status! */
	ifr.ifr_data = (char *) &conf;
	err = ioctl(fd, SIOSSWITCHIFPORT, &ifr);
	close(fd);
	if(err < 0)
	{
		if(errno != ENODEV)
			return -1;
		pr_error(1, "SIOSSWITCHIFPORT");
		return -1;
	}
	return 0;
}

int eth_switch_extport_powerdown(char *ifname, int pwd)
{
	char *p;
	int fd, err;
	struct ifreq ifr;

	if( !ifname )
		return -1;

	/* Create a socket to the INET kernel. */
	if( (fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		pr_error(1, "eth_switch_ports_get_status: socket");
		return -1;
	}
	strcpy(ifr.ifr_name, ifname);
	if( (p = strchr(ifr.ifr_name, '.')) != NULL )
		*p=0; /* vlan uses ethernetX status! */
	ifr.ifr_data = NULL;
	if( pwd )
		err = ioctl(fd, SIOSSWITCHIFPOWERDOWN, &ifr);
	else
		err = ioctl(fd, SIOSSWITCHIFPOWERUP, &ifr);
	close(fd);
	if(err < 0)
	{
		if(errno != ENODEV)
			return -1;
		if( pwd )
			pr_error(1, "SIOSSWITCHIFPOWERDOWN");
		else
			pr_error(1, "SIOSSWITCHIFPOWERUP");
		return -1;
	}
	return 0;
}
#else /* CONFIG_BERLIN_SATROUTER */
#ifdef CONFIGURE_MDIX
static void configure_auto_mdix(char *dev)
{
	int gpcr;

	if ((gpcr = lan_get_phy_reg(dev, MII_ADM7001_GPCR)) < 0)
		return;
	gpcr |= MII_ADM7001_GPCR_XOVEN;
	lan_set_phy_reg(dev, MII_ADM7001_GPCR, gpcr);
}
#endif

#undef ADVERTISE

int fec_autonegotiate_link(char *dev)
{
	int bmcr;
#ifdef ADVERTISE
	int advertise
#endif

#ifdef CONFIG_MDIX
	configure_auto_mdix(dev);
#endif
#ifdef ADVERTISE
	if ((advertise = lan_get_phy_reg(dev, MII_ADVERTISE)) < 0)
		return -1;
	advertise |= (ADVERTISE_10HALF | ADVERTISE_10FULL | ADVERTISE_100HALF | ADVERTISE_100FULL);
	lan_set_phy_reg(dev, MII_ADVERTISE, advertise);
#endif
	if ((bmcr = lan_get_phy_reg(dev, MII_BMCR)) < 0)
		return -1;
	bmcr |= (BMCR_ANENABLE | BMCR_ANRESTART);
	if (lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0)
		return -1;
	return 0;
}

#undef VERIFY_LP

int fec_config_link(char *dev, int speed100, int duplex)
{
	int bmcr;
#ifdef VERIFY_LP
	int lpa , impossible = 0;
#endif

#ifdef CONFIG_MDIX
	configure_auto_mdix(dev);
#endif
#ifdef VERIFY_LP /* Verify link partner */
	if ((lpa = lan_get_phy_reg(dev, MII_LPA)) < 0)
		return -1;
	if (speed100) {
		if (!(lpa & LPA_100))
			impossible++;
		else {
			if (duplex) {
				if (!(lpa & LPA_100FULL))
					impossible++;
			}
		}
	} else {
		if (duplex) {
			if (!(lpa & LPA_10FULL))
				impossible++;
		}
	}
	if (impossible) {
		printf("%% Link partner abilities are not enough for this mode\n");
		printf("%% Forcing mode anyway\n");
	}
#endif
	if ((bmcr = lan_get_phy_reg(dev, MII_BMCR)) < 0)
		return -1;
	if (!(bmcr & BMCR_PDOWN)) {
		int advertise, icsr; /* backups */

		if ((advertise = lan_get_phy_reg(dev, MII_ADVERTISE)) < 0)
			return -1;
		if ((icsr = lan_get_phy_reg(dev, 0x1b)) < 0)
			return -1;

		bmcr |= BMCR_PDOWN; /* alert partner! */
		if (lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0)
			return -1;

		sleep(1);
		bmcr &= (~BMCR_PDOWN);

		if (lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0) /* wake-up! (delay!) */
			return -1;
		/* KS8721 re-sample io pins! */
		if (lan_set_phy_reg(dev, 0x1b, icsr) < 0) /* restore! */
			return -1;
		if (lan_set_phy_reg(dev, MII_ADVERTISE, advertise) < 0) /* restore! */
			return -1;
	}
	bmcr &= ~BMCR_ANENABLE; /* disable auto-negotiate */
	if (speed100) bmcr |= BMCR_SPEED100;
		else bmcr &= (~BMCR_SPEED100);
	if (duplex) bmcr |= BMCR_FULLDPLX;
		else bmcr &= (~BMCR_FULLDPLX);
	if (lan_set_phy_reg(dev, MII_BMCR, bmcr) < 0)
		return -1;
	return 0;
}
#endif /* CONFIG_BERLIN_SATROUTER */

