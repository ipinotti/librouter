#ifndef _DHCP_H
#define _DHCP_H

#define UDHCPD

enum { DHCP_NONE, DHCP_SERVER, DHCP_RELAY };
enum { DHCP_INTF_ETHERNET_0, DHCP_INTF_ETHERNET_1, DHCP_INTF_LOOPBACK };
#define DHCP_INTF_FILE "/var/run/l2tp_dhcp.interface"

#ifdef UDHCPD
#define DHCPD_DAEMON_LOCAL "/sbin/udhcpd /etc/udhcpd.loopback0.conf"
#define FILE_DHCPDCONF_LOCAL "/etc/udhcpd.loopback0.conf"
#define FILE_DHCPDLEASES_LOCAL "/etc/udhcpd.loopback0.leases"
#define FILE_DHCPDPID_LOCAL "/var/run/udhcpd.loopback0.pid"

#define DHCPD_DAEMON "/sbin/udhcpd /etc/udhcpd.ethernet%d.conf"
#define FILE_DHCPDCONF "/etc/udhcpd.ethernet%d.conf"
#define FILE_DHCPDLEASES "/etc/udhcpd.ethernet%d.leases"
#define FILE_DHCPDPID "/var/run/udhcpd.ethernet%d.pid"
#define FILE_DHCPCPID "/var/run/udhcpc.%s.pid"
#else
#define DHCPD_DAEMON "dhcpd"
#define FILE_DHCPDCONF "/etc/dhcpd.conf"
#define FILE_DHCPDLEASES "/etc/dhcpd.leases"
#endif
#define DHCRELAY_DAEMON "dhcrelay"
#define DHCPC_DAEMON "udhcpc -i %s"

int reload_udhcpd(int eth);
int kick_udhcpd(int eth);
int kick_udhcpc(char *iface);
int set_dhcpd(int on_off, int eth);
int get_dhcp(void);
int set_dhcp_none(void);
int set_no_dhcp_server(void);
int set_no_dhcp_relay(void);
int set_dhcp_server(int save_dns, char *cmdline);
int get_dhcp_server(char *buf);
int check_dhcp_server(char *ifname);
int set_dhcp_relay(char *servers);
int get_dhcp_relay(char *buf);
int l2tp_get_dhcp_interface(void);
int set_dhcpd_local(int on_off);
int get_dhcp_local(void);
int set_dhcp_server_local(int save_dns, char *cmdline);
int get_dhcp_server_local(char *buf);

#endif
