/*
 * ipv6.h
 *
 *  Created on: Aug 29, 2011
 *      Author: ipinotti
 */

#ifndef IPV6_H_
#define IPV6_H_


#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>

#include "defines.h"
#include "typedefs.h"

typedef struct in6_addr IPV6;

//#define IPV6_DEBUG

#ifdef IPV6_DEBUG
#define ipv6_dbg(x,...) \
	printf("%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define ipv6_dbg(x,...)
#endif

#ifdef CONFIG_DIGISTAR_MRG
#define IP_BIN "/bin/ip"
#else
#define IP_BIN "/sbin/ip"
#endif

/* Addresses and Masks */
struct ipv6_t {
	char ipv6addr[46];
	char ipv6mask[4];
};

#ifdef NOT_YET_IMPLEMENTED
/* IPv6 Tables variables */
#define IPT_BUF_SIZE 100
struct iptables_t {
	char in_acl[IPT_BUF_SIZE];
	char out_acl[IPT_BUF_SIZE];
	char in_mangle[IPT_BUF_SIZE];
	char out_mangle[IPT_BUF_SIZE];
	char in_nat[IPT_BUF_SIZE];
	char out_nat[IPT_BUF_SIZE];
};
#endif

struct interfacev6_conf {
	char *name;
	int is_subiface; /* Sub-interface */
#ifdef NOT_YET_IMPLEMENTED
	struct iptables_t ipt;
#endif
	int mtu;
	int flags;
	int up;
	int running;
	struct net_device_stats *stats;
	unsigned short linktype;
	char mac[32];
	struct ipv6_t main_ip[MAX_NUM_IPS];
	struct ipv6_t sec_ip[MAX_NUM_IPS];
	int dhcpcV6; /* DHCPv6 Client running ? */
	struct intfv6_info *info;
};

struct ipaddrv6_table {
	char ifname[IFNAMSIZ];
	struct in6_addr local;
	struct in6_addr remote;
	struct in6_addr multicast;
	int bitlen;

};

struct linkv6_table {
	char ifname[IFNAMSIZ];
	int index;
	int mtu;
	unsigned char addr[MAX_HWADDR];
	unsigned char addr_size;
	struct net_device_stats stats;
	unsigned flags;
	unsigned short type;
};

struct intfv6_info {
	struct linkv6_table link[MAX_NUM_LINKS];
	struct ipaddrv6_table ipaddr[MAX_NUM_IPS];
};

struct nlmsg_v6_list {
	struct nlmsg_v6_list *next;
	struct nlmsghdr h;
};

/* New implementation*/
int librouter_ipv6_get_if_list (struct intfv6_info *info);
int librouter_ipv6_addr_add (char *ifname, char *ipv6_addr, char *netmask_v6);
int librouter_ipv6_addr_del (char *ifname, char *ipv6_addr, char *netmask_v6);
int librouter_ipv6_addr_flush (char *ifname);
int librouter_ipv6_ethernet_set_addr (char *ifname, char *addr, char *mask, char *feature);
int librouter_ipv6_ethernet_set_no_addr (char *ifname, char *addr, char *mask);
int librouter_ipv6_ethernet_set_no_addr_flush (char *ifname);
int librouter_ipv6_interface_set_addr (char *ifname, char *addr, char *mask, char *feature);
int librouter_ipv6_interface_set_no_addr (char *ifname, char *addr, char *mask);
int librouter_ipv6_interface_set_no_addr_flush (char *ifname);
int librouter_ipv6_is_addr_link_local (char *addr);
int librouter_ipv6_mod_eui64 (char *dev, char *addr_mask_str);
int librouter_ipv6_get_eui64_addr(char *addr_mask_str, char *mac_addr);
int librouter_ipv6_check_addr_string_for_ipv6(char *addr_str);
int librouter_ipv6_conv_ipv4_addr_in_6to4_ipv6_addr(char *addr_str);
int librouter_ipv6_conv_6to4_ipv6_addr_in_ipv4_addr(char *addr_str);


int librouter_ipv6_get_mac(char *ifname, char *mac, int dot_mode);
char *librouter_ipv6_ethernet_get_dev(char *dev);
int librouter_ipv6_interface_get_info(char *ifname, IPV6 *addr, IPV6 *mask, IPV6 *bc, IPV6 *peer);
void librouter_ipv6_interface_get_ipv6_addr(char *ifname, char *addr_str, char *mask_str);
int librouter_ipv6_iface_get_stats(char *ifname, void *store);
int librouter_ipv6_iface_get_config(char *interface, struct interfacev6_conf *conf, struct intfv6_info *info);
void librouter_ipv6_iface_free_config(struct interfacev6_conf *conf);



/* Excluded for a moment (ip functions copied to ipv6 [legacy])*/
#if 0
void librouter_ipv6_bitlen2mask(int bitlen, char *mask);
void librouter_ipv6_ethernet_ipv6_addr(char *ifname, char *addr_str, char *mask_str);
void librouter_ipv6_interface_set_no_addr_secondary(char *ifname, char *addr, char *mask);
void librouter_ipv6_interface_set_addr_secondary(char *ifname, char *addr, char *mask);
unsigned int librouter_ipv6_is_valid_port(char *data);
unsigned int librouter_ipv6_is_valid_netmask(char *data);
void librouter_ipv6_ethernet_set_no_addr_secondary(char *ifname, char *addr, char *mask);
void librouter_ipv6_ethernet_set_addr_secondary(char *ifname, char *addr, char *mask);
const char *librouter_ipv6_ciscomask(const char *mask);
const char *librouter_ipv6_extract_mask(char *cidrblock);
int librouter_ipv6_modify_addr(int add_del, struct ipaddrv6_table *data, struct intfv6_info *info);
#endif



#endif /* IPV6_H_ */
