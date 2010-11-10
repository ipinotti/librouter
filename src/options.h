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

/* ------ Digistar 3G Product Models Declaration ------ */
/* RCG1000 */
#ifdef CONFIG_DIGISTAR_RCG1000
#define OPTION_MODEM3G
#define OPTION_PPP
#define OPTION_DHCP_SWITCH_ETH0
#endif

/* RCG800 */
#ifdef CONFIG_DIGISTAR_RCG800
#define OPTION_NO_WAN
#define OPTION_MODEM3G
#define OPTION_PPP
#define OPTION_DHCP_SWITCH_ETH0
#endif

/* RCG700 */
#ifdef CONFIG_DIGISTAR_RCG700
#define OPTION_DHCP_SWITCH_ETH0
#endif
/*-----------------------------------------------------*/

/* ====== Digistar EFM Product Models Declaration ==== */
/* EFM MOD1 */

/* EFM MOD2 */

/* EFM MOD3 */
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
