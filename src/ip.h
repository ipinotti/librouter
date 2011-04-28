/*
 * ip.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef IP_H_
#define IP_H_
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

typedef struct in_addr IP;

//#define IP_DEBUG
#ifdef IP_DEBUG
#define ip_dbg(x,...) \
	syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define ip_dbg(x,...)
#endif

/* Addresses and Masks */
struct ip_t {
	char ipaddr[16];
	char ipmask[16];
	char ippeer[16];
};

/* IP Tables variables */
#define IPT_BUF_SIZE 100
struct iptables_t {
	char in_acl[IPT_BUF_SIZE];
	char out_acl[IPT_BUF_SIZE];
	char in_mangle[IPT_BUF_SIZE];
	char out_mangle[IPT_BUF_SIZE];
	char in_nat[IPT_BUF_SIZE];
	char out_nat[IPT_BUF_SIZE];
};

struct interface_conf {
	char *name;
	int is_subiface; /* Sub-interface */
	struct iptables_t ipt;
	int mtu;
	int flags;
	int up;
	int running;
	struct net_device_stats *stats;
	unsigned short linktype;
	int txqueue;
	char mac[32];
	struct ip_t main_ip;
	struct ip_t sec_ip[MAX_NUM_IPS];
	int dhcpc; /* DHCP Client running ? */
	struct intf_info *info;
};

struct ipaddr_table {
	char ifname[IFNAMSIZ];
	struct in_addr local;
	struct in_addr remote;
	struct in_addr broadcast;
	int bitlen;
};

struct link_table {
	char ifname[IFNAMSIZ];
	int index;
	int mtu;
	unsigned char addr[MAX_HWADDR];
	unsigned char addr_size;
	struct net_device_stats stats;
	unsigned flags;
	unsigned short type;
};

struct intf_info {
	struct link_table link[MAX_NUM_LINKS];
	struct ipaddr_table ipaddr[MAX_NUM_IPS];
};

struct nlmsg_list {
	struct nlmsg_list *next;
	struct nlmsghdr h;
};

typedef enum {
	IP_DEL_ADDR = 0, IP_ADD_ADDR, IP_DEL_SADDR, IP_ADD_SADDR
} ip_addr_oper_t;

int librouter_ip_modify_addr(int add_del,
                  struct ipaddr_table *data,
                  struct intf_info *info);

int librouter_ip_get_if_list(struct intf_info *info);
void librouter_ip_bitlen2mask(int bitlen, char *mask);

int librouter_ip_addr_add_del(ip_addr_oper_t add_del,
                    char *ifname,
                    char *local_ip,
                    char *remote_ip,
                    char *netmask);

#define ip_addr_del(a,b,c,d) librouter_ip_addr_add_del(IP_DEL_ADDR,a,b,c,d)
#define ip_addr_add(a,b,c,d) librouter_ip_addr_add_del(IP_ADD_ADDR,a,b,c,d)
#define ip_addr_del_secondary(a,b,c,d) librouter_ip_addr_add_del(IP_DEL_SADDR,a,b,c,d)
#define ip_addr_add_secondary(a,b,c,d) librouter_ip_addr_add_del(IP_ADD_SADDR,a,b,c,d)

int librouter_ip_addr_flush(char *ifname);
int librouter_ip_get_mac(char *ifname, char *mac);

const char *librouter_ip_ciscomask(const char *mask);
const char *librouter_ip_extract_mask(char *cidrblock);
int librouter_ip_netmask2cidr(const char *netmask);

extern const char *masks[33];
extern const char *rmasks[33];

char *librouter_ip_ethernet_get_dev(char *dev);
void librouter_ip_ethernet_set_addr(char *ifname, char *addr, char *mask);
void librouter_ip_ethernet_set_addr_secondary(char *ifname, char *addr, char *mask);
void librouter_ip_ethernet_set_no_addr(char *ifname);
void librouter_ip_ethernet_set_no_addr_secondary(char *ifname, char *addr, char *mask);
int librouter_ip_interface_get_info(char *ifname, IP *addr, IP *mask, IP *bc, IP *peer);
void librouter_ip_interface_get_ip_addr(char *ifname, char *addr_str, char *mask_str);
void librouter_ip_ethernet_ip_addr(char *ifname, char *addr_str, char *mask_str);

void librouter_ip_interface_set_addr(char *ifname, char *addr, char *mask);
void librouter_ip_interface_set_addr_secondary(char *ifname, char *addr, char *mask);
void librouter_ip_interface_set_no_addr(char *ifname);
void librouter_ip_interface_set_no_addr_secondary(char *ifname, char *addr, char *mask);
unsigned int librouter_ip_is_valid_port(char *data);
unsigned int librouter_ip_is_valid_netmask(char *data);
int librouter_ip_iface_get_stats(char *ifname, void *store);

int librouter_ip_iface_get_config(char *interface, struct interface_conf *conf, struct intf_info *info);
void librouter_ip_iface_free_config(struct interface_conf *conf);

#ifdef OPTION_SHM_IP_TABLE
int librouter_ip_create_shm (int flags);
int librouter_ip_detach_shm (void);
int librouter_ip_modify_shm (int add_del, struct ipaddr_table *data);
#endif

#endif /* IP_H_ */
