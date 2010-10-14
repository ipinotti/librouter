/*
 * lan.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef LAN_H_
#define LAN_H_


/* Defines para os registros do PHY ethernet */
#define MII_ADM7001_GPCR 0x10 /* Generic PHY Control/Configuration Register */
#define MII_ADM7001_GPCR_XOVEN 0x0010 /* Cross Over Auto Detect Enable */
#define MII_ADM7001_GPCR_DPMG 0x0001 /* Disable Power Management Feature */
#define MII_ADM7001_PGSR 0x16 /* PHY Generic Status Register */
#define MII_ADM7001_PGSR_MD 0x0400 /* Medium Detect */
#define MII_ADM7001_PGSR_XOVER 0x0100 /* Cross Over status */
#define MII_ADM7001_PGSR_CBLEN 0x00ff /* cable length */
#define MII_ADM7001_PSSR 0x17 /* PHY Specific Status Register */
#define MII_ADM7001_PSSR_SPD 0x0020 /* Operating speed */

#include <linux/mii.h>

/* prototipos de funcoes */
int librouter_lan_get_status(char *ifname, struct lan_status *st);
int librouter_lan_get_phy_reg(char *ifname, unsigned short regnum);
int librouter_lan_set_phy_reg(char *ifname, unsigned short regnum, unsigned short data);
int librouter_fec_autonegotiate_link(char *dev);
int librouter_fec_config_link(char *dev, int speed100, int duplex);

#if 0
int eth_switch_port_get_status(char *ifname, int *status);
int eth_switch_ports_set(char *ifname, int conf);
int eth_switch_extport_powerdown(char *ifname, int pwd);
#endif

#endif /* LAN_H_ */
