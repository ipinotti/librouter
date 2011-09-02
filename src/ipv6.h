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

//#define ipv6_DEBUG
#ifdef ipv6_DEBUG
#define ipv6_dbg(x,...) \
	syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define ipv6_dbg(x,...)
#endif

/* Addresses and Masks */
struct ipv6_t {
	char ipv6addr[46];
	char ipv6mask[4];
};

/* IPv6 Tables variables */
//#define IPT_BUF_SIZE 100
//struct iptables_t {
//	char in_acl[IPT_BUF_SIZE];
//	char out_acl[IPT_BUF_SIZE];
//	char in_mangle[IPT_BUF_SIZE];
//	char out_mangle[IPT_BUF_SIZE];
//	char in_nat[IPT_BUF_SIZE];
//	char out_nat[IPT_BUF_SIZE];
//};

struct interfacev6_conf {
	char *name;
	int is_subiface; /* Sub-interface */
//	struct iptables_t ipt;
	int mtu;
	int flags;
	int up;
	int running;
	struct net_device_stats *stats;
	unsigned short linktype;
	char mac[32];
	struct ipv6_t main_ip;
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

typedef enum {
	IPv6_DEL_ADDR = 0, IPv6_ADD_ADDR, IPv6_DEL_SADDR, IPv6_ADD_SADDR
} ipv6_addr_oper_t;

int librouter_ipv6_modify_addr(int add_del,
                  struct ipaddrv6_table *data,
                  struct intfv6_info *info);

int librouter_ipv6_get_if_list(struct intfv6_info *info);
void librouter_ipv6_bitlen2mask(int bitlen, char *mask);

int librouter_ipv6_addr_add_del(ipv6_addr_oper_t add_del,
                    char *ifname,
                    char *local_ip,
                    char *remote_ip,
                    char *netmask);

#define ipv6_addr_del(a,b,c,d) librouter_ipv6_addr_add_del(ipv6_DEL_ADDR,a,b,c,d)
#define ipv6_addr_add(a,b,c,d) librouter_ipv6_addr_add_del(ipv6_ADD_ADDR,a,b,c,d)
#define ipv6_addr_del_secondary(a,b,c,d) librouter_ipv6_addr_add_del(ipv6_DEL_SADDR,a,b,c,d)
#define ipv6_addr_add_secondary(a,b,c,d) librouter_ipv6_addr_add_del(ipv6_ADD_SADDR,a,b,c,d)

int librouter_ipv6_addr_flush(char *ifname);
int librouter_ipv6_get_mac(char *ifname, char *mac);

const char *librouter_ipv6_ciscomask(const char *mask);
const char *librouter_ipv6_extract_mask(char *cidrblock);
int librouter_ipv6_netmask2cidr(const char *netmask);
int librouter_ipv6_netmask2cidr_pbr(const char *netmask);


extern const char *masks[33];
extern const char *rmasks[33];

char *librouter_ipv6_ethernet_get_dev(char *dev);
void librouter_ipv6_ethernet_set_addr(char *ifname, char *addr, char *mask);
void librouter_ipv6_ethernet_set_addr_secondary(char *ifname, char *addr, char *mask);
void librouter_ipv6_ethernet_set_no_addr(char *ifname);
void librouter_ipv6_ethernet_set_no_addr_secondary(char *ifname, char *addr, char *mask);
int librouter_ipv6_interface_get_info(char *ifname, IPV6 *addr, IPV6 *mask, IPV6 *bc, IPV6 *peer);
void librouter_ipv6_interface_get_ipv6_addr(char *ifname, char *addr_str, char *mask_str);
void librouter_ipv6_ethernet_ipv6_addr(char *ifname, char *addr_str, char *mask_str);

void librouter_ipv6_interface_set_addr(char *ifname, char *addr, char *mask);
void librouter_ipv6_interface_set_addr_secondary(char *ifname, char *addr, char *mask);
void librouter_ipv6_interface_set_no_addr(char *ifname);
void librouter_ipv6_interface_set_no_addr_secondary(char *ifname, char *addr, char *mask);
unsigned int librouter_ipv6_is_valid_port(char *data);
unsigned int librouter_ipv6_is_valid_netmask(char *data);
int librouter_ipv6_iface_get_stats(char *ifname, void *store);

int librouter_ipv6_iface_get_config(char *interface, struct interfacev6_conf *conf, struct intfv6_info *info);
void librouter_ipv6_iface_free_config(struct interfacev6_conf *conf);

#endif /* IPV6_H_ */
