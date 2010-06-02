#ifndef _IP_H
#define _IP_H
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>

#include "defines.h"
#include "typedefs.h"

//#define DEBUG
#ifdef DEBUG
#define ip_dbg(x,...) \
	printf("%s : %d => ", __FUNCTION__, __LINE__); \
	printf(x,##__VA_ARGS__)
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
	enum dump_type {
		DUMP_INTF_CONFIG,
		DUMP_INTF_STATUS
	} type;
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
	DEL_ADDR = 0, ADD_ADDR, DEL_SADDR, ADD_SADDR
} ip_addr_oper_t;

int ipaddr_modify (int add_del,
                   struct ipaddr_table *data,
                   struct intf_info *info);

int get_if_list (struct intf_info *info);
void ip_bitlen2mask (int bitlen, char *mask);

int ip_addr_add_del (ip_addr_oper_t add_del,
                     char *ifname,
                     char *local_ip,
                     char *remote_ip,
                     char *netmask);

#define ip_addr_del(a,b,c,d) ip_addr_add_del(DEL_ADDR,a,b,c,d)
#define ip_addr_add(a,b,c,d) ip_addr_add_del(ADD_ADDR,a,b,c,d)
#define ip_addr_del_secondary(a,b,c,d) ip_addr_add_del(DEL_SADDR,a,b,c,d)
#define ip_addr_add_secondary(a,b,c,d) ip_addr_add_del(ADD_SADDR,a,b,c,d)

int ip_addr_flush (char *ifname);
int get_mac (char *ifname, char *mac);

const char *ciscomask (const char *mask);
const char *extract_mask (char *cidrblock);
int netmask2cidr (const char *netmask);

extern const char *masks[33];
extern const char *rmasks[33];

char *get_ethernet_dev(char *dev);
void set_ethernet_ip_addr(char *ifname, char *addr, char *mask);
void set_ethernet_ip_addr_secondary(char *ifname, char *addr, char *mask);
void set_ethernet_no_ip_addr(char *ifname);
void set_ethernet_no_ip_addr_secondary(char *ifname, char *addr, char *mask);
int get_interface_address(char *ifname, IP *addr, IP *mask, IP *bc, IP *peer);
void get_interface_ip_addr(char *ifname, char *addr_str, char *mask_str);
void get_ethernet_ip_addr(char *ifname, char *addr_str, char *mask_str);

void set_interface_ip_addr(char *ifname, char *addr, char *mask);
void set_interface_ip_addr_secondary(char *ifname, char *addr, char *mask);
void set_interface_no_ip_addr(char *ifname);
void set_interface_no_ip_addr_secondary(char *ifname, char *addr, char *mask);
unsigned int is_valid_port(char *data);
unsigned int is_valid_netmask(char *data);
int get_iface_stats(char *ifname, void *store);

int lconfig_get_iface_config(char *interface, struct interface_conf *conf);

#ifdef OPTION_SHM_IP_TABLE
int create_ipaddr_shm (int flags);
int detach_ipaddr_shm (void);
int ipaddr_modify_shm (int add_del, struct ipaddr_table *data);
#endif

#endif
