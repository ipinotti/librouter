/*
 * options.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef OPTIONS_H_
#define OPTIONS_H_

#ifndef DO_NOT_INCLUDE_AUTOCONF
#include <linux/autoconf.h>
#endif

/*****************************************************************************/
/* Defines for Digistar Models ***********************************************/
/*****************************************************************************/

#if defined(CONFIG_DIGISTAR_3G) /* RCG */

#define CFG_PRODUCT	"3GRouter"

#define OPTION_ETHERNET /* Enables ethernet interfaces */
#define OPTION_ETHERNET_LAN_INDEX 0
#define OPTION_GIGAETHERNET /* Enable gigabit options */
#define OPTION_PPPOE
#define OPTION_PPTP
#define OPTION_TUNNEL
#define OPTION_ROUTER /* ip forwarding, enable dynamic routing protocols, etc. */
#define OPTION_SMCROUTE /* static multicast routing */

/* Factory Test System Digistar*/
#define OPTION_FTS_DIGISTAR

/* PBR - Policy Based Routing */
/* DEPENDE DE OPTION_ROUTER*/
#ifdef OPTION_ROUTER
#define OPTION_PBR
#endif

/* AAA */
#define OPTION_AAA_ACCOUNTING
#define OPTION_AAA_AUTHORIZATION

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

/* IPv6 */
#define OPTION_IPV6

/* ------ Digistar 3G Product Models Declaration ------ */
/* RCG1000 */
#if defined(CONFIG_DIGISTAR_RCG1000)
#define OPTION_ETHERNET_WAN
#define OPTION_MODEM3G
#define OPTION_DUAL_SIM
#define OPTION_NUM_ETHERNET_IFACES	2
#define OPTION_NUM_3G_IFACES	3

/* RCG800 */
#elif defined(CONFIG_DIGISTAR_RCG800)
#define OPTION_MODEM3G
#define OPTION_DUAL_SIM
#define OPTION_NUM_ETHERNET_IFACES	1
#define OPTION_NUM_3G_IFACES	3

/* RCG700 */
#elif defined(CONFIG_DIGISTAR_RCG700)
#define OPTION_ETHERNET_WAN
#define OPTION_NUM_ETHERNET_IFACES	2

#else
#error "No 3G board defined!"
#endif

#define OPTION_PPP

#define OPTION_MANAGED_SWITCH
#define OPTION_SWITCH_BROADCOM
#define OPTION_SWITCH_PORT_NUM		4

#define GPIO_PORT_3G_RESET	0
#define GPIO_PIN_3G_RESET	12


/* End of CONFIG_DIGISTAR_3G */
#elif defined(CONFIG_DIGISTAR_EFM)

/* Código pras nomeclaturas ETL-ABCD onde 'A' é o nro de fios (8),
 * 'B' é nro de ETHs (1),
 * 'C' é o tipo de back up (0=nenhum, 1=USB e 2=3G) e
 * 'D' é o tipo de SW (0=bridge e 1=router)...*/

#define OPTION_EFM
#define OPTION_ETHERNET
#define OPTION_ETHERNET_LAN_INDEX 0
#define OPTION_NUM_ETHERNET_IFACES	1

#define OPTION_MANAGED_SWITCH
#define OPTION_SWITCH_MICREL
#define OPTION_SWITCH_MICREL_KSZ8863
#define OPTION_SWITCH_PORT_NUM		2

#undef OPTION_PPPOE
#undef OPTION_PPTP

/* IPv6 */
#define OPTION_IPV6

/* Factory Test System Digistar*/
#define OPTION_FTS_DIGISTAR

/* AAA */
#define OPTION_AAA_ACCOUNTING
#define OPTION_AAA_AUTHORIZATION

#define GPIO_PORT_3G_RESET	0
#define GPIO_PIN_3G_RESET	17

/* Bridge Options */
#if defined(CONFIG_DIGISTAR_BRIDGE)
#define CFG_PRODUCT	"EFMBridge"
#define OPTION_NUM_EFM_CHANNELS	4
#undef OPTION_IP_ROUTING
#undef OPTION_IPSEC
#undef OPTION_FIREWALL
#undef OPTION_NAT
#undef OPTION_QOS
#undef OPTION_ROUTER
#undef OPTION_IPV6
#undef OPTION_SMCROUTE
#undef OPTION_TUNNEL

/* Router Options */
#elif defined(CONFIG_DIGISTAR_ROUTER)
#define CFG_PRODUCT	"EFMRouter"
#define OPTION_NUM_EFM_CHANNELS	4
#define OPTION_IP_ROUTING
#define OPTION_IPSEC
#define N_IPSEC_IF 5
#define OPTION_FIREWALL
#define OPTION_NAT
#define OPTION_QOS
#define OPTION_ROUTER
#define OPTION_IPV6
#define OPTION_SMCROUTE
#define OPTION_TUNNEL
#define OPTION_BGP

#define OPTION_MODEM3G
#define OPTION_PPP
#define OPTION_NUM_3G_IFACES	1

#else
#error "No EFM board defined!"
#endif
/* End of CONFIG_DIGISTAR_EFM */
#elif defined(CONFIG_DIGISTAR_EFM4ETH)

/* Código pras nomeclaturas ETL-ABCD onde 'A' é o nro de fios (8),
 * 'B' é nro de ETHs (1),
 * 'C' é o tipo de back up (0=nenhum, 1=USB e 2=3G) e
 * 'D' é o tipo de SW (0=bridge e 1=router)...*/

#define OPTION_EFM
#define OPTION_ETHERNET
#define OPTION_ETHERNET_LAN_INDEX 0
#define OPTION_NUM_ETHERNET_IFACES	1

#define OPTION_MANAGED_SWITCH
#define OPTION_SWITCH_MICREL
#define OPTION_SWITCH_MICREL_KSZ8895
#define OPTION_SWITCH_PORT_NUM		4

#undef OPTION_PPPOE
#undef OPTION_PPTP


/* Factory Test System Digistar*/
#define OPTION_FTS_DIGISTAR

/* AAA */
#define OPTION_AAA_ACCOUNTING
#define OPTION_AAA_AUTHORIZATION

#define GPIO_PORT_3G_RESET	0
#define GPIO_PIN_3G_RESET	31

/* Bridge Options */
#if defined(CONFIG_DIGISTAR_BRIDGE)
#define CFG_PRODUCT	"EFM4ETH"
#define OPTION_NUM_EFM_CHANNELS	4
#undef OPTION_IP_ROUTING
#undef OPTION_IPSEC
#undef OPTION_FIREWALL
#undef OPTION_NAT
#undef OPTION_QOS
#undef OPTION_ROUTER
#undef OPTION_IPV6
#undef OPTION_SMCROUTE
#undef OPTION_TUNNEL

/* Router Options */
#elif defined(CONFIG_DIGISTAR_ROUTER)
#define CFG_PRODUCT	"EFM4ETH"
#define OPTION_NUM_EFM_CHANNELS	4
#define OPTION_IP_ROUTING
#define OPTION_IPSEC
#define N_IPSEC_IF 5
#define OPTION_FIREWALL
#define OPTION_NAT
#define OPTION_QOS
#define OPTION_ROUTER
#define OPTION_IPV6
#define OPTION_SMCROUTE
#define OPTION_TUNNEL
#define OPTION_BGP

#define OPTION_MODEM3G
#define OPTION_PPP
#define OPTION_NUM_3G_IFACES	1

#else
#error "No EFM board defined!"
#endif
/* End of CONFIG_DIGISTAR_EFM4ETH */

#elif defined(CONFIG_DIGISTAR_MRG)

#define CFG_PRODUCT	"MRG"

#define OPTION_ETHERNET /* Enables ethernet interfaces */
#define OPTION_ETHERNET_LAN_INDEX 0
#define OPTION_GIGAETHERNET /* Enable gigabit options */
#define OPTION_PPPOE
#define OPTION_PPTP
#define OPTION_TUNNEL
#define OPTION_ROUTER /* ip forwarding, enable dynamic routing protocols, etc. */
#define OPTION_SMCROUTE /* static multicast routing */

/* Wireless LAN */
#define OPTION_WIFI

#ifdef OPTION_WIFI
#define OPTION_HOSTAP
#endif

/* Factory Test System Digistar*/
#define OPTION_FTS_DIGISTAR

/* PBR - Policy Based Routing */
/* DEPENDE DE OPTION_ROUTER*/
#ifdef OPTION_ROUTER
#define OPTION_PBR
#endif

/* AAA */
#define OPTION_AAA_ACCOUNTING
#define OPTION_AAA_AUTHORIZATION

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

/* IPv6 */
#define OPTION_IPV6

#define OPTION_ETHERNET_WAN
#define OPTION_MODEM3G
#define OPTION_NUM_ETHERNET_IFACES	2
#define OPTION_NUM_3G_IFACES	1

#define OPTION_PPP

#define OPTION_MANAGED_SWITCH
#define OPTION_SWITCH_BROADCOM
#define OPTION_SWITCH_PORT_NUM		4

#define GPIO_PORT_3G_RESET	1
#define GPIO_PIN_3G_RESET	3

/* End of CONFIG_DIGISTAR_MRG */
#else
#error "No such board!"
#endif
/*=====================================================*/

/*****************************************************************************/
/*****************************************************************************/
/* Maximum hostname size */
#define OPTION_HOSTNAME_MAXSIZE	30

#define OPTION_CLEAR_INTERFACE_COUNTERS

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
