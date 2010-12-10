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

#define OPTION_ETHERNET /* Enables ethernet interfaces */
#define OPTION_GIGAETHERNET /* Enable gigabit options */
#define OPTION_IP_ROUTING

/* ------ Digistar 3G Product Models Declaration ------ */
/* RCG1000 */
#if defined(CONFIG_DIGISTAR_RCG1000)
#define OPTION_ETHERNET_WAN
#define OPTION_MODEM3G
#define OPTION_PPP
#define OPTION_DHCP_SWITCH_ETH0
#define OPTION_NUM_ETHERNET_IFACES	2

/* RCG800 */
#elif defined(CONFIG_DIGISTAR_RCG800)
#define OPTION_ETHERNET_WAN
#define OPTION_MODEM3G
#define OPTION_PPP
#define OPTION_DHCP_SWITCH_ETH0
#define OPTION_NUM_ETHERNET_IFACES	1

/* RCG700 */
#elif defined(CONFIG_DIGISTAR_RCG700)
#define OPTION_ETHERNET_WAN
#define OPTION_PPP
#define OPTION_DHCP_SWITCH_ETH0
#define OPTION_NUM_ETHERNET_IFACES	2

#else
#error "No such 3G Board!"
#endif

#elif defined(CONFIG_DIGISTAR_EFM)

#define OPTION_EFM
#define OPTION_MANAGED_SWITCH
#define OPTION_NUM_ETHERNET_IFACES	1

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

/* MODEM3G */
#define NUM_3G_INTFS	3
//#define OPTION_MODEM3G
//#define OPTION_PPP

/* BGP */
#define OPTION_BGP

/* Bridging */
#define OPTION_BRIDGE

/* IPSec */
#define OPTION_IPSEC 1
#define N_IPSEC_IF 5

#define OPTION_FULL_CONSOLE_LOG 0

/* NTP */
#define OPTION_NTPD
#undef OPTION_NTPD_authenticate

/* AAA */
#undef OPTION_AAA_ACCOUNTING
#undef OPTION_AAA_AUTHORIZATION

/* QoS */
#define QOS_MIN_BANDWIDTH	1000 /* 1Kbps */
#ifdef OPTION_GIGAETHERNET
#define QOS_MAX_BANDWIDTH	1000000000 /* 1Gbps */
#else
#define QOS_MAX_BANDWIDTH	100000000 /* 100Mbps */
#endif

#define OPTION_OPENSSH
#define OPTION_PIMD
#define OPTION_RMON
#define OPTION_SMCROUTE
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
