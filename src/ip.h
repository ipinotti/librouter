#ifndef _IP_H
#define _IP_H
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include "defines.h"
#include "typedefs.h"
#include "shm.h"

typedef struct
{
	char ifname[IFNAMSIZ];
	int index;
	int mtu;
	unsigned char addr[MAX_HWADDR];
	unsigned char addr_size;
	struct net_device_stats stats;
	unsigned flags;
	unsigned short type;
} link_table_t;

int ipaddr_modify(int add_del, ip_addr_table_t *data);
int get_if_list(void);
void ip_bitlen2mask(int bitlen, char *mask);
typedef enum { DEL_ADDR=0, ADD_ADDR, DEL_SADDR, ADD_SADDR } ip_addr_oper_t;
int ip_addr_add_del(ip_addr_oper_t add_del, char *ifname, char *local_ip, char *remote_ip, char *netmask);
#define ip_addr_del(a,b,c,d) ip_addr_add_del(DEL_ADDR,a,b,c,d)
#define ip_addr_add(a,b,c,d) ip_addr_add_del(ADD_ADDR,a,b,c,d)
#define ip_addr_del_secondary(a,b,c,d) ip_addr_add_del(DEL_SADDR,a,b,c,d)
#define ip_addr_add_secondary(a,b,c,d) ip_addr_add_del(ADD_SADDR,a,b,c,d)
int ip_addr_flush(char *ifname);
int get_mac(char *ifname, char *mac);

const char *ciscomask(const char *mask);
const char *extract_mask(char *cidrblock);
int netmask2cidr(const char *netmask);

extern int link_table_index;
extern link_table_t link_table[MAX_NUM_LINKS];
extern int ip_addr_table_index;
extern ip_addr_table_t ip_addr_table[MAX_NUM_IPS];

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

#endif
