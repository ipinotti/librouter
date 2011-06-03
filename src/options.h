/*
 * options.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <linux/autoconf.h>

/*****************************************************************************/
/* Defines for Digistar Models ***********************************************/
/*****************************************************************************/

#if defined(CONFIG_DIGISTAR_3G)

#define CFG_PRODUCT	"3GRouter"

#define OPTION_ETHERNET /* Enables ethernet interfaces */
#define OPTION_ETHERNET_LAN_INDEX 0
#define OPTION_GIGAETHERNET /* Enable gigabit options */
#define OPTION_PPPOE
#define OPTION_PPTP
#define OPTION_TUNNEL
#define OPTION_ROUTER /* ip forwarding, enable dynamic routing protocols, etc. */
#define OPTION_SMCROUTE /* static multicast routing */

/* IPSec */
#define OPTION_IPSEC
#define N_IPSEC_IF 5

/* BGP */
#define OPTION_BGP

/* PIM */
#define OPTION_PIMD

/* IPtables */
#define OPTION_FIREWALL
#define OPTION_NAT
#define OPTION_QOS


/* ------ Digistar 3G Product Models Declaration ------ */
/* RCG1000 */
#if defined(CONFIG_DIGISTAR_RCG1000)
#define OPTION_MANAGED_SWITCH
#define OPTION_ETHERNET_WAN
#define OPTION_MODEM3G
#define OPTION_DUAL_SIM
#define OPTION_PPP
#define OPTION_NUM_ETHERNET_IFACES	2
#define OPTION_NUM_3G_IFACES	3

/* RCG800 */
#elif defined(CONFIG_DIGISTAR_RCG800)
#define OPTION_MANAGED_SWITCH
#define OPTION_MODEM3G
#define OPTION_DUAL_SIM
#define OPTION_PPP
#define OPTION_NUM_ETHERNET_IFACES	1
#define OPTION_NUM_3G_IFACES	3

/* RCG700 */
#elif defined(CONFIG_DIGISTAR_RCG700)
#define OPTION_MANAGED_SWITCH
#define OPTION_ETHERNET_WAN
#define OPTION_PPP
#define OPTION_NUM_ETHERNET_IFACES	2


#else
#error "No 3G board defined!"
#endif

#elif defined(CONFIG_DIGISTAR_EFM)

#define CFG_PRODUCT	"EFMRouter"

#define OPTION_EFM
#define OPTION_ETHERNET
#define OPTION_ETHERNET_LAN_INDEX 0
#define OPTION_MANAGED_SWITCH
#define OPTION_NUM_ETHERNET_IFACES	1
#undef OPTION_PPPOE
#undef OPTION_PPTP

/* AAA */
#define OPTION_AAA_ACCOUNTING
#define OPTION_AAA_AUTHORIZATION

//#define OPTION_MODEM3G
//#define OPTION_PPP
//#define OPTION_NUM_3G_IFACES	1

/* ETL8110 Options */
#if defined(CONFIG_DIGISTAR_ETL8110)
#define OPTION_NUM_EFM_CHANNELS	4
#undef OPTION_IP_ROUTING
#undef OPTION_IPSEC
#undef OPTION_FIREWALL
#undef OPTION_NAT
#undef OPTION_QOS
#undef OPTION_ROUTER
#undef OPTION_SMCROUTE
#undef OPTION_TUNNEL

/* ETL8210 Options */
#elif defined(CONFIG_DIGISTAR_ETL8210)
#define OPTION_NUM_EFM_CHANNELS	4
#undef OPTION_IP_ROUTING
#undef OPTION_IPSEC
#undef OPTION_FIREWALL
#undef OPTION_NAT
#undef OPTION_QOS
#undef OPTION_ROUTER
#undef OPTION_SMCROUTE
#undef OPTION_TUNNEL

/* ETL8221 Options */
#elif defined(CONFIG_DIGISTAR_ETL8221)
#define OPTION_NUM_EFM_CHANNELS	4
#define OPTION_IP_ROUTING
#define OPTION_IPSEC
#define N_IPSEC_IF 5
#define OPTION_FIREWALL
#define OPTION_NAT
#define OPTION_QOS
#define OPTION_ROUTER
#define OPTION_SMCROUTE
#define OPTION_TUNNEL

/* ETL8421 Options */
#elif defined(CONFIG_DIGISTAR_ETL8421)
#define OPTION_NUM_EFM_CHANNELS	4
#define OPTION_IP_ROUTING
#define OPTION_IPSEC
#define N_IPSEC_IF 5
#define OPTION_FIREWALL
#define OPTION_NAT
#define OPTION_QOS
#define OPTION_ROUTER
#define OPTION_SMCROUTE
#define OPTION_TUNNEL
#else
#error "No EFM board defined!"
#endif




#undef OPTION_DUAL_SIM

/* TODO */
/* ------ Digistar EFM Product Models Declaration ------ */
/* EFM MOD1 */

/* EFM MOD2 */

/* EFM MOD3 */

#else
#error "No such board!"
#endif
/*=====================================================*/

/*****************************************************************************/
/*****************************************************************************/
/* Maximum hostname size */
#define OPTION_HOSTNAME_MAXSIZE	30

/* Bridging */
#define OPTION_BRIDGE

#define OPTION_FULL_CONSOLE_LOG 0

/* NTP */
#define OPTION_NTPD
#undef OPTION_NTPD_authenticate

/* QoS */
#define QOS_MIN_BANDWIDTH	1000 /* 1Kbps */
#ifdef OPTION_GIGAETHERNET
#define QOS_MAX_BANDWIDTH	1000000000 /* 1Gbps */
#else
#define QOS_MAX_BANDWIDTH	100000000 /* 100Mbps */
#endif

#define OPTION_OPENSSH
#define OPTION_RMON
#undef OPTION_VRRP
#define OPTION_HTTP
#define OPTION_HTTPS

#undef OPTION_DHCP_NETBIOS

/* HTTP Server */
#define HTTP_DAEMON		"wnsd"
#define HTTPS_DAEMON		"wnsslsd"
#define SSH_DAEMON		"sshd"
#define TELNET_DAEMON		"telnetd"
#define FTP_DAEMON		"ftpd"
#define PIMS_DAEMON 		"pimsd"
#define PIMD_DAEMON 		"pimdd"
#define SMC_DAEMON 		"smcroute"

#endif /* OPTIONS_H_ */
