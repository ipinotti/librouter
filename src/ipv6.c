/*
 * ipv6.c
 *
 *  Created on: Aug 29, 2011
 *      Author: ipinotti
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

#include <linux/autoconf.h>
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
#include "ipv6.h"
#include "dev.h"
#include "error.h"
#include "ppcio.h"
#include "pptp.h"

static int _get_linkinfo (struct nlmsghdr *n, struct linkv6_table *link)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_INET6_MAX + 1];
	int len = n->nlmsg_len;

	if (n->nlmsg_type != RTM_NEWLINK && n->nlmsg_type != RTM_DELLINK)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	ipv6_dbg("Netlink message from kernel has %d bytes\n", n->nlmsg_len);

	bzero(link, sizeof(struct linkv6_table));
	link->index = ifi->ifi_index;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, IFLA_INET6_MAX, IFLA_RTA (ifi), len);
	if (tb[IFLA_IFNAME] == NULL) {
		fprintf(stderr, "BUG: nil ifname\n");
		return -1;
	}

	strncpy(link->ifname, tb[IFLA_IFNAME] ? (char*) RTA_DATA (
			tb[IFLA_IFNAME]) : "<nil>", IFNAMSIZ);

	if (tb[IFLA_MTU])
		link->mtu = *(int*) RTA_DATA (tb[IFLA_MTU]);

	if (tb[IFLA_INET6_STATS]) {
		memcpy(&link->stats, RTA_DATA (tb[IFLA_INET6_STATS]),
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

static int _has_ppp_intf (int index, struct intfv6_info *info)
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

static int _clear_ppp_link_value_ (int index, struct intfv6_info *info)
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

static int _clear_ppp_ipaddr_value_ (int index, struct intfv6_info *info)
{
	char intf[8];
	int i;

	sprintf(intf, "ppp%d", index);

	for (i=0; info->ipaddr[i].ifname[0] != 0; i++) {
		if (!strcmp(info->ipaddr[i].ifname, intf)){
			if (librouter_dev_get_link_running(intf) != IFF_RUNNING){
				memset(&info->ipaddr[i], 0, sizeof(struct ipaddrv6_table));
				return 1;
			}
		}
	}

	return 0; /* Not found */
}

static int _link_name_to_index (char *ifname, struct linkv6_table *link)
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

static int _get_addrinfo (struct nlmsghdr *n, struct ipaddrv6_table *ipaddr)
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

	bzero(ipaddr, sizeof(struct ipaddrv6_table));

	memset(rta_tb, 0, sizeof(rta_tb));
	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA (ifa), IFA_PAYLOAD(n));

	if (ifa->ifa_family != AF_INET6) {
		return -1;
	}

	ipv6_dbg("Netlink message from kernel has %d bytes\n", n->nlmsg_len);

	if (!rta_tb[IFA_LOCAL]) {
		rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
	}

	if (!rta_tb[IFA_ADDRESS]) {
		rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];
	}

	ipaddr->bitlen = ifa->ifa_prefixlen;

	if (rta_tb[IFA_LOCAL]) {
		char ipv6_buf[128];
		memcpy(&ipaddr->local, RTA_DATA (rta_tb[IFA_LOCAL]), RTA_PAYLOAD(rta_tb[IFA_LOCAL]));
		inet_ntop(AF_INET6, &ipaddr->local, ipv6_buf, INET6_ADDRSTRLEN);
		ipv6_dbg("Copied address %s to main structure\n", ipv6_buf);
//		if (rta_tb[IFA_ADDRESS]) {
//			inet_ntop(AF_INET6, RTA_DATA(rta_tb[IFA_ADDRESS]), ipv6_buf, RTA_PAYLOAD(rta_tb[IFA_ADDRESS]));
//			ipv6_dbg("Copying peer address %s to main structure \n", ipv6_buf);
//			memcpy(&ipaddr->remote, RTA_DATA (rta_tb[IFA_ADDRESS]), RTA_PAYLOAD(rta_tb[IFA_ADDRESS]));
//		}
	}

	if (rta_tb[IFA_MULTICAST]) {
		memcpy(&ipaddr->multicast, RTA_DATA (rta_tb[IFA_MULTICAST]), RTA_PAYLOAD(rta_tb[IFA_MULTICAST]));
	}

	if (rta_tb[IFA_LABEL]) {
		strncpy(ipaddr->ifname, (char*) RTA_DATA (rta_tb[IFA_LABEL]), RTA_PAYLOAD(rta_tb[IFA_LABEL]));
		ipv6_dbg("Copied label %s to main structure\n", ipaddr->ifname);
	} else {
		strncpy(ipaddr->ifname, ll_index_to_name(ifa->ifa_index), IFNAMSIZ);
		ipv6_dbg("Copied label (no rta_tb) %s to main structure\n", ipaddr->ifname);

	}

	return 0;
}

int librouter_ipv6_store_nlmsg (const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct nlmsg_v6_list **linfo = (struct nlmsg_v6_list**) arg;
	struct nlmsg_v6_list *h;
	struct nlmsg_v6_list **lp;

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

int librouter_ipv6_get_if_list (struct intfv6_info *info)
{
	int link_index, ipv6_index, i;
	struct linkv6_table *link = &info->link[0];
	struct ipaddrv6_table *ipaddr = &info->ipaddr[0];
	struct rtnl_handle rth;
	struct nlmsg_v6_list *linfo = NULL;
	struct nlmsg_v6_list *ainfo = NULL;
	struct nlmsg_v6_list *l, *a, *tmp;
	char buff[256];
	memset(&buff, 0, sizeof(buff));


	if (rtnl_open(&rth, 0) < 0)
		return -1;

	if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		goto error;
	}

	if (rtnl_dump_filter(&rth, librouter_ipv6_store_nlmsg, &linfo, NULL, NULL) < 0) {
		fprintf(stderr, "Link Dump terminated\n");
		goto error;
	}

	if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETADDR) < 0) {
		perror("Cannot send dump request");
		goto error;
	}

	if (rtnl_dump_filter(&rth, librouter_ipv6_store_nlmsg, &ainfo, NULL, NULL) < 0) {
		fprintf(stderr, "Address Dump terminated\n");
		goto error;
	}


	link_index = 0;
	ipv6_index = 0;

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
			ipv6_dbg("__getaddrinfov6 succeeded for %s\n", ipaddr->ifname);
			inet_ntop(AF_INET6, &ipaddr->local, buff, INET6_ADDRSTRLEN);
			ipv6_dbg("ipv6 address is %s\n", buff);
			ipv6_dbg("bitmaskv6 is %d\n", ipaddr->bitlen);
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

int librouter_ipv6_addr_add (char *ifname, char *ipv6_addr, char *netmask_v6)
{
	char *ipv6_addr_mask;
	int ret = 0;

	ipv6_dbg("librouter_ipv6_addr_add\n\n");
	if (netmask_v6 == NULL){
		ipv6_dbg("Applying ipv6 cmd: /bin/ip -6 addr add %s dev %s\n", ipv6_addr, ifname);
		ret = librouter_exec_prog(1, IP_BIN, "-6", "addr", "add", ipv6_addr, "dev", ifname, NULL);
	}
	else{
		ipv6_addr_mask = malloc(256);
		snprintf(ipv6_addr_mask, 256, "%s/%s",ipv6_addr,netmask_v6);
		ipv6_dbg("Applying ipv6 cmd: /bin/ip -6 addr add %s/%s dev %s\n", ipv6_addr, netmask_v6, ifname);
		ret = librouter_exec_prog(1, IP_BIN, "-6", "addr", "add", ipv6_addr_mask, "dev", ifname, NULL);
		free(ipv6_addr_mask);
	}

	return ret;
}

int librouter_ipv6_addr_del (char *ifname, char *ipv6_addr, char *netmask_v6)
{
	char *ipv6_addr_mask;
	int ret = 0;

	if (netmask_v6 == NULL){
		ipv6_dbg("Applying ipv6 cmd: ip -6 addr del %s dev %s\n", ipv6_addr, ifname);
		ret = librouter_exec_prog(1, IP_BIN, "-6", "addr", "del", ipv6_addr, "dev", ifname, NULL);
	}
	else{
		ipv6_addr_mask = malloc(256);
		snprintf(ipv6_addr_mask, 256, "%s/%s",ipv6_addr,netmask_v6);
		ipv6_dbg("Applying ipv6 cmd: ip -6 addr del %s/%s dev %s\n", ipv6_addr, netmask_v6, ifname);
		ret = librouter_exec_prog(1, IP_BIN, "-6", "addr", "del", ipv6_addr_mask, "dev", ifname, NULL);
		free(ipv6_addr_mask);
	}

	return ret;
}

int librouter_ipv6_addr_flush (char *ifname)
{
	ipv6_dbg("Applying ipv6 cmd: ip -6 addr flush dev %s\n", ifname);
	return  librouter_exec_prog(1, IP_BIN, "-6", "addr", "flush", "dev", ifname, NULL);
}

int librouter_ipv6_get_mac (char *ifname, char *mac, int dot_mode)
{
	char mac_bin[32];
	memset(&mac_bin, 0, sizeof(mac_bin));
	if (librouter_dev_get_hwaddr(ifname, (unsigned char *) mac_bin) < 0)
		return -1;

	if (dot_mode)
		sprintf(mac, "%02x%02x.%02x%02x.%02x%02x", mac_bin[0], mac_bin[1], mac_bin[2], mac_bin[3], mac_bin[4], mac_bin[5]);
	else
		sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", mac_bin[0], mac_bin[1], mac_bin[2], mac_bin[3], mac_bin[4], mac_bin[5]);

	return 0;
}

char *librouter_ipv6_ethernet_get_dev (char *dev)
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

int librouter_ipv6_ethernet_set_addr (char *ifname, char *addr, char *mask, char *feature) /* main ethernet interface */
{
	char *dev = librouter_ipv6_ethernet_get_dev(ifname); /* bridge x ethernet */
	char *addr_mask_str = NULL;

#if 0
	int i;
	struct intfv6_info info;
	memset(&info, 0, sizeof(struct intfv6_info));

	if (librouter_ipv6_get_if_list(&info) < 0)
		return;

	/* remove current address */
	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (strcmp(dev, info.ipaddr[i].ifname) == 0) {
			librouter_ipv6_modify_addr(0, &info.ipaddr[i], &info);
			break;
		}
	}
#endif

	if (feature != NULL){
		if (!strcmp(feature, "link-local")){
			ipv6_dbg("IPv6: Verifying ipv6 addr for link-local\n");
			if (!librouter_ipv6_is_addr_link_local(addr))
				return -1;
		}

		if (!strcmp(feature, "eui-64")){
			ipv6_dbg("IPv6: Applying mod for ipv6 addr eui-64\n");
			addr_mask_str = malloc(256);
			memset(addr_mask_str, 0, 256);
			snprintf(addr_mask_str, 256,"%s/%s", addr, mask);
			ipv6_dbg("IPv6: Entering mod eui-64 for ipv6 addr [ %s ]\n", addr_mask_str);

			if (librouter_ipv6_mod_eui64(dev, addr_mask_str) < 0)
				return -1;

			ipv6_dbg("IPv6: Adding eth IPv6 address %s to %s\n", addr_mask_str, dev);
			if (librouter_ipv6_addr_add (dev, addr_mask_str, NULL) < 0)
				return -1;

			free(addr_mask_str);
			addr_mask_str = NULL;
			goto end;

		}
	}

	ipv6_dbg("IPv6: Adding eth IPv6 address %s to %s\n", addr, dev);
	if (librouter_ipv6_addr_add (dev, addr, mask) < 0)
		return -1;

end:
//	if (strcmp(dev, "eth0") == 0)
//		librouter_udhcpd_reload(0); /* udhcpd integration! */
//
//#ifdef OPTION_BRIDGE
//	librouter_br_update_ipaddr(ifname);
//#endif

	return 0;
}

int librouter_ipv6_ethernet_set_no_addr (char *ifname, char *addr, char *mask)
{
	char *dev = librouter_ipv6_ethernet_get_dev(ifname); /* bridge x ethernet */

	return librouter_ipv6_addr_del (dev, addr, mask);
}

int librouter_ipv6_ethernet_set_no_addr_flush (char *ifname)
{
	int ret = 0;
	char *dev = librouter_ipv6_ethernet_get_dev(ifname); /* bridge x ethernet */

	ipv6_dbg("Removing IPv6 addresses from %s\n", dev);
	ret = librouter_ipv6_addr_flush(dev); /* Clear all interface addresses */

//#ifdef OPTION_BRIDGE
//	librouter_br_update_ipaddr(ifname);
//#endif
	return ret;
}

int librouter_ipv6_is_addr_link_local (char *addr_str)
{
	IPV6 addr;

	memset(&addr, 0, sizeof(IPV6));
	ipv6_dbg("Doing verification in ipv6 addr [ %s ]\n", addr_str);

	inet_pton(AF_INET6, addr_str, &addr.s6_addr16);

	if ((addr.s6_addr16[0] == 0xfe80) || (addr.s6_addr16[0] == 0xfe90))
		return 1;

	return 0;

}

int librouter_ipv6_mod_eui64 (char *dev, char *addr_mask_str)
{
	char mac_addr[32];
	memset(&mac_addr, 0, sizeof(mac_addr));

	ipv6_dbg("Doing modification in ipv6 addr [ %s ] for dev [ %s ]\n", addr_mask_str, dev);

	if (librouter_ipv6_get_mac(dev, mac_addr, 0) < 0)
		return -1;

	ipv6_dbg("HW addrs is [ %s ]\n", mac_addr);

	if (librouter_ipv6_get_eui64_addr(addr_mask_str, mac_addr) < 0)
		return -1;

	librouter_str_striplf(addr_mask_str);

	ipv6_dbg("IPv6 addrs eui-64 is [ %s ]\n", addr_mask_str);

	return 0;
}

int librouter_ipv6_check_addr_string_for_ipv6(char *addr_str)
{
	    struct sockaddr_in6 sa;
	    return (inet_pton(AF_INET6, addr_str, &sa.sin6_addr));
}

int librouter_ipv6_conv_ipv4_addr_in_6to4_ipv6_addr(char *addr_str)
{
	FILE *ipv6calc;
	char line[256];
	char file_cmd[256];
	int check=0;

	if (librouter_ip_check_addr_string_for_ipv4(addr_str) < 0)
		return -1;

	sprintf(file_cmd,"/bin/ipv6calc -q --action conv6to4 --in ipv4 %s --out ipv6\n", addr_str);
	ipv6_dbg("Applying ipv6 cmd: /bin/ipv6calc -q --action conv6to4 --in ipv4 %s --out ipv6\n", addr_str);

	if (!(ipv6calc = popen(file_cmd, "r"))) {
		fprintf(stderr, "%% IPv6Calc not found\n");
		check = -1;
		goto end;
	}

	while (fgets(line, sizeof(line), ipv6calc) != NULL)
		strcpy(addr_str,line);

	if (addr_str == NULL)
		check = -1;

	librouter_str_striplf(addr_str);

	strcat(addr_str,"1");

	ipv6_dbg("IPv4 Addr converted to 6to4 IPv6 Addr correctly [ %s ]\n", addr_str);

end:
	pclose(ipv6calc);

	return check;
}

int librouter_ipv6_conv_6to4_ipv6_addr_in_ipv4_addr(char *addr_str)
{
	FILE *ipv6calc;
	char line[256];
	char file_cmd[256];
	int check=0;

	if (librouter_ipv6_check_addr_string_for_ipv6(addr_str) < 0)
		return -1;

	sprintf(file_cmd,"/bin/ipv6calc -q --action conv6to4 --in ipv6 %s --out ipv4\n", addr_str);
	ipv6_dbg("Applying ipv6 cmd: /bin/ipv6calc -q --action conv6to4 --in ipv6 %s --out ipv4\n", addr_str);

	if (!(ipv6calc = popen(file_cmd, "r"))) {
		fprintf(stderr, "%% IPv6Calc not found\n");
		check = -1;
		goto end;
	}

	while (fgets(line, sizeof(line), ipv6calc) != NULL)
		strcpy(addr_str,line);

	if (addr_str == NULL)
		check = -1;

	ipv6_dbg("IPv6 6to4 Addr converted to IPv4 Addr correctly\n");

end:
	pclose(ipv6calc);

	return check;
}

int librouter_ipv6_get_eui64_addr(char *addr_mask_str, char *mac_addr)
{
	FILE *ipv6calc;
	char line[256];
	char file_cmd[256];
	int check=0;

	sprintf(file_cmd,"/bin/ipv6calc -q --in prefix+mac --action prefixmac2ipv6 %s %s --out ipv6addr\n", addr_mask_str, mac_addr);
	ipv6_dbg("Applying ipv6 cmd: /bin/ipv6calc -q --in prefix+mac --action prefixmac2ipv6 %s %s --out ipv6addr\n", addr_mask_str, mac_addr);

	if (!(ipv6calc = popen(file_cmd, "r"))) {
		fprintf(stderr, "%% IPv6Calc not found\n");
		check = -1;
		goto end;
	}

	while (fgets(line, sizeof(line), ipv6calc) != NULL)
		strcpy(addr_mask_str,line);

	if (addr_mask_str == NULL)
		check = -1;

	ipv6_dbg("Ipv6 Addr EUI-64 modified correctly\n");

end:
	pclose(ipv6calc);

	return check;
}

/* Get address info !!! set_dhcp_server */
int librouter_ipv6_interface_get_info(char *ifname, IPV6 *addr, IPV6 *mask, IPV6 *mc, IPV6 *peer)
{
	int i, ret;
	struct intfv6_info info;

	memset(&info, 0, sizeof(struct intfv6_info));

	/* Get information into info structure */
	if ((ret = librouter_ipv6_get_if_list(&info)) < 0)
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
		if (mc)
			*mc = info.ipaddr[i].multicast;
		if (mask) {
//			if (info.ipaddr[i].bitlen)
//				mask->s6_addr = htonl(~((1 << (32 - info.ipaddr[i].bitlen)) - 1));
//			else
				mask = 0;
		}
	}

	return 0;
}

void librouter_ipv6_interface_get_ipv6_addr(char *ifname, char *addr_str, char *mask_str)
{
	IPV6 addr, mask;

	memset(&addr, 0, sizeof(IPV6));
	memset(&mask, 0, sizeof(IPV6));

	librouter_ipv6_interface_get_info(ifname, &addr, &mask, 0, 0);
	inet_pton(AF_INET6, addr.s6_addr, addr_str);
	inet_pton(AF_INET6, mask.s6_addr, mask_str);
}

/* ppp unnumbered helper !!! ppp unnumbered x bridge hdlc */
void librouter_ipv6_ethernet_ipv6_addr(char *ifname, char *addr_str, char *mask_str)
{
	char *dev;

	dev = librouter_ipv6_ethernet_get_dev(ifname); /* bridge x ethernet */
	librouter_ipv6_interface_get_ipv6_addr(dev, addr_str, mask_str);
}

/* Interface generic */
int librouter_ipv6_interface_set_addr(char *ifname, char *addr, char *mask, char *feature)
{
	int i = 0;
	char *addr_mask_str;

#if 0
	struct intfv6_info info;
	memset(&info, 0, sizeof(struct intfv6_info));

	/* Get information into info structure */
	if (librouter_ipv6_get_if_list(&info) < 0)
		return;

	/* remove current address */
	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (strcmp(ifname, info.ipaddr[i].ifname) == 0) {
			librouter_ipv6_modify_addr(0, &info.ipaddr[i], &info);
			break;
		}
	}
#endif

	if (feature != NULL){
		if (!strcmp(feature, "link-local")){
			ipv6_dbg("IPv6: Verifying ipv6 addr for link-local\n");
			if (!librouter_ipv6_is_addr_link_local(addr))
				return -1;
		}

		if (!strcmp(feature, "eui-64")){
			ipv6_dbg("IPv6: Applying mod for ipv6 addr eui-64\n");
			addr_mask_str = malloc(256);
			memset(addr_mask_str, 0, sizeof(&addr_mask_str));
			snprintf(addr_mask_str, sizeof(addr_mask_str),"%s/%s", addr, mask);

			if (librouter_ipv6_mod_eui64(ifname, addr_mask_str) < 0)
				return -1;

			ipv6_dbg("IPv6: Adding IP address %s to %s\n", addr_mask_str, ifname);
			if (librouter_ipv6_addr_add (ifname, addr_mask_str, NULL) < 0);
				return -1;

			free(addr_mask_str);
			addr_mask_str = NULL;
			goto end;

		}
	}

	ipv6_dbg("IPv6: Adding IP address %s to %s\n", addr, ifname);
	if (librouter_ipv6_addr_add (ifname, addr, mask) < 0)
		return -1;
end:
	return 0;
}

int librouter_ipv6_interface_set_no_addr(char *ifname, char *addr, char *mask)
{
	char *dev = librouter_ipv6_ethernet_get_dev(ifname); /* bridge x ethernet */

	return librouter_ipv6_addr_del (dev, addr, mask);
}

int librouter_ipv6_interface_set_no_addr_flush(char *ifname)
{
	int ret = 0;
	char *dev = librouter_ipv6_ethernet_get_dev(ifname); /* bridge x ethernet */

	ipv6_dbg("Removing IPv6 addresses from %s\n", dev);
	ret = librouter_ipv6_addr_flush(dev); /* Clear all interface addresses */

//#ifdef OPTION_BRIDGE
//	librouter_br_update_ipaddr(ifname);
//#endif
	return ret;
}

/* Busca as estatisticas de uma interface */
int librouter_ipv6_iface_get_stats(char *ifname, void *store)
{
	int i;
	struct intfv6_info info;

	memset(&info, 0, sizeof(struct intfv6_info));

	if ((store == NULL) || (librouter_ipv6_get_if_list(&info) < 0))
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

int librouter_ipv6_iface_get_config(char *interface, struct interfacev6_conf *conf,
                                  struct intfv6_info *info)
{
	char mac_bin[6];
	char name[16];
	int ret = -1;
	int i;
	char daemon_dhcpcv6[32];

	ipv6_dbg("Getting config for interface %s\n", interface);
	memset(conf, 0, sizeof(struct interfacev6_conf));

	if (strstr(interface, ".") != NULL) {/* Is sub-interface? */
		conf->is_subiface = 1;
	}

	/* Get all information if it hasn't been passed to us */
	if (info == NULL) {
		struct intfv6_info tmp_info;

		memset(&tmp_info, 0, sizeof(struct intfv6_info));
		ret = librouter_ipv6_get_if_list(&tmp_info);

		if (ret < 0) {
			printf("%% ERROR : Could not get interfaces information\n");
			return ret;
		}
		info = &tmp_info;
		ipv6_dbg("End ipv6 if_list for interface %s\n", interface);

	}

	ipv6_dbg("Got ipv6 if_list for interface %s\n", interface);


	/* Start searching for the desired interface,
	 * if not found return negative value */
	ret = -1;
	for (i = 0; i < MAX_NUM_LINKS; i++) {
		struct ipaddrv6_table *ipaddr;
		struct ipv6_t *main_ip;
		struct ipv6_t *sec_ip;
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

		if (librouter_dev_get_hwaddr(
					conf->linktype == ARPHRD_ETHER ? conf->name : "eth0",
							(unsigned char *)mac_bin) == 0)
		sprintf(conf->mac, "%02x%02x.%02x%02x.%02x%02x",
						mac_bin[0], mac_bin[1], mac_bin[2],
						mac_bin[3], mac_bin[4], mac_bin[5]);

		/* Search for main IP address */
		ipaddr = &info->ipaddr[0];
		main_ip = &conf->main_ip[0];

		for (j = 0; j < MAX_NUM_IPS; j++, ipaddr++) {

			if (ipaddr->ifname[0] == 0)
				break; /* Finished */

			if (!strcmp(interface, ipaddr->ifname)) {
				inet_ntop(AF_INET6, &ipaddr->local, main_ip->ipv6addr, INET6_ADDRSTRLEN);
				snprintf(main_ip->ipv6mask, 4, "%d",ipaddr->bitlen);
				main_ip++;
#if 0
				/* If point-to-point, there is a remote IP address */
				if (conf->flags & IFF_POINTOPOINT)
					inet_pton(AF_INET6, ipaddr->remote.s6_addr, main_ip->ippeer);
				break; /* Found main IP, break main loop */
#endif
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
				inet_ntop(AF_INET6, &ipaddr->local, sec_ip->ipv6addr, INET6_ADDRSTRLEN);
				snprintf(sec_ip->ipv6mask, 4, "%d", ipaddr->bitlen);
				sec_ip++;
			}
		}

	}

#if 0
	/* Check if IP was configured by DHCP on ethernet interfaces */
	if (conf->linktype == ARPHRD_ETHER &&
			strstr(interface, "eth") &&
			!conf->is_subiface) {
		sprintf(daemon_dhcpcv6, DHCPC_DAEMON, interface);
		if (librouter_exec_check_daemon(daemon_dhcpc))
			conf->dhcpc = 1;
	}
#endif

	return ret;
}

void librouter_ipv6_iface_free_config(struct interfacev6_conf *conf)
{
	if (conf == NULL)
		return;

//	if (conf->name)
//		free(conf->name);

	return;
}




void librouter_ipv6_bitlen2mask(int bitlen, char *mask)
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





/* ################# Excluded for a moment ######################### */

#if 0
/* DEL_ADDR, ADD_ADDR, DEL_SADDR, ADD_SADDR */
int librouter_ipv6_addr_add_del(ipv6_addr_oper_t add_del, char *ifname, char *local_ip, char *remote_ip, char *netmask)
{
	int i, bitlen, ret = 0;
	IPV6 mask, bitmask;
	struct ipaddrv6_table data, prim;
	struct intfv6_info info;

	ipv6_dbg("Modifying IPv6 ...");

	memset(&info, 0, sizeof(struct intfv6_info));

	/* Get information into info structure */
	if ((ret = librouter_ipv6_get_if_list(&info)) < 0)
		return -1;

	strncpy(data.ifname, ifname, IFNAMSIZ);
	if (inet_pton(AF_INET6, local_ip, &data.local) == 0)
		return -1;

	if ((remote_ip != NULL) && (remote_ip[0])) {
		if (inet_pton(AF_INET6, remote_ip, &data.remote) == 0)
			return -1;
	} else {
		data.remote = data.local; /* remote as local! */
	}

//	if (inet_pton(AF_INET6, netmask, &mask) == 0)
//		return -1;

//	for (bitlen = 0, bitmask.s6_addr = 0; bitlen < 32; bitlen++) {
//		if (mask.s6_addr == bitmask.s6_addr)
//			break;
//		bitmask.s6_addr32 |= htonl(1 << (31 - bitlen));
//	}
	data.bitlen = bitlen; /* /8 /16 /24 */

	data.multicast = data.remote;
//	for (; bitlen < 32; bitlen++)
//		data.multicast.s6_addr |= htonl(1 << (31 - bitlen));

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
				if (memcmp(&info.ipaddr[i], &data,sizeof(struct ipaddrv6_table)) == 0 ||
					memcmp(&info.ipaddr[i], &prim, sizeof(struct ipaddrv6_table)) == 0)
					break; /* found same address entry */
		} else {
			data = prim; /* secondary as primary */
			i = MAX_NUM_IPS; /* not found test below */
		}
	} else { /* primary address */
		strcat(prim.ifname, ":0"); /* alias label! */
		for (i = 0; i < MAX_NUM_IPS; i++)
			if (memcmp(&info.ipaddr[i], &data, sizeof(struct ipaddrv6_table)) == 0)
				break; /* found same address entry */
	}

	if (i == MAX_NUM_IPS) {
		if (!(add_del & 0x1))
			return -1; /* no address to remove */
	} else {
		if (add_del & 0x1)
			return -1; /* address already configured! */
	}

	ret = librouter_ipv6_modify_addr((add_del & 0x1), &data, &info);

	return ret;
}
#endif

#if 0
int librouter_ipv6_addr_flush(char *ifname)
{
	int i, ret;
	char alias[32];
	struct intfv6_info info;

	memset(&info, 0, sizeof(struct intfv6_info));

	strcpy(alias, ifname);
//	strcat(alias, ":0");

	if ((ret = librouter_ipv6_get_if_list(&info)) < 0)
		return ret;

	for (i = 0; i < MAX_NUM_IPS; i++) {
		if (strcmp(ifname, info.ipaddr[i].ifname) == 0 || strcmp(alias,
		                info.ipaddr[i].ifname) == 0) { /* with alias */
			/* delete primary also delete secondaries */
			ret = librouter_ipv6_modify_addr(0, &info.ipaddr[i], &info);
		}
	}

	return 0;
}
#endif

#if 0
const char *librouter_ipv6_ciscomask(const char *mask)
{
	int i;

	for (i = 0; i < 33; i++) {
		if (strcmp(masks[i], mask) == 0)
			return rmasks[i];
	}
	return mask;
}
#endif

#if 0
const char *librouter_ipv6_extract_mask(char *cidrblock)
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
#endif

#if 0
//TODO Analisar função, pode resultar em BUG devido ao rmasks
int librouter_ipv6_netmask2cidr(const char *netmask)
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
#endif

#if 0
void librouter_ipv6_interface_set_no_addr_secondary(char *ifname, char *addr, char *mask)
{
	ipv6_addr_del_secondary (ifname, addr, NULL, mask);
}
#endif

#if 0
void librouter_ipv6_ethernet_set_addr_secondary(char *ifname, char *addr, char *mask)
{
	char *dev = librouter_ipv6_ethernet_get_dev(ifname); /* bridge x ethernet */

	ipv6_addr_add_secondary (dev, addr, NULL, mask);
}
#endif

#if 0
unsigned int librouter_ipv6_is_valid_port(char *data)
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
#endif

#if 0
unsigned int librouter_ipv6_is_valid_netmask(char *mask)
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
#endif

#if 0
/* add/del ip address with netlink */
int librouter_ipv6_modify_addr(int add_del,
                  struct ipaddrv6_table *data,
                  struct intfv6_info *info)
{
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr n;
		struct ifaddrmsg ifa;
		char buf[256];
	} req;
	int cmd;
	char *p, dev[16];

	ipv6_dbg("Modifying IPv6 ...");

	/* Add or Remove */
	cmd = add_del ? RTM_NEWADDR : RTM_DELADDR;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = cmd;
	req.ifa.ifa_family = AF_INET6;
	req.ifa.ifa_prefixlen = data->bitlen;

	/* local */
	addattr_l(&req.n, sizeof(req), IFA_LOCAL, &data->local.s6_addr, 4);

	/* peer */
	addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &data->remote.s6_addr, 4);

	/* broadcast */
	addattr_l(&req.n, sizeof(req), IFA_MULTICAST, &data->multicast.s6_addr, 4);

	/* label */
	addattr_l(&req.n, sizeof(req), IFA_LABEL, data->ifname, strlen(data->ifname) + 1);

	if (cmd != RTM_DELADDR) {
		if (*((unsigned char *) (&data->local.s6_addr)) == 127)
			req.ifa.ifa_scope = RT_SCOPE_HOST;
		else
			req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
	}

	if (rtnl_open(&rth, 0) < 0)
		return -1;

//	/* Identifica que eh um alias e retira os ':' */
//	strncpy(dev, data->ifname, 16);
//	if ((p = strchr(dev, ':')) != NULL) {
//		*p = 0;
//	}

	if ((req.ifa.ifa_index = _link_name_to_index(dev, &(info->link[0])))
	                == 0) {
		fprintf(stderr, "Cannot find device \"%s\"\n", dev);
		goto error;
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		goto error;

	rtnl_close(&rth);
	ipv6_dbg("Success ...");
	return 0;

	error: rtnl_close(&rth);
	ipv6_dbg("Error ...");
	return -1;
}
#endif
