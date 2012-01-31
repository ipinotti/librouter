/*
 * dhcp.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef DHCP_H_
#define DHCP_H_

#include <stdint.h>

//#define DHCP_DEBUG
#ifdef DHCP_DEBUG
#define dhcp_dbg(x,...) \
	syslog(LOG_INFO, "%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define dhcp_dbg(x,...)
#endif

#define DHCP_MAX_NUM_LEASES	128

/* Taken from udhcpd code */
struct dyn_lease {
	/* "nip": IP in network order */
	/* Unix time when lease expires. Kept in memory in host order.
	 * When written to file, converted to network order
	 * and adjusted (current time subtracted) */
	uint32_t expires;
	uint32_t lease_nip;
	/* We use lease_mac[6], since e.g. ARP probing uses
	 * only 6 first bytes anyway. We check received dhcp packets
	 * that their hlen == 6 and thus chaddr has only 6 significant bytes
	 * (dhcp packet has chaddr[16], not [6])
	 */
	uint8_t lease_mac[6];
	char hostname[20];
	uint8_t pad[2];
	/* total size is a multiply of 4 */
} PACKED;

struct dhcp_server_conf_t {
	char *pool_start;
	char *pool_end;
	char *default_router;
	char *domain;
	char *dnsserver;
	int default_lease_time;
	int max_lease_time;
	int enable;
};

struct dhcp_lease_t {
	char *mac;
	char *ipaddr;
	char *lease_time;
};

#define UDHCPD

enum {
	DHCP_NONE, DHCP_SERVER, DHCP_RELAY
};
enum {
	DHCP_INTF_ETHERNET_0, DHCP_INTF_ETHERNET_1, DHCP_INTF_LOOPBACK
};

#define DHCP_INTF_FILE "/var/run/l2tp_dhcp.interface"

#ifdef UDHCPD
#define DHCPD_DAEMON_LOCAL "/sbin/udhcpd -f -S /etc/udhcpd.lo0.conf"
#define FILE_DHCPDCONF_LOCAL "/etc/udhcpd.lo0.conf"
#define FILE_DHCPDLEASES_LOCAL "/etc/udhcpd.lo0.leases"
#define FILE_DHCPDPID_LOCAL "/var/run/udhcpd.lo0.pid"

#define DHCPD_DAEMON "/sbin/udhcpd -f -S /etc/udhcpd.conf"
#define FILE_DHCPDCONF "/etc/udhcpd.conf"
#define FILE_DHCPDLEASES "/etc/udhcpd.leases"
#define FILE_DHCPD_PID_ETH "/var/run/udhcpd.pid"
#define FILE_DHCPD_PID_STR "/var/run/udhcpc.pid"

#else
#define DHCPD_DAEMON "dhcpd"
#define FILE_DHCPDCONF "/etc/dhcpd.conf"
#define FILE_DHCPDLEASES "/etc/dhcpd.leases"
#endif

#define DHCRELAY_DAEMON "dhcrelay"
#define DHCPC_DAEMON "udhcpc -f -i %s"

#define DHCPD_CONFIG_FILE "/etc/dhcpd.conf"

/* Value related to interface for DHCP_SERVER_DEFAULT */
#define INTF_DHCP_SERVER_DEFAULT OPTION_ETHERNET_LAN_INDEX

int librouter_udhcpd_reload(int eth);
int librouter_udhcpd_kick_by_eth(int eth);
int librouter_udhcpd_kick_by_name(char *iface);
int librouter_dhcpd_set_status(int on_off, int eth);
int librouter_dhcp_get_status(void);
int librouter_dhcp_set_none(void);
int librouter_dhcp_set_no_server(void);
int librouter_dhcp_set_no_relay(void);

int librouter_dhcp_get_server(char *buf);
int librouter_dhcp_server_set_dnsserver(char *dns);
int librouter_dhcp_server_set_leasetime(int time);
int librouter_dhcp_server_set_maxleasetime(int time);
int librouter_dhcp_server_set_iface(char *iface);
int librouter_dhcp_server_get_iface(char **iface);
int librouter_dhcp_server_set_domain(char *domain);
int librouter_dhcp_server_get_domain(char **domain);
int librouter_dhcp_server_set_nbns(char *ns);
int librouter_dhcp_server_set_nbdd(char *dd);
int librouter_dhcp_server_set_nbnt(int nt);
int librouter_dhcp_server_set_router(char *router);
int librouter_dhcp_server_set_network(char *network, char *mask);
int librouter_dhcp_server_set_pool(char *start, char *end);
int librouter_dhcp_server_set(int enable);
int librouter_dhcp_server_get_config(struct dhcp_server_conf_t *dhcp);
int librouter_dhcp_server_set_config(struct dhcp_server_conf_t *dhcp);
int librouter_dhcp_server_free_config(struct dhcp_server_conf_t *dhcp);
int librouter_dhcp_server_get_leases(struct dhcp_lease_t *leases);
int librouter_dhcp_server_free_leases(struct dhcp_lease_t *leases);

int librouter_dhcp_check_server(char *ifname);
int librouter_dhcp_set_relay(char *servers);
int librouter_dhcp_get_relay(char *buf);
int librouter_dhcp_l2tp_get_interface(void);
int librouter_dhcpd_set_local(int on_off);
int librouter_dhcp_get_local(void);
int librouter_dhcp_set_server_local(int save_dns, char *cmdline);
int librouter_dhcp_get_server_local(char *buf);

#endif /* DHCP_H_ */
