#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/config.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/sockios.h>
#include <fnmatch.h>
#include <linux/netlink.h>
#include <libnetlink.h>
#include <ll_map.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>
#include <libconfig/typedefs.h>
#include <libconfig/defines.h>
#include <libconfig/args.h>
#include <libconfig/dhcp.h>
#include <libconfig/ip.h>
#include <libconfig/dev.h>
#include <libconfig/error.h>
#include <libconfig/ppcio.h>

/* interface & address local lists */
int link_table_index;
link_table_t link_table[MAX_NUM_LINKS];
int ip_addr_table_index;
ip_addr_table_t ip_addr_table[MAX_NUM_IPS];

int ip_addr_table_backup_shmid = 0; /* ip address backup base on shmid */
ip_addr_table_t *ip_addr_table_backup = NULL; /* ip address backup base on shm */

int create_ipaddr_shm(int flags) /* SHM_RDONLY */
{
	if( ip_addr_table_backup_shmid == 0 ) {
		if( (ip_addr_table_backup_shmid =
				shmget((key_t)IPADDR_SHM_KEY, sizeof(ip_addr_table_t) * MAX_NUM_IPS, IPC_CREAT | IPC_EXCL | 0600)) == -1 ) {
			if( (ip_addr_table_backup_shmid = shmget((key_t)IPADDR_SHM_KEY, sizeof(ip_addr_table_t) * MAX_NUM_IPS, 0)) == -1 )
				return -1;
			else if( (ip_addr_table_backup = shmat(ip_addr_table_backup_shmid, 0, flags)) == NULL )
				return -1;
		} else {
			if( (ip_addr_table_backup = shmat(ip_addr_table_backup_shmid, 0, 0)) == NULL )
				return -1;
			memset(ip_addr_table_backup, 0, sizeof(ip_addr_table_t)*MAX_NUM_IPS); /* initial setting! */
		}
	}
	return 0;
}

int detach_ipaddr_shm(void)
{
	if( ip_addr_table_backup )
		return shmdt(ip_addr_table_backup);
	return 0;
}

int ipaddr_modify_shm(int add_del, ip_addr_table_t *data)
{
	int i;

	if( ip_addr_table_backup == NULL ) {
		syslog(LOG_ERR, "Addresses data not loaded!");
		return -1;
	}
	for( i=0; i < MAX_NUM_IPS; i++ ) {
		if( ip_addr_table_backup[i].ifname[0] == 0 )
			break; /* free slot */
		if( memcmp(&ip_addr_table_backup[i], data, sizeof(ip_addr_table_t)) == 0 )
		    break; /* same data! */
	}
	if( i == MAX_NUM_IPS )
		return -1;
	if( add_del )
		ip_addr_table_backup[i] = *data;
	else {
		if( i < (MAX_NUM_IPS - 1) ) /* shift over! */
			memmove(&ip_addr_table_backup[i], &ip_addr_table_backup[i+1], sizeof(ip_addr_table_t) * (MAX_NUM_IPS - i - 1));
		memset(&ip_addr_table_backup[MAX_NUM_IPS-1], 0, sizeof(ip_addr_table_t));
	}
	return 0;
}

static int link_name_to_index(char *ifname)
{
	int i;

	for(i=0; i < link_table_index; i++) {
		if (strcmp(link_table[i].ifname, ifname) == 0)
			return link_table[i].index;
	}
	return -1;
}

#if 0
static unsigned int link_name_to_flags(char *ifname)
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
int ipaddr_modify(int add_del, ip_addr_table_t *data)
{
	struct rtnl_handle rth;
	struct
	{
		struct nlmsghdr n;
		struct ifaddrmsg ifa;
		char buf[256];
	} req;
	int cmd;
	char *p, dev[16];

#if 0 /* !!! */
	fprintf(stderr, "ipaddr_modify %d %s\n", add_del, data->ifname);
#endif

	cmd = add_del ? RTM_NEWADDR : RTM_DELADDR;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = cmd;
	req.ifa.ifa_family = AF_INET; // AF_IPX
	req.ifa.ifa_prefixlen = data->bitlen;

	// local
	addattr_l(&req.n, sizeof(req), IFA_LOCAL, &data->local.s_addr, 4);

	// peer
	addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &data->remote.s_addr, 4);

	// broadcast
	addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &data->broadcast.s_addr, 4);

	// label
	addattr_l(&req.n, sizeof(req), IFA_LABEL, data->ifname, strlen(data->ifname)+1);

	if (cmd != RTM_DELADDR)
	{
		if (*((unsigned char *)(&data->local.s_addr)) == 127)
			req.ifa.ifa_scope = RT_SCOPE_HOST;
		else
			req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
	}

	if (rtnl_open(&rth, 0) < 0)
		return -1;

	// Identifica que eh um alias e retira os ':'
	strncpy(dev, data->ifname, 16);
	if ((p=strchr(dev, ':')) != NULL) {
		*p=0;
	}
	if ((req.ifa.ifa_index = link_name_to_index(dev)) == 0)
	{
		fprintf(stderr, "Cannot find device \"%s\"\n", dev);
		goto error;
	}
	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		goto error;

	rtnl_close(&rth);
	return 0;

error:
	rtnl_close(&rth);
	return -1;
}

int get_addrinfo(struct nlmsghdr *n)
{
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *rta_tb[IFA_MAX+1];

	if (ip_addr_table_index >= MAX_NUM_IPS) return 0;

	if (n->nlmsg_type != RTM_NEWADDR && n->nlmsg_type != RTM_DELADDR)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifa));
	if (len < 0)
	{
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	bzero(&ip_addr_table[ip_addr_table_index], sizeof(ip_addr_table_t));

	memset(rta_tb, 0, sizeof(rta_tb));
	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));

	if (!rta_tb[IFA_LOCAL])
		rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
#ifndef CONFIG_BERLIN_SATROUTER
	if (!rta_tb[IFA_ADDRESS])
		rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];
#endif

	if (ifa->ifa_family != AF_INET) return 0;

	ip_addr_table[ip_addr_table_index].bitlen = ifa->ifa_prefixlen;

	if (rta_tb[IFA_LOCAL])
	{
		memcpy(&ip_addr_table[ip_addr_table_index].local,
			RTA_DATA(rta_tb[IFA_LOCAL]), 4);

		if (rta_tb[IFA_ADDRESS])
		{
			memcpy(&ip_addr_table[ip_addr_table_index].remote,
				RTA_DATA(rta_tb[IFA_ADDRESS]), 4);
		}
	}

	if (rta_tb[IFA_BROADCAST])
	{
		memcpy(&ip_addr_table[ip_addr_table_index].broadcast, 
			RTA_DATA(rta_tb[IFA_BROADCAST]), 4);
	}
	
	strncpy(ip_addr_table[ip_addr_table_index].ifname, 
		(char*)RTA_DATA(rta_tb[IFA_LABEL]), IFNAMSIZ);

	ip_addr_table_index++;
			
	return 0;
}

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
};

#if 0
int get_selected_addrinfo(int ifindex, struct nlmsg_list *ainfo)
{
	for (; ainfo;  ainfo = ainfo->next) 
	{
		struct nlmsghdr *n = &ainfo->h;
		struct ifaddrmsg *ifa = NLMSG_DATA(n);

		if (n->nlmsg_type != RTM_NEWADDR)
			continue;

		if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifa)))
			return -1;

		get_addrinfo(n);
	}
	return 0;
}
#endif

int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
	struct nlmsg_list *h;
	struct nlmsg_list **lp;

	h = malloc(n->nlmsg_len+sizeof(void*));
	if (h == NULL)
		return -1;

	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	for (lp = linfo; *lp; lp = &(*lp)->next) /* NOTHING */;
	*lp = h;

	ll_remember_index(who, n, NULL);
	return 0;
}

int get_linkinfo(struct nlmsghdr *n)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX+1];
	int len = n->nlmsg_len;

	if (link_table_index >= MAX_NUM_LINKS) return 0;

	if (n->nlmsg_type != RTM_NEWLINK && n->nlmsg_type != RTM_DELLINK)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	bzero(&link_table[link_table_index], sizeof(link_table_t));
	link_table[link_table_index].index = ifi->ifi_index;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL)
	{
		fprintf(stderr, "BUG: nil ifname\n");
		return -1;
	}
	
	strncpy(link_table[link_table_index].ifname, 
		tb[IFLA_IFNAME] ? (char*)RTA_DATA(tb[IFLA_IFNAME]) : "<nil>", IFNAMSIZ);

	if (tb[IFLA_MTU])
		link_table[link_table_index].mtu = *(int*)RTA_DATA(tb[IFLA_MTU]);

	if (tb[IFLA_STATS])
	{
		memcpy(&link_table[link_table_index].stats, RTA_DATA(tb[IFLA_STATS]),
			sizeof(struct net_device_stats));
	}

	if (tb[IFLA_ADDRESS])
	{
		unsigned char size;
		size = RTA_PAYLOAD(tb[IFLA_ADDRESS]);
		link_table[link_table_index].addr_size = size;
		if (size>=MAX_HWADDR) size=MAX_HWADDR;
		memcpy(link_table[link_table_index].addr,
			RTA_DATA(tb[IFLA_ADDRESS]), size);
	}
	
	link_table[link_table_index].flags = ifi->ifi_flags;
	link_table[link_table_index].type = ifi->ifi_type;

	link_table_index++;

	return 0;
}

int get_if_list(void)
{
	struct rtnl_handle rth;
	struct nlmsg_list *linfo = NULL;
	struct nlmsg_list *ainfo = NULL;
	struct nlmsg_list *l, *a, *tmp;

	if (rtnl_open(&rth, 0) < 0)
		return -1;

	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETLINK) < 0)
	{
		perror("Cannot send dump request");
		goto error;
	}
	if (rtnl_dump_filter(&rth, store_nlmsg, &linfo, NULL, NULL) < 0)
	{
		fprintf(stderr, "Link Dump terminated\n");
		goto error;
	}
	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETADDR) < 0)
	{
		perror("Cannot send dump request");
		goto error;
	}
	if (rtnl_dump_filter(&rth, store_nlmsg, &ainfo, NULL, NULL) < 0)
	{
		fprintf(stderr, "Address Dump terminated\n");
		goto error;
	}

	link_table_index=0;
	ip_addr_table_index=0;
	for (l=linfo; l; )
	{
		get_linkinfo(&l->h);
		tmp=l;
		l=l->next;
		free(tmp);
	}
	for (a=ainfo; a; )
	{
		get_addrinfo(&a->h);
		tmp=a;
		a=a->next;
		free(tmp);
	}
	rtnl_close(&rth);
	return 0;

error:
	rtnl_close(&rth);
	return -1;
}

void ip_bitlen2mask(int bitlen, char *mask)
{
	unsigned long bitmask;
	int i;

	for (i=0, bitmask=0; i < bitlen; i++)
	{
		bitmask >>= 1;
		bitmask |= (1<<31);
	}
	sprintf(mask, "%d.%d.%d.%d", (int)((bitmask>>24)&0xff), 
		(int)((bitmask>>16)&0xff), (int)((bitmask>>8)&0xff), 
		(int)(bitmask&0xff));
}

/* DEL_ADDR, ADD_ADDR, DEL_SADDR, ADD_SADDR */
int ip_addr_add_del(ip_addr_oper_t add_del, char *ifname, char *local_ip, char *remote_ip, char *netmask)
{
	int i, bitlen, ret=0;
	IP mask, bitmask;
	ip_addr_table_t data, prim;

	if((ret=get_if_list()) < 0)
		return -1;

	strncpy(data.ifname, ifname, IFNAMSIZ);

	if(inet_aton(local_ip, &data.local) == 0)
		return -1;

	if((remote_ip != NULL) && (remote_ip[0])) {
		if (inet_aton(remote_ip, &data.remote) == 0)
			return -1;
	} else {
		data.remote.s_addr=data.local.s_addr; /* remote as local! */
	}

	if(inet_aton(netmask, &mask) == 0)
		return -1;
	for(bitlen=0, bitmask.s_addr=0; bitlen < 32; bitlen++)
	{
		if (mask.s_addr == bitmask.s_addr)
			break;
		bitmask.s_addr |= htonl(1<<(31-bitlen));
	}
	data.bitlen=bitlen; /* /8 /16 /24 */

	data.broadcast.s_addr=data.remote.s_addr;
	for(; bitlen < 32; bitlen++)
		data.broadcast.s_addr |= htonl(1<<(31-bitlen));

	prim=data; /* for primary address search */
	if (add_del&0x2) { /* secondary address */
		int primary;
		strcat(data.ifname, ":0"); /* alias label! */
		for(i=0; ip_addr_table[i].ifname[0] != 0 && i < MAX_NUM_IPS; i++) {
			if (strcmp(prim.ifname, ip_addr_table[i].ifname) == 0) {
				primary=1; /* primary address setted */
				break;
			}
		}
		if (primary) {
			for(i=0; i < ip_addr_table_index; i++)
				if (memcmp(&ip_addr_table[i], &data, sizeof(ip_addr_table_t)) == 0 ||
					memcmp(&ip_addr_table[i], &prim, sizeof(ip_addr_table_t)) == 0)
					break; /* found same address entry */
		} else {
			data=prim; /* secondary as primary */
			i=ip_addr_table_index; /* not found test below */
		}
	} else { /* primary address */
		strcat(prim.ifname, ":0"); /* alias label! */
		for(i=0; i < ip_addr_table_index; i++)
			if (memcmp(&ip_addr_table[i], &data, sizeof(ip_addr_table_t)) == 0)
				break; /* found same address entry */
	}

	if(i == ip_addr_table_index) {
		if (!(add_del&0x1)) 
			return -1; /* no address to remove */
	} else {
		if (add_del&0x1)
			return -1; /* address already configured! */
	}

	ret=ipaddr_modify((add_del&0x1), &data);

	return ret;
}

int ip_addr_flush(char *ifname)
{
	int i, ret;
	char alias[32];

	strcpy(alias, ifname);
	strcat(alias, ":0");

	if ((ret=get_if_list()) < 0)
		return ret;

	for (i=0; i < ip_addr_table_index; i++) {
		if (strcmp(ifname, ip_addr_table[i].ifname) == 0 ||
			strcmp(alias, ip_addr_table[i].ifname) == 0) { /* with alias */
				/* delete primary also delete secondaries */
				ret=ipaddr_modify(0, &ip_addr_table[i]); 
		}
	}

	return 0;
}

const char *masks[33] = {
  "0.0.0.0", "128.0.0.0", "192.0.0.0", "224.0.0.0",
  "240.0.0.0", "248.0.0.0", "252.0.0.0", "254.0.0.0",
  "255.0.0.0", "255.128.0.0", "255.192.0.0", "255.224.0.0",
  "255.240.0.0", "255.248.0.0", "255.252.0.0", "255.254.0.0",
  "255.255.0.0", "255.255.128.0", "255.255.192.0", "255.255.224.0",
  "255.255.240.0", "255.255.248.0", "255.255.252.0", "255.255.254.0",
  "255.255.255.0", "255.255.255.128", "255.255.255.192", "255.255.255.224",
  "255.255.255.240", "255.255.255.248", "255.255.255.252", "255.255.255.254",
  "255.255.255.255"
};

const char *rmasks[33] = {
  "255.255.255.255", "127.255.255.255", "63.255.255.255", "31.255.255.255",
  "15.255.255.255", "7.255.255.255", "3.255.255.255", "1.255.255.255",
  "0.255.255.255", "0.127.255.255", "0.63.255.255", "0.31.255.255",
  "0.15.255.255",  "0.7.255.255", "0.3.255.255", "0.1.255.255",
  "0.0.255.255", "0.0.127.255", "0.0.63.255", "0.0.31.255",
  "0.0.15.255", "0.0.7.255", "0.0.3.255", "0.0.1.255",
  "0.0.0.255", "0.0.0.127", "0.0.0.63", "0.0.0.31",
  "0.0.0.15", "0.0.0.7", "0.0.0.3", "0.0.0.1",
  "0.0.0.0"
};

const char *ciscomask(const char *mask)
{
	int i;
	
	for (i=0; i < 33; i++)
	{
		if (strcmp (masks[i], mask) == 0) return rmasks[i];
	}
	return mask;
}

const char *extract_mask(char *cidrblock)
{
	char *seperate;
	int   bits;

	seperate = strchr (cidrblock, '/');
	if (!seperate)
	{
		return masks[32];
	}

	*seperate = 0;
	++seperate;

	bits = atoi (seperate);
	if ((bits<33) && (bits>=0)) return masks[bits];
	return masks[32];
}

int netmask2cidr(const char *netmask)
{
	int i;

	for (i=0; i < 33; i++)
	{
		if (strcmp (masks[i], netmask) == 0) return i;
		if (strcmp (rmasks[i], netmask) == 0) return i;
	}
	return -1;
}

int get_mac(char *ifname, char *mac)
{
	return dev_get_hwaddr(ifname, (unsigned char *)mac);
}

/* !!! Caso especial para ethernet, em funcao do modo bridge */
char *get_ethernet_dev(char *dev)
{
#if 0
	static char brname[32]; /* dangerous! */

	snprintf(brname, 32, "%s1", BRIDGE_NAME); // TODO - incluir teste para mais de uma bridge
	// Teste: a ethernet pertence a alguma bridge ?
	if (br_checkif(brname, dev))
		return brname;
	else
#endif
		return dev;
}

void set_ethernet_ip_addr(char *ifname, char *addr, char *mask) /* main ethernet interface */
{
	char *dev=get_ethernet_dev(ifname); /* bridge x ethernet */
	int i;

	if (get_if_list() < 0)
		return;

	/* remove current address */
	for (i=0; i < ip_addr_table_index; i++)
	{
		if (strcmp(dev, ip_addr_table[i].ifname) == 0)
		{
			ipaddr_modify(0, &ip_addr_table[i]);
			break;
		}
	}

	ip_addr_add(dev, addr, NULL, mask); /* new address */

	if (strcmp(dev, "ethernet0") == 0)
		reload_udhcpd(0); /* udhcpd integration! */
#ifdef CONFIG_BERLIN_MU0
	else if (strcmp(dev, "ethernet1") == 0)
			reload_udhcpd(1); /* udhcpd integration! */
#elif defined(CONFIG_BERLIN_SATROUTER)
	switch( get_board_hw_id() )
	{
		case BOARD_HW_ID_1:
			break;
		case BOARD_HW_ID_0:
		case BOARD_HW_ID_2:
		case BOARD_HW_ID_3:
		case BOARD_HW_ID_4:
			if (strcmp(dev, "ethernet1") == 0)
				reload_udhcpd(1); /* udhcpd integration! */
			break;
	}
#endif
}

void set_ethernet_ip_addr_secondary(char *ifname, char *addr, char *mask)
{
	char *dev=get_ethernet_dev(ifname); /* bridge x ethernet */

	ip_addr_add_secondary(dev, addr, NULL, mask);
}

void set_ethernet_no_ip_addr(char *ifname)
{
	char *dev=get_ethernet_dev(ifname); /* bridge x ethernet */

	ip_addr_flush(dev); /* Clear all interface addresses */
}

void set_ethernet_no_ip_addr_secondary(char *ifname, char *addr, char *mask)
{
	char *dev=get_ethernet_dev(ifname); /* bridge x ethernet */

	ip_addr_del_secondary(dev, addr, NULL, mask);
}

/* Get address info !!! set_dhcp_server */
int get_interface_address(char *ifname, IP *addr, IP *mask, IP *bc, IP *peer)
{
	int i, ret;

	if ((ret=get_if_list()) < 0)
		return ret;
	/* running addresses */
	for (i=0; i < ip_addr_table_index; i++)
	{
		if (strcmp(ifname, ip_addr_table[i].ifname) == 0)
			break;
	}

	if (i <  ip_addr_table_index) {
		if (addr) *addr=ip_addr_table[i].local;
		if (peer) *peer=ip_addr_table[i].remote;
		if (bc) *bc=ip_addr_table[i].broadcast;
		if (mask) {
			if (ip_addr_table[i].bitlen) 
				mask->s_addr=htonl(~((1<<(32-ip_addr_table[i].bitlen))-1));
			else 
				mask->s_addr=0;
		}
	}

	return 0;
}

void get_interface_ip_addr(char *ifname, char *addr_str, char *mask_str)
{
	IP addr, mask;

	addr.s_addr = 0;
	mask.s_addr = 0;

	get_interface_address(ifname, &addr, &mask, 0, 0);
	strcpy(addr_str, inet_ntoa(addr));
	strcpy(mask_str, inet_ntoa(mask));
}

/* ppp unnumbered helper !!! ppp unnumbered x bridge hdlc */
void get_ethernet_ip_addr(char *ifname, char *addr_str, char *mask_str)
{
	char *dev;

	dev=get_ethernet_dev(ifname); /* bridge x ethernet */
	get_interface_ip_addr(dev, addr_str, mask_str);
}

/* Interface generic */
void set_interface_ip_addr(char *ifname, char *addr, char *mask)
{
	int i;

	if (get_if_list() < 0)
		return;
	/* remove current address */
	for (i=0; i < ip_addr_table_index; i++)
	{
		if (strcmp(ifname, ip_addr_table[i].ifname) == 0)
		{
			ipaddr_modify(0, &ip_addr_table[i]);
			break;
		}
	}

	ip_addr_add(ifname, addr, NULL, mask); /* new address */
}

void set_interface_ip_addr_secondary(char *ifname, char *addr, char *mask)
{
	ip_addr_add_secondary(ifname, addr, NULL, mask);
}

void set_interface_no_ip_addr(char *ifname)
{
	ip_addr_flush(ifname); /* Clear all interface addresses */
}

void set_interface_no_ip_addr_secondary(char *ifname, char *addr, char *mask)
{
	ip_addr_del_secondary(ifname, addr, NULL, mask);
}

unsigned int is_valid_port(char *data)
{
	char *p;

	if( !data )
		return 0;

	for( p=data; *p; p++ )
	{
		if( isdigit(*p) == 0 )
			return 0;
	}

	if( atoi(data) < 0 )
		return 0;

	return 1;
}

unsigned int is_valid_netmask(char *mask)
{
	arg_list argl=NULL;
	unsigned char elem;
	int i, k, n, ok, bit;
	char *p, *t, data[16];

	if( !mask )
		return 0;
	strncpy(data, mask, 15);
	data[15] = 0;
	for( p=data, n=0; *p; )
	{
		if( (t = strchr(p, '.')) )
		{
			*t = ' ';
			n++;
			p = t + 1;
		}
		else
			break;
	}
	if( n != 3 )
		return 0;
	if( !(n = parse_args_din(data, &argl)) )
		return 0;
	if( n != 4 )
	{
		free_args_din(&argl);
		return 0;
	}
	for( i=0, ok=1, bit=1; i < n; i++ )
	{
		elem = (unsigned char) atoi( argl[i] );
		if( elem != 0xFF )
		{
			if( bit )
			{
				for( k=7; k >= 0; k-- )
				{
					if( !((elem >> k) & 0x01) )
					{
						bit = 0;
						for( ; k >= 0; k-- )
						{
							if( ((elem >> k) & 0x01) )
							{
								ok = 0;
								break;
							}
						}
					}
					if( !ok )
						break;
				}
			}
			else
			{
				if( elem != 0x00 )
				{
					ok = 0;
					break;
				}
			}
		}
		if( !ok )
			break;
	}
	free_args_din(&argl);
	return ok;
}

/* Busca as estatisticas de uma interface */
int get_iface_stats(char *ifname, void *store)
{
	int i;
	int intf_indexes[MAX_NUM_LINKS];

	if( (store == NULL) || (get_if_list() < 0) )
		return -1;
	for( i=0; i < link_table_index; i++ )
		intf_indexes[i] = i;
	for( i=0; i < link_table_index; i++ ) {
		if( strcmp(link_table[intf_indexes[i]].ifname, ifname) == 0 ) {
			memcpy(store, &link_table[intf_indexes[i]].stats, sizeof(struct net_device_stats));
			return 0;
		}
	}
	return -1;
}

