/*
 * dhcp.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef DHCP_H_
#define DHCP_H_

#define UDHCPD

enum {
	DHCP_NONE, DHCP_SERVER, DHCP_RELAY
};
enum {
	DHCP_INTF_ETHERNET_0, DHCP_INTF_ETHERNET_1, DHCP_INTF_LOOPBACK
};

#define DHCP_INTF_FILE "/var/run/l2tp_dhcp.interface"

#ifdef UDHCPD
#define DHCPD_DAEMON_LOCAL "/sbin/udhcpd /etc/udhcpd.loopback0.conf"
#define FILE_DHCPDCONF_LOCAL "/etc/udhcpd.loopback0.conf"
#define FILE_DHCPDLEASES_LOCAL "/etc/udhcpd.loopback0.leases"
#define FILE_DHCPDPID_LOCAL "/var/run/udhcpd.loopback0.pid"
#define DHCPD_DAEMON "/sbin/udhcpd /etc/udhcpd.ethernet%d.conf"
#define FILE_DHCPDCONF "/etc/udhcpd.ethernet%d.conf"
#define FILE_DHCPDLEASES "/etc/udhcpd.ethernet%d.leases"
#define FILE_DHCPD_PID_ETH "/var/run/udhcpd.ethernet%d.pid"
#define FILE_DHCPD_PID_STR "/var/run/udhcpc.%s.pid"
#else
#define DHCPD_DAEMON "dhcpd"
#define FILE_DHCPDCONF "/etc/dhcpd.conf"
#define FILE_DHCPDLEASES "/etc/dhcpd.leases"
#endif

#define DHCRELAY_DAEMON "dhcrelay"
#define DHCPC_DAEMON "udhcpc -i %s"

int libconfig_udhcpd_reload(int eth);
int libconfig_udhcpd_kick_by_eth(int eth);
int libconfig_udhcpd_kick_by_name(char *iface);
int libconfig_dhcpd_set_status(int on_off, int eth);
int libconfig_dhcp_get_status(void);
int libconfig_dhcp_set_none(void);
int libconfig_dhcp_set_no_server(void);
int libconfig_dhcp_set_no_relay(void);
int libconfig_dhcp_set_server(int save_dns, char *cmdline);
int libconfig_dhcp_get_server(char *buf);
int libconfig_dhcp_check_server(char *ifname);
int libconfig_dhcp_set_relay(char *servers);
int libconfig_dhcp_get_relay(char *buf);
int libconfig_dhcp_l2tp_get_interface(void);
int libconfig_dhcpd_set_local(int on_off);
int libconfig_dhcp_get_local(void);
int libconfig_dhcp_set_server_local(int save_dns, char *cmdline);
int libconfig_dhcp_get_server_local(char *buf);

#endif /* DHCP_H_ */
