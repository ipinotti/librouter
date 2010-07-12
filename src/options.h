/*
 * options.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef OPTIONS_H_
#define OPTIONS_H_

/*****************************************************************************/
/* Defines for Digistar Models ***********************************************/
/*****************************************************************************/
#define OPTION_PPP

/* MODEM3G */
#define OPTION_MODEM3G
#define NUM_3G_INTFS	3

/* BGP */
#define OPTION_BGP

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
