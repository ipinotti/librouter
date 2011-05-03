/*
 * lan.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef LAN_H_
#define LAN_H_

//#define LAN_DEBUG
#ifdef LAN_DEBUG
#define lan_dbg(x,...) \
		syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__); \
		printf("%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define lan_dbg(x,...)
#endif

enum media_type {
	MEDIA_TYPE_COPPER,
	MEDIA_TYPE_FIBER
};

struct lan_status {
	char link;
	char autoneg;
	int speed;
	char duplex;
	enum media_type media;
};

/* prototipos de funcoes */
int librouter_lan_get_status(char *ifname, struct lan_status *st);
int librouter_lan_get_phy_reg(char *ifname, unsigned short regnum);
int librouter_lan_set_phy_reg(char *ifname, unsigned short regnum, unsigned short data);
int librouter_fec_autonegotiate_link(char *dev);
int librouter_fec_config_link(char *dev, int speed, int duplex);
int librouter_phy_probe(char *ifname);

#endif /* LAN_H_ */
