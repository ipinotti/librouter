/*
 * ip.c
 *
 *  Created on: Jun 24, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/sockios.h>
#include <fnmatch.h>
#include <linux/netlink.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>

#include "libnetlink/libnetlink.h"
#include "libnetlink/ll_map.h"

#include "options.h"

#include "typedefs.h"
#include "defines.h"
#include "args.h"
#include "dhcp.h"
#include "ip.h"
#include "dev.h"
#include "error.h"
#include "ppcio.h"
#include "pptp.h"

#ifdef OPTION_SHM_IP_TABLE
int ip_addr_table_backup_shmid = 0; /* ip address backup base on shmid */
struct ipaddr_table *ip_addr_table_backup = NULL; /* ip address backup base on shm */

int librouter_ip_create_shm(int flags) /* SHM_RDONLY */
{
	if (ip_addr_table_backup_shmid == 0) {
		if ((ip_addr_table_backup_shmid = shmget(
								(key_t) IPADDR_SHM_KEY, sizeof(struct ipaddr)
								* MAX_NUM_IPS, IPC_CREAT
								| IPC_EXCL | 0600)) == -1) {
			if ((ip_addr_table_backup_shmid = shmget(
									(key_t) IPADDR_SHM_KEY,
									sizeof(struct ipaddr) * MAX_NUM_IPS, 0))
					== -1)
			return -1;
			else if ((ip_addr_table_backup = shmat(
									ip_addr_table_backup_shmid, 0, flags))
					== NULL)
			return -1;
		} else {
			if ((ip_addr_table_backup = shmat(
									ip_addr_table_backup_shmid, 0, 0))
					== NULL)
			return -1;
			memset(ip_addr_table_backup, 0, sizeof(struct ipaddr)
					* MAX_NUM_IPS); /* initial setting! */
		}
	}
	return 0;
}

int librouter_ip_detach_shm(void)
{
	if (ip_addr_table_backup)
	return shmdt(ip_addr_table_backup);
	return 0;
}

int librouter_ip_modify_shm(int add_del, struct ipaddr_table *data)
{
	int i;

	if (ip_addr_table_backup == NULL) {
		syslog(LOG_ERR, "Addresses data not loaded!");
		return -1;
	}
	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (ip_addr_table_backup[i].ifname[0] == 0)
		break; /* free slot */
		if (memcmp(&ip_addr_table_backup[i], data,
						sizeof(struct ipaddr)) == 0)
		break; /* same data! */
	}
	if (i == MAX_NUM_IPS)
	return -1;
	if (add_del)
	ip_addr_table_backup[i] = *data;
	else {
		if (i < (MAX_NUM_IPS - 1)) /* shift over! */
		memmove(&ip_addr_table_backup[i],
				&ip_addr_table_backup[i + 1],
				sizeof(struct ipaddr) * (MAX_NUM_IPS
						- i - 1));
		memset(&ip_addr_table_backup[MAX_NUM_IPS - 1], 0,
				sizeof(struct ipaddr));
	}
	return 0;
}
#endif

static int _link_name_to_index(char *ifname, struct link_table *link)
{
	int i;

	for (i = 0; i < MAX_NUM_LINKS; i++) {
		if (strcmp(link[i].ifname, ifname) == 0)
			return link[i].index;
	}

	return -1;
}

#if 0
static unsigned int _link_name_to_flags(char *ifname)
{
	int i;

	for(i=0; i < link_table_index; i++) {
		if (strcmp(link_table[i].ifname, ifname) == 0)
		return link_table[i].flags;
	}
	return 0;
}
#endif

/* add/del ip address with netlink */
int librouter_ip_modify_addr(int add_del,
                  struct ipaddr_table *data,
                  struct intf_info *info)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr n;
		struct ifaddrmsg ifa;
		char buf[256];
	} req;
	int cmd;
	char *p, dev[16];

#if 0 /* !!! */
	fprintf(stderr, "librouter_ip_modify_addr %d %s\n", add_del, data->ifname);
#endif

	/* Add or Remove */
	cmd = add_del ? RTM_NEWADDR : RTM_DELADDR;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = cmd;
	req.ifa.ifa_family = AF_INET; // AF_IPX
	req.ifa.ifa_prefixlen = data->bitlen;

	/* local */
	addattr_l(&req.n, sizeof(req), IFA_LOCAL, &data->local.s_addr, 4);

	/* peer */
	addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &data->remote.s_addr, 4);

	/* broadcast */
	addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &data->broadcast.s_addr, 4);

	/* label */
	addattr_l(&req.n, sizeof(req), IFA_LABEL, data->ifname, strlen(data->ifname) + 1);

	if (cmd != RTM_DELADDR) {
		if (*((unsigned char *) (&data->local.s_addr)) == 127)
			req.ifa.ifa_scope = RT_SCOPE_HOST;
		else
			req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
	}

	if (rtnl_open(&rth, 0) < 0)
		return -1;

	/* Identifica que eh um alias e retira os ':' */
	strncpy(dev, data->ifname, 16);
	if ((p = strchr(dev, ':')) != NULL) {
		*p = 0;
	}

	if ((req.ifa.ifa_index = _link_name_to_index(dev, &(info->link[0])))
	                == 0) {
		fprintf(stderr, "Cannot find device \"%s\"\n", dev);
		goto error;
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		goto error;

	rtnl_close(&rth);
	return 0;

	error: rtnl_close(&rth);
	return -1;
}

static int _get_addrinfo(struct nlmsghdr *n, struct ipaddr_table *ipaddr)
{
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *rta_tb[IFA_MAX + 1];

	if (n->nlmsg_type != RTM_NEWADDR && n->nlmsg_type != RTM_DELADDR)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifa));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	bzero(ipaddr, sizeof(struct ipaddr_table));

	memset(rta_tb, 0, sizeof(rta_tb));
	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA (ifa), n->nlmsg_len
	                - NLMSG_LENGTH(sizeof(*ifa)));

	if (!rta_tb[IFA_LOCAL])
		rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];

	if (!rta_tb[IFA_ADDRESS])
		rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];

	if (ifa->ifa_family != AF_INET)
		return 0;

	ipaddr->bitlen = ifa->ifa_prefixlen;

	if (rta_tb[IFA_LOCAL]) {
		memcpy(&ipaddr->local, RTA_DATA (rta_tb[IFA_LOCAL]), 4);

		if (rta_tb[IFA_ADDRESS]) {
			memcpy(&ipaddr->remote, RTA_DATA (rta_tb[IFA_ADDRESS]),
			                4);
		}
	}

	if (rta_tb[IFA_BROADCAST]) {
		memcpy(&ipaddr->broadcast, RTA_DATA (rta_tb[IFA_BROADCAST]), 4);
	}

	strncpy(ipaddr->ifname, (char*) RTA_DATA (rta_tb[IFA_LABEL]), IFNAMSIZ);

	return 0;
}

int librouter_ip_store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct nlmsg_list **linfo = (struct nlmsg_list**) arg;
	struct nlmsg_list *h;
	struct nlmsg_list **lp;

	h = malloc(n->nlmsg_len + sizeof(void*));
	if (h == NULL)
		return -1;

	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	for (lp = linfo; *lp; lp = &(*lp)->next)
		/* NOTHING */;
	*lp = h;

	ll_remember_index(who, n, NULL);
	return 0;
}

static int _get_linkinfo(struct nlmsghdr *n, struct link_table *link)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX + 1];
	int len = n->nlmsg_len;

	if (n->nlmsg_type != RTM_NEWLINK && n->nlmsg_type != RTM_DELLINK)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	bzero(link, sizeof(struct link_table));
	link->index = ifi->ifi_index;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA (ifi), len);
	if (tb[IFLA_IFNAME] == NULL) {
		fprintf(stderr, "BUG: nil ifname\n");
		return -1;
	}

	strncpy(link->ifname, tb[IFLA_IFNAME] ? (char*) RTA_DATA (
			tb[IFLA_IFNAME]) : "<nil>", IFNAMSIZ);

	if (tb[IFLA_MTU])
		link->mtu = *(int*) RTA_DATA (tb[IFLA_MTU]);

	if (tb[IFLA_STATS]) {
		memcpy(&link->stats, RTA_DATA (tb[IFLA_STATS]),
		                sizeof(struct net_device_stats));
	}

	if (tb[IFLA_ADDRESS]) {
		unsigned char size;
		size = RTA_PAYLOAD (tb[IFLA_ADDRESS]);
		link->addr_size = size;
		if (size >= MAX_HWADDR)
			size = MAX_HWADDR;
		memcpy(link->addr, RTA_DATA (tb[IFLA_ADDRESS]), size);
	}

	link->flags = ifi->ifi_flags;
	link->type = ifi->ifi_type;

	return 0;
}

static int _has_ppp_intf(int index, struct intf_info *info)
{
	char intf[8];
	int i;

	sprintf(intf, "ppp%d", index);

	for (i=0; info->link[i].ifname[0] != 0; i++) {
		if (!strcmp(info->link[i].ifname, intf))
			return 1; /* Found */
	}

	return 0; /* Not found */
}

static int _clear_ppp_link_value_ (int index, struct intf_info *info)
{
	char intf[8];
	int i;

	sprintf(intf, "ppp%d", index);

	for (i=0; info->link[i].ifname[0] != 0; i++) {
		if (!strcmp(info->link[i].ifname, intf)){
			if (librouter_dev_get_link_running(intf) != IFF_RUNNING){
				info->link[i].flags = 0;
				return 1;
			}
		}
	}

	return 0; /* Not found */
}

static int _clear_ppp_ipaddr_value_ (int index, struct intf_info *info)
{
	char intf[8];
	int i;

	sprintf(intf, "ppp%d", index);

	for (i=0; info->ipaddr[i].ifname[0] != 0; i++) {
		if (!strcmp(info->ipaddr[i].ifname, intf)){
			if (librouter_dev_get_link_running(intf) != IFF_RUNNING){
				memset(&info->ipaddr[i], 0, sizeof(struct ipaddr_table));
				return 1;
			}
		}
	}

	return 0; /* Not found */
}


int librouter_ip_get_if_list(struct intf_info *info)
{
	int link_index, ip_index, i;
	struct link_table *link = &info->link[0];
	struct ipaddr_table *ipaddr = &info->ipaddr[0];
	struct rtnl_handle rth;
	struct nlmsg_list *linfo = NULL;
	struct nlmsg_list *ainfo = NULL;
	struct nlmsg_list *l, *a, *tmp;

	if (rtnl_open(&rth, 0) < 0)
		return -1;

	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		goto error;
	}
	if (rtnl_dump_filter(&rth, librouter_ip_store_nlmsg, &linfo, NULL, NULL) < 0) {
		fprintf(stderr, "Link Dump terminated\n");
		goto error;
	}
	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETADDR) < 0) {
		perror("Cannot send dump request");
		goto error;
	}
	if (rtnl_dump_filter(&rth, librouter_ip_store_nlmsg, &ainfo, NULL, NULL) < 0) {
		fprintf(stderr, "Address Dump terminated\n");
		goto error;
	}

	link_index = 0;
	ip_index = 0;

	/*
	 * Parse link information stored in netlink "packets".
	 */
	for (l = linfo; l;) {
		/* Update link pointer if __get_linkinfo succeeds */
		if (!_get_linkinfo(&l->h, link))
			link++;
		tmp = l;
		l = l->next;
		free(tmp);
	}

	for (a = ainfo; a;) {
		/* Update ipaddr pointer if __get_addrinfo succeeds */
		if (!_get_addrinfo(&a->h, ipaddr)) {
			ip_dbg("__getaddrinfo succeeded for %s\n", ipaddr->ifname);
			ip_dbg("ip address is %s\n", inet_ntoa(ipaddr->local));
			ip_dbg("bitmask is %d\n", ipaddr->bitlen);
			ipaddr++;
		}
		tmp = a;
		a = a->next;
		free(tmp);
	}


#ifdef OPTION_PPTP
	/*
	 * Search for PPTP interfaces. They may not exist in the kernel,
	 * since PPP interfaces are created dinamically.
	 * So we need to create some here.
	 * For now, just PPTP0 -> ppp20 is available by the define PPTP_PPP_START
	 */
	if (!_has_ppp_intf(PPTP_PPP_START, info)) {
		sprintf(link->ifname, "ppp%d", PPTP_PPP_START);
		link->type = ARPHRD_PPP;
		link++;
	}
	else {
		_clear_ppp_link_value_(PPTP_PPP_START,info);
		_clear_ppp_ipaddr_value_(PPTP_PPP_START,info);
	}
#endif	/* OPTION PPTP */


#ifdef OPTION_PPPOE
	/*
	 * Search for PPPOE interfaces. They may not exist in the kernel,
	 * since PPP interfaces are created dinamically.
	 * So we need to create some here.
	 * For now, just PPPOE0 -> ppp30 is available by the define PPPOE_PPP_START
	 */
	if (!_has_ppp_intf(PPPOE_PPP_START, info)) {
		sprintf(link->ifname, "ppp%d", PPPOE_PPP_START);
		link->type = ARPHRD_PPP;
		link++;
	}
	else {
		_clear_ppp_link_value_(PPPOE_PPP_START,info);
		_clear_ppp_ipaddr_value_(PPPOE_PPP_START,info);
	}
#endif	/* OPTION PPPOE */


#ifdef OPTION_MODEM3G
	/*
	 * Search for 3G Interfaces. They may not exist in the kernel,
	 * since PPP interfaces are created dinamically.
	 * So we need to create some here.
	 */
	for (i=0; i < OPTION_NUM_3G_IFACES; i++) {
		if (!_has_ppp_intf(i, info)) {
			sprintf(link->ifname, "ppp%d", i);
			link->type = ARPHRD_PPP;
			link++;
		}
		else{
			_clear_ppp_link_value_(i,info);
			_clear_ppp_ipaddr_value_(i,info);
		}
	}
#endif

	rtnl_close(&rth);
	return 0;

	error: rtnl_close(&rth);
	return -1;
}

void librouter_ip_bitlen2mask(int bitlen, char *mask)
{
	unsigned long bitmask;
	int i;

	for (i = 0, bitmask = 0; i < bitlen; i++) {
		bitmask >>= 1;
		bitmask |= (1 << 31);
	}
	sprintf(mask, "%d.%d.%d.%d", (int) ((bitmask >> 24) & 0xff),
	                (int) ((bitmask >> 16) & 0xff), (int) ((bitmask >> 8)
	                                & 0xff), (int) (bitmask & 0xff));
}

/* DEL_ADDR, ADD_ADDR, DEL_SADDR, ADD_SADDR */
int librouter_ip_addr_add_del(ip_addr_oper_t add_del,
                    char *ifname,
                    char *local_ip,
                    char *remote_ip,
                    char *netmask)
{
	int i, bitlen, ret = 0;
	IP mask, bitmask;
	struct ipaddr_table data, prim;
	struct intf_info info;

	memset(&info, 0, sizeof(struct intf_info));

	/* Get information into info structure */
	if ((ret = librouter_ip_get_if_list(&info)) < 0)
		return -1;

	strncpy(data.ifname, ifname, IFNAMSIZ);

	if (inet_aton(local_ip, &data.local) == 0)
		return -1;

	if ((remote_ip != NULL) && (remote_ip[0])) {
		if (inet_aton(remote_ip, &data.remote) == 0)
			return -1;
	} else {
		data.remote.s_addr = data.local.s_addr; /* remote as local! */
	}

	if (inet_aton(netmask, &mask) == 0)
		return -1;
	for (bitlen = 0, bitmask.s_addr = 0; bitlen < 32; bitlen++) {
		if (mask.s_addr == bitmask.s_addr)
			break;
		bitmask.s_addr |= htonl(1 << (31 - bitlen));
	}
	data.bitlen = bitlen; /* /8 /16 /24 */

	data.broadcast.s_addr = data.remote.s_addr;
	for (; bitlen < 32; bitlen++)
		data.broadcast.s_addr |= htonl(1 << (31 - bitlen));

	prim = data; /* for primary address search */
	if (add_del & 0x2) { /* secondary address */
		int primary;
		strcat(data.ifname, ":0"); /* alias label! */
		for (i = 0; info.ipaddr[i].ifname[0] != 0 && i < MAX_NUM_IPS; i++) {
			if (strcmp(prim.ifname, info.ipaddr[i].ifname) == 0) {
				primary = 1; /* primary address setted */
				break;
			}
		}
		if (primary) {
			for (i = 0; i < MAX_NUM_IPS; i++)
				if (memcmp(&info.ipaddr[i], &data,sizeof(struct ipaddr_table)) == 0 ||
					memcmp(&info.ipaddr[i], &prim, sizeof(struct ipaddr_table)) == 0)
					break; /* found same address entry */
		} else {
			data = prim; /* secondary as primary */
			i = MAX_NUM_IPS; /* not found test below */
		}
	} else { /* primary address */
		strcat(prim.ifname, ":0"); /* alias label! */
		for (i = 0; i < MAX_NUM_IPS; i++)
			if (memcmp(&info.ipaddr[i], &data, sizeof(struct ipaddr_table)) == 0)
				break; /* found same address entry */
	}

	if (i == MAX_NUM_IPS) {
		if (!(add_del & 0x1))
			return -1; /* no address to remove */
	} else {
		if (add_del & 0x1)
			return -1; /* address already configured! */
	}

	ret = librouter_ip_modify_addr((add_del & 0x1), &data, &info);

	return ret;
}

int librouter_ip_addr_flush(char *ifname)
{
	int i, ret;
	char alias[32];
	struct intf_info info;

	memset(&info, 0, sizeof(struct intf_info));

	strcpy(alias, ifname);
	strcat(alias, ":0");

	if ((ret = librouter_ip_get_if_list(&info)) < 0)
		return ret;

	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (strcmp(ifname, info.ipaddr[i].ifname) == 0 || strcmp(alias,
		                info.ipaddr[i].ifname) == 0) { /* with alias */
			/* delete primary also delete secondaries */
			ret = librouter_ip_modify_addr(0, &info.ipaddr[i], &info);
		}
	}

	return 0;
}

const char *masks[33] = { "0.0.0.0", "128.0.0.0", "192.0.0.0", "224.0.0.0",
                "240.0.0.0", "248.0.0.0", "252.0.0.0", "254.0.0.0",
                "255.0.0.0", "255.128.0.0", "255.192.0.0", "255.224.0.0",
                "255.240.0.0", "255.248.0.0", "255.252.0.0", "255.254.0.0",
                "255.255.0.0", "255.255.128.0", "255.255.192.0",
                "255.255.224.0", "255.255.240.0", "255.255.248.0",
                "255.255.252.0", "255.255.254.0", "255.255.255.0",
                "255.255.255.128", "255.255.255.192", "255.255.255.224",
                "255.255.255.240", "255.255.255.248", "255.255.255.252",
                "255.255.255.254", "255.255.255.255" };

const char *rmasks[33] = { "255.255.255.255", "127.255.255.255",
                "63.255.255.255", "31.255.255.255", "15.255.255.255",
                "7.255.255.255", "3.255.255.255", "1.255.255.255",
                "0.255.255.255", "0.127.255.255", "0.63.255.255",
                "0.31.255.255", "0.15.255.255", "0.7.255.255", "0.3.255.255",
                "0.1.255.255", "0.0.255.255", "0.0.127.255", "0.0.63.255",
                "0.0.31.255", "0.0.15.255", "0.0.7.255", "0.0.3.255",
                "0.0.1.255", "0.0.0.255", "0.0.0.127", "0.0.0.63", "0.0.0.31",
                "0.0.0.15", "0.0.0.7", "0.0.0.3", "0.0.0.1", "0.0.0.0" };

const char *librouter_ip_ciscomask(const char *mask)
{
	int i;

	for (i = 0; i < 33; i++) {
		if (strcmp(masks[i], mask) == 0)
			return rmasks[i];
	}
	return mask;
}

const char *librouter_ip_extract_mask(char *cidrblock)
{
	char *seperate;
	int bits;

	seperate = strchr(cidrblock, '/');
	if (!seperate) {
		return masks[32];
	}

	*seperate = 0;
	++seperate;

	bits = atoi(seperate);
	if ((bits < 33) && (bits >= 0))
		return masks[bits];
	return masks[32];
}

//TODO Analisar função, pode resultar em BUG devido ao rmasks
int librouter_ip_netmask2cidr(const char *netmask)
{
	int i;

	for (i = 0; i < 33; i++) {
		if (strcmp(masks[i], netmask) == 0)
			return i;
		if (strcmp(rmasks[i], netmask) == 0)
			return i;
	}
	return -1;
}

//TODO Função criada para evitar bug da função original, aplicado ao pbr (problema com mask 255.255.255.255 -- retornando 0) na func original
int librouter_ip_netmask2cidr_pbr(const char *netmask)
{
	int i;

	for (i = 0; i < 33; i++) {
		if (strcmp(masks[i], netmask) == 0)
			return i;
	}
	return -1;
}

int librouter_ip_get_mac(char *ifname, char *mac)
{
	return librouter_dev_get_hwaddr(ifname, (unsigned char *) mac);
}

char *librouter_ip_ethernet_get_dev(char *dev)
{
	int i;
	static char brname[32];

	/* Only valid for eth0 */
	if (strcmp(dev, "eth0"))
		return dev;

	for (i = 0; i < MAX_BRIDGE; i++) {
		snprintf(brname, 32, "%s%d", BRIDGE_NAME, i);
		/* Is ethernet interface enslaved by a bridge interface?  */
		if (librouter_br_checkif(brname, dev))
			return brname;
	}

	/* Not in a bridge, return interface itself */
	return dev;
}

void librouter_ip_ethernet_set_addr(char *ifname, char *addr, char *mask) /* main ethernet interface */
{
	char *dev = librouter_ip_ethernet_get_dev(ifname); /* bridge x ethernet */
	struct intf_info info;
	int i;

	memset(&info, 0, sizeof(struct intf_info));

	if (librouter_ip_get_if_list(&info) < 0)
		return;

	/* remove current address */
	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (strcmp(dev, info.ipaddr[i].ifname) == 0) {
			librouter_ip_modify_addr(0, &info.ipaddr[i], &info);
			break;
		}
	}

	ip_dbg("Adding IP address %s to %s\n", addr, dev);
	ip_addr_add (dev, addr, NULL, mask); /* new address */

#ifdef OPTION_BRIDGE
	librouter_br_update_ipaddr(ifname);
#endif
}

void librouter_ip_ethernet_set_addr_secondary(char *ifname, char *addr, char *mask)
{
	char *dev = librouter_ip_ethernet_get_dev(ifname); /* bridge x ethernet */

	ip_addr_add_secondary (dev, addr, NULL, mask);
}

void librouter_ip_ethernet_set_no_addr(char *ifname)
{
	char *dev = librouter_ip_ethernet_get_dev(ifname); /* bridge x ethernet */

	ip_dbg("Removing IP address from %s\n", dev);
	librouter_ip_addr_flush(dev); /* Clear all interface addresses */

#ifdef OPTION_BRIDGE
	librouter_br_update_ipaddr(ifname);
#endif
}

void librouter_ip_ethernet_set_no_addr_secondary(char *ifname, char *addr, char *mask)
{
	char *dev = librouter_ip_ethernet_get_dev(ifname); /* bridge x ethernet */

	ip_addr_del_secondary (dev, addr, NULL, mask);
}

int librouter_ip_interface_get_info(char *ifname, IP *addr, IP *mask, IP *bc, IP *peer)
{
	int i, ret;
	struct intf_info info;

	memset(&info, 0, sizeof(struct intf_info));

	/* Get information into info structure */
	if ((ret = librouter_ip_get_if_list(&info)) < 0)
		return ret;

	/* running addresses */
	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (strcmp(ifname, info.ipaddr[i].ifname) == 0)
			break;
	}

	if (i < MAX_NUM_IPS) {
		if (addr)
			*addr = info.ipaddr[i].local;
		if (peer)
			*peer = info.ipaddr[i].remote;
		if (bc)
			*bc = info.ipaddr[i].broadcast;
		if (mask) {
			if (info.ipaddr[i].bitlen)
				mask->s_addr = htonl(~((1 << (32
				                - info.ipaddr[i].bitlen)) - 1));
			else
				mask->s_addr = 0;
		}
	}

	return 0;
}

void librouter_ip_interface_get_ip_addr(char *ifname, char *addr_str, char *mask_str)
{
	IP addr, mask;

	addr.s_addr = 0;
	mask.s_addr = 0;

	librouter_ip_interface_get_info(ifname, &addr, &mask, 0, 0);
	strcpy(addr_str, inet_ntoa(addr));
	strcpy(mask_str, inet_ntoa(mask));
}

/* ppp unnumbered helper !!! ppp unnumbered x bridge hdlc */
void librouter_ip_ethernet_ip_addr(char *ifname, char *addr_str, char *mask_str)
{
	char *dev;

	dev = librouter_ip_ethernet_get_dev(ifname); /* bridge x ethernet */
	librouter_ip_interface_get_ip_addr(dev, addr_str, mask_str);
}

/* Interface generic */
void librouter_ip_interface_set_addr(char *ifname, char *addr, char *mask)
{
	int i;
	struct intf_info info;

	memset(&info, 0, sizeof(struct intf_info));

	/* Get information into info structure */
	if (librouter_ip_get_if_list(&info) < 0)
		return;

	/* remove current address */
	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (strcmp(ifname, info.ipaddr[i].ifname) == 0) {
			librouter_ip_modify_addr(0, &info.ipaddr[i], &info);
			break;
		}
	}

	ip_addr_add (ifname, addr, NULL, mask); /* new address */
}

void librouter_ip_interface_set_addr_secondary(char *ifname, char *addr, char *mask)
{
	ip_addr_add_secondary (ifname, addr, NULL, mask);
}

void librouter_ip_interface_set_no_addr(char *ifname)
{
	librouter_ip_addr_flush(ifname); /* Clear all interface addresses */
}

void librouter_ip_interface_set_no_addr_secondary(char *ifname, char *addr, char *mask)
{
	ip_addr_del_secondary (ifname, addr, NULL, mask);
}

int librouter_ip_check_addr_string_for_ipv4(char *addr_str)
{
	    struct sockaddr_in sa;
	    return (inet_pton(AF_INET, addr_str, &sa.sin_addr));
}

unsigned int librouter_ip_is_valid_port(char *data)
{
	char *p;

	if (!data)
		return 0;

	for (p = data; *p; p++) {
		if (isdigit(*p) == 0)
			return 0;
	}

	if (atoi(data) < 0)
		return 0;

	return 1;
}

unsigned int librouter_ip_is_valid_netmask(char *mask)
{
	arg_list argl = NULL;
	unsigned char elem;
	int i, k, n, ok, bit;
	char *p, *t, data[16];

	if (!mask)
		return 0;
	strncpy(data, mask, 15);
	data[15] = 0;
	for (p = data, n = 0; *p;) {
		if ((t = strchr(p, '.'))) {
			*t = ' ';
			n++;
			p = t + 1;
		} else
			break;
	}
	if (n != 3)
		return 0;
	if (!(n = librouter_parse_args_din(data, &argl)))
		return 0;
	if (n != 4) {
		librouter_destroy_args_din(&argl);
		return 0;
	}
	for (i = 0, ok = 1, bit = 1; i < n; i++) {
		elem = (unsigned char) atoi(argl[i]);
		if (elem != 0xFF) {
			if (bit) {
				for (k = 7; k >= 0; k--) {
					if (!((elem >> k) & 0x01)) {
						bit = 0;
						for (; k >= 0; k--) {
							if (((elem >> k) & 0x01)) {
								ok = 0;
								break;
							}
						}
					}
					if (!ok)
						break;
				}
			} else {
				if (elem != 0x00) {
					ok = 0;
					break;
				}
			}
		}
		if (!ok)
			break;
	}
	librouter_destroy_args_din(&argl);
	return ok;
}

/* Busca as estatisticas de uma interface */
int librouter_ip_iface_get_stats(char *ifname, void *store)
{
	int i;
	struct intf_info info;

	memset(&info, 0, sizeof(struct intf_info));

	if ((store == NULL) || (librouter_ip_get_if_list(&info) < 0))
		return -1;

	/* Find interface and copy its statistics
	 * to memory pointed by store */
	for (i = 0; i < MAX_NUM_LINKS; i++) {
		if (strcmp(info.link[i].ifname, ifname) == 0) {
			memcpy(store, &info.link[i].stats,
			                sizeof(struct net_device_stats));
			return 0;
		}
	}
	return -1;
}

int librouter_ip_iface_get_config(char *interface, struct interface_conf *conf,
                                  struct intf_info *info)
{
	char mac_bin[6];
	char name[16];
	int ret = -1;
	int i;
	char daemon_dhcpc[32];

	ip_dbg("Getting config for interface %s\n", interface);
	memset(conf, 0, sizeof(struct interface_conf));

	if (strstr(interface, ".") != NULL) {/* Is sub-interface? */
		conf->is_subiface = 1;
	}

	/* Get all information if it hasn't been passed to us */
	if (info == NULL) {
		struct intf_info tmp_info;

		memset(&tmp_info, 0, sizeof(struct intf_info));
		ret = librouter_ip_get_if_list(&tmp_info);

		if (ret < 0) {
			printf("%% ERROR : Could not get interfaces information\n");
			return ret;
		}
		info = &tmp_info;
	}

	/* Start searching for the desired interface,
	 * if not found return negative value */
	ret = -1;
	for (i = 0; i < MAX_NUM_LINKS; i++) {
		struct ipaddr_table *ipaddr;
		struct ip_t *main_ip;
		struct ip_t *sec_ip;
		int j;

		/* Test if it has a valid name */
		if (info->link[i].ifname[0] == 0)
			break;

		/* Check if this is the interface we are looking for */
		if (strcmp(info->link[i].ifname, interface))
			continue;

		ret = 0; /* Found the interface */

		/* Fill in the configuration structure */
		conf->name = info->link[i].ifname;
		conf->flags = info->link[i].flags;
		conf->up = (info->link[i].flags & IFF_UP) ? 1 : 0;
		conf->mtu = info->link[i].mtu;
		conf->stats = &info->link[i].stats;
		conf->linktype = info->link[i].type;
		conf->mac[0] = 0;

		/* FIXME Do we really need this? For now,
		 * we use this information to find sub-interfaces */
		conf->info = info;

		/* Get ethernet 0 MAC if not an ethernet interface */
		if (librouter_ip_get_mac(conf->linktype == ARPHRD_ETHER ? conf->name : "ethernet0",
		                mac_bin) == 0)
			sprintf(conf->mac, "%02x%02x.%02x%02x.%02x%02x",
			                mac_bin[0], mac_bin[1], mac_bin[2],
			                mac_bin[3], mac_bin[4], mac_bin[5]);

		/* Search for main IP address */
		ipaddr = &info->ipaddr[0];
		main_ip = &conf->main_ip;

		for (j = 0; j < MAX_NUM_IPS; j++, ipaddr++) {
			if (ipaddr->ifname[0] == 0)
				break; /* Finished */

			if (!strcmp(interface, ipaddr->ifname)) {
				strcpy(main_ip->ipaddr, inet_ntoa(ipaddr->local));
				librouter_ip_bitlen2mask(ipaddr->bitlen, main_ip->ipmask);
				/* If point-to-point, there is a remote IP address */
				if (conf->flags & IFF_POINTOPOINT)
					strcpy(main_ip->ippeer, inet_ntoa(ipaddr->remote));

				break; /* Found main IP, break main loop */
			}
		}

		/* Search for aliases */
		strncpy(name, conf->name, 14);
		strcat(name, ":0");

		/* Go through IP configuration */
		ipaddr = &info->ipaddr[0];
		sec_ip = &conf->sec_ip[0];
		for (j = 0; j < MAX_NUM_IPS; j++, ipaddr++) {

			if (ipaddr->ifname[0] == 0)
				break; /* Finished */

			if (strcmp(name, ipaddr->ifname) == 0) {
				strcpy(sec_ip->ipaddr, inet_ntoa(ipaddr->local));
				librouter_ip_bitlen2mask(ipaddr->bitlen, sec_ip->ipmask);
				sec_ip++;
			}
		}

	}

	/* Check if IP was configured by DHCP on ethernet interfaces */
	if (conf->linktype == ARPHRD_ETHER &&
			strstr(interface, "eth") &&
			!conf->is_subiface) {
		sprintf(daemon_dhcpc, DHCPC_DAEMON, interface);
		if (librouter_exec_check_daemon(daemon_dhcpc))
			conf->dhcpc = 1;
	}

	return ret;
}

void librouter_ip_iface_free_config(struct interface_conf *conf)
{
	if (conf == NULL)
		return;

//	if (conf->name)
//		free(conf->name);

	return;
}
