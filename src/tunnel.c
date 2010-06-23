/*
 * iptunnel.c	       "ip tunnel"
 *
 * Check linux kernel:
 * linux/net/ipv4/ip_gre.c
 * linux/net/ipv4/ipip.c
 *
 * Changes:
 */

/*
 Cisco1700-PD3#show interfaces tunnel 0
 Tunnel0 is up, line protocol is up
 Hardware is Tunnel
 Internet address is 12.12.12.12/8
 MTU 1514 bytes, BW 9 Kbit, DLY 500000 usec,
 reliability 255/255, txload 1/255, rxload 1/255
 Encapsulation TUNNEL, loopback not set
 Keepalive set (10 sec)
 Tunnel source 192.168.2.1 (FastEthernet0), destination 192.168.2.70
 Tunnel protocol/transport GRE/IP, key disabled, sequencing disabled
 Checksumming of packets disabled,  fast tunneling enabled
 Last input never, output never, output hang never
 Last clearing of "show interface" counters never
 Input queue: 0/75/0/0 (size/max/drops/flushes); Total output drops: 12
 Queueing strategy: fifo
 Output queue :0/0 (size/max)
 5 minute input rate 0 bits/sec, 0 packets/sec
 5 minute output rate 0 bits/sec, 0 packets/sec
 0 packets input, 0 bytes, 0 no buffer
 Received 0 broadcasts, 0 runts, 0 giants, 0 throttles
 0 input errors, 0 CRC, 0 frame, 0 overrun, 0 ignored, 0 abort
 12 packets output, 624 bytes, 0 underruns
 0 output errors, 0 collisions, 0 interface resets
 0 output buffer failures, 0 output buffers swapped out

 Cisco1700-PD3(config)#interface ?
 FastEthernet       FastEthernet IEEE 802.3
 Loopback           Loopback interface
 Null               Null interface
 Serial             Serial
 Tunnel             Tunnel interface

 Cisco1700-PD3(config)#interface tunnel ?
 <0-2147483647>  Tunnel interface number

 Cisco1700-PD3(config-if)#?
 Interface configuration commands:
 backup                  Modify backup parameters
 bandwidth               Set bandwidth informational parameter
 description             Interface specific description
 exit                    Exit from interface configuration mode
 help                    Description of the interactive help system
 ip                      Interface Internet Protocol config commands
 logging                 Configure logging for interface
 mtu                     Set the interface Maximum Transmission Unit (MTU)
 no                      Negate a command or set its defaults
 shutdown                Shutdown the selected interface
 snmp                    Modify SNMP interface parameters
 tunnel                  protocol-over-protocol tunneling

 Cisco1700-PD3(config-if)#description ?
 LINE  Up to 240 characters describing this interface

 Cisco1700-PD3(config-if)#ip ?
 Interface IP configuration subcommands:
 access-group        Specify access control for packets
 address             Set the IP address of an interface
 mtu                 Set IP Maximum Transmission Unit
 nat                 NAT interface commands
 ospf                OSPF interface commands
 policy              Enable policy routing
 rip                 Router Information Protocol
 split-horizon       Perform split horizon

 Cisco1700-PD3(config-if)#mtu ?
 <64-18024>  MTU size in bytes

 Cisco1700-PD3(config-if)#snmp ?
 trap  Allow a specific SNMP trap

 Cisco1700-PD3(config-if)#snmp trap ?
 link-status  Allow SNMP LINKUP and LINKDOWN traps

 Cisco1700-PD3(config-if)#snmp trap link-status ?
 <cr>

 Cisco1700-PD3(config-if)#tunnel ?
 checksum            enable end to end checksumming of packets
 destination         destination of tunnel
 key                 security or selector key
 mode                tunnel encapsulation method
 path-mtu-discovery  Enable Path MTU Discovery on tunnel
 sequence-datagrams  drop datagrams arriving out of order
 source              source of tunnel packets
 udlr                associate tunnel with unidirectional interface

 Cisco1700-PD3(config-if)#tunnel checksum ?
 <cr>

 Cisco1700-PD3(config-if)#tunnel destination ?
 Hostname or A.B.C.D  ip address or host name

 Cisco1700-PD3(config-if)#tunnel key ?
 <0-4294967295>  key

 Cisco1700-PD3(config-if)#tunnel mode ?
 gre     generic route encapsulation protocol
 ipip    IP over IP encapsulation

 Cisco1700-PD3(config-if)#tunnel mode ipip ?
 decapsulate-any  Incoming traffic only
 <cr>

 Cisco1700-PD3(config-if)#tunnel mode gre ?
 ip          over IP
 multipoint  over IP (multipoint)

 Cisco1700-PD3(config-if)#tunnel path-mtu-discovery ?
 age-timer  Set PMTU aging timer
 <cr>

 Cisco1700-PD3(config-if)#tunnel sequence-datagrams ?
 <cr>

 Cisco1700-PD3(config-if)#tunnel source ?
 A.B.C.D            ip address
 Async              Async interface
 FastEthernet       FastEthernet IEEE 802.3
 Loopback           Loopback interface
 Null               Null interface
 Serial             Serial
 Tunnel             Tunnel interface

 Cisco1700-PD3(config-if)#tunnel udlr ?
 receive-only  Tunnel is receive-only capable
 send-only     Tunnel is send-only capable
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <linux/byteorder/big_endian.h>
#include <linux/if_tunnel.h>
#include "dev.h"
#include "device.h"
#include "ip.h"
#include "tunnel.h"

#if 0
static int do_ioctl_get_iftype(char *dev)
{
	struct ifreq ifr;
	int fd;
	int err;

	strcpy(ifr.ifr_name, dev);
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCGIFHWADDR, &ifr);
	if (err) {
		perror("do_ioctl_get_iftype");
		return -1;
	}
	close(fd);
	return ifr.ifr_addr.sa_family;
}

static char *do_ioctl_get_ifname(int idx)
{
	static struct ifreq ifr;
	int fd;
	int err;

	ifr.ifr_ifindex = idx;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	err = ioctl(fd, SIOCGIFNAME, &ifr);
	if (err) {
		perror("do_ioctl_get_ifname");
		return NULL;
	}
	close(fd);
	return ifr.ifr_name;
}

static int do_ioctl_get_ifaddr(char *dev)
{
	struct ifreq ifr;
	int fd;
	int err;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_addr.sa_family = AF_INET;
	err = ioctl(fd, SIOCGIFADDR, &ifr);
	if (err) {
		perror("do_ioctl_get_ifaddr");
		return 0;
	}
	close(fd);
	return *((int *)&ifr.ifr_addr.sa_data[2]);
}
#endif

static int do_ioctl_get_ifindex (char *dev)
{
	struct ifreq ifr;
	int fd;
	int err;

	strcpy (ifr.ifr_name, dev);
	fd = socket (AF_INET, SOCK_DGRAM, 0);
	err = ioctl (fd, SIOCGIFINDEX, &ifr);
	if (err) {
		perror ("do_ioctl_get_ifindex");
		return 0;
	}
	close (fd);
	return ifr.ifr_ifindex;
}

static int do_get_ioctl (char *basedev, struct ip_tunnel_parm *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	strcpy (ifr.ifr_name, basedev);
	ifr.ifr_ifru.ifru_data = (void*) p;
	fd = socket (AF_INET, SOCK_DGRAM, 0);
	err = ioctl (fd, SIOCGETTUNNEL, &ifr);
	if (err)
		perror ("do_get_ioctl");
	close (fd);
	return err;
}

static int do_add_ioctl (int cmd, struct ip_tunnel_parm *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	if (cmd == SIOCCHGTUNNEL && p->name[0])
		strcpy (ifr.ifr_name, p->name);
	else {
		switch (p->iph.protocol) {
		case IPPROTO_IPIP:
			strcpy (ifr.ifr_name, "tunl0");
			break;
		case IPPROTO_GRE:
		default:
			strcpy (ifr.ifr_name, "gre0");
			break;
		}
	}
	ifr.ifr_ifru.ifru_data = (void*) p;
	fd = socket (AF_INET, SOCK_DGRAM, 0);
	err = ioctl (fd, cmd, &ifr);
	if (err)
		perror ("do_add_ioctl");
	close (fd);
	return err;
}

static int do_del_ioctl (char *basedev, struct ip_tunnel_parm *p)
{
	struct ifreq ifr;
	int fd;
	int err;

	if (p->name[0])
		strcpy (ifr.ifr_name, p->name);
	else
		strcpy (ifr.ifr_name, basedev);
	ifr.ifr_ifru.ifru_data = (void*) p;
	fd = socket (AF_INET, SOCK_DGRAM, 0);
	err = ioctl (fd, SIOCDELTUNNEL, &ifr);
	if (err)
		perror ("do_del_ioctl");
	close (fd);
	return err;
}

int add_tunnel (char *name) /* interface tunnel <0-9> */
{
	struct ip_tunnel_parm p;

	if (!libconfig_dev_exists (name)) {
		memset (&p, 0, sizeof(p));
		p.iph.version = 4;
		p.iph.ihl = 5;
#ifndef IP_DF
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#endif
		p.iph.frag_off = htons (IP_DF);
		p.iph.protocol = IPPROTO_GRE; /* default is gre/ip */
		strncpy (p.name, name, IFNAMSIZ);
		return do_add_ioctl (SIOCADDTUNNEL, &p);
	}
	return 0;
}

int del_tunnel (char *name) /* no interface tunnel <0-9> */
{
	int err;
	struct ip_tunnel_parm p;

	if (libconfig_dev_exists (name)) {
		if ((err = do_get_ioctl (name, &p)))
			return -1;
		return do_del_ioctl (p.name, &p);
	}
	return 0;
}

/*
 tos/dsfield p->iph.tos = uval;
 */
int change_tunnel (char *name, tunnel_param_type type, void *param)
{
	int err;
	struct in_addr address;
	struct ip_tunnel_parm p;

	if (libconfig_dev_exists (name)) {
		if ((err = do_get_ioctl (name, &p))) {
			fprintf (stderr, "%% %s not found.\n", name);
			return -1;
		}
		switch (type) {
		case source:
			if (param != NULL) {
				if (inet_aton ((char *) param, &address) != 0)
					p.iph.saddr = address.s_addr;
			} else {
				p.iph.saddr = 0;
			}
			p.link = 0; /* Disable interface association! */
			break;
#if 0
			case source_interface:
			if (param != NULL) {
				strncpy(p.linkname, (char *)param, IFNAMSIZ);
				p.link = do_ioctl_get_ifindex((char *)param);
				if (p.link) {
					IP addr;

					if (get_interface_address((char *)param, &addr, NULL, NULL, NULL) == 0)
					p.iph.saddr = addr.s_addr;
				}
			}
			break;
#endif
		case destination:
			if (param != NULL) {
				if (inet_aton ((char *) param, &address) != 0)
					p.iph.daddr = address.s_addr;
			} else {
				p.iph.daddr = 0;
			}
			if (p.i_key == 0 && IN_MULTICAST(ntohl(p.iph.daddr))) {
				p.i_key = p.iph.daddr;
				p.i_flags |= GRE_KEY;
			}
			if (p.o_key == 0 && IN_MULTICAST(ntohl(p.iph.daddr))) {
				p.o_key = p.iph.daddr;
				p.o_flags |= GRE_KEY;
			}
			if (IN_MULTICAST(ntohl(p.iph.daddr))
			                && !p.iph.saddr) {
				fprintf (stderr,
				                "%% Broadcast tunnel requires a source address.\n");
				return -1;
			}
			break;
		case checksum:
			if (param != NULL) {
				p.i_flags |= GRE_CSUM;
				p.o_flags |= GRE_CSUM;
			} else {
				p.i_flags &= ~GRE_CSUM;
				p.o_flags &= ~GRE_CSUM;
			}
			break;
		case sequence:
			if (param != NULL) {
				p.i_flags |= GRE_SEQ;
				p.o_flags |= GRE_SEQ;
			} else {
				p.i_flags &= ~GRE_SEQ;
				p.o_flags &= ~GRE_SEQ;
			}
			break;
		case pmtu: /* CHANGE */
			if (param != NULL) {
				p.iph.frag_off = htons (IP_DF);
			} else {
#if 0
				if (p.iph.ttl) {
					fprintf(stderr, "%% ttl != 0 and no pmtu discovery are incompatible\n");
					return -1;
				}
#else
				p.iph.ttl = 0;
#endif
				p.iph.frag_off = 0;
			}
			return do_add_ioctl (SIOCCHGTUNNEL, &p);
		case ttl: /* CHANGE */
			if (param != NULL) {
				p.iph.ttl = htonl (atoi ((char *) param));
#if 0
				if (p.iph.ttl && p.iph.frag_off == 0) {
					fprintf(stderr, "%% ttl != 0 and no pmtu discovery are incompatible\n");
					return -1;
				}
#else
				p.iph.frag_off = htons (IP_DF);
#endif
			} else {
				p.iph.ttl = 0;
			}
			return do_add_ioctl (SIOCCHGTUNNEL, &p);
		case key:
			if (param != NULL) {
				p.i_flags |= GRE_KEY;
				p.o_flags |= GRE_KEY;
				p.i_key = p.o_key = htonl (
				                atoi ((char *) param));
			} else {
				p.i_flags &= ~GRE_KEY;
				p.o_flags &= ~GRE_KEY;
				p.i_key = p.o_key = 0;
			}
			break;
		default:
			break;
		}
		do_del_ioctl (p.name, &p); /* remove old tunnel param! */
		return do_add_ioctl (SIOCADDTUNNEL, &p); /* add new tunnel param! */
	}
	return 0;
}

int mode_tunnel (char *name, int mode)
{
	int err;
	struct ip_tunnel_parm p;

	if (libconfig_dev_exists (name)) {
		if ((err = do_get_ioctl (name, &p)))
			return -1;
		if (p.iph.protocol != mode) {
			do_del_ioctl (p.name, &p); /* remove tunnel */
			p.iph.protocol = mode; /* new mode */
			if (mode == IPPROTO_IPIP) { /* Keys are not allowed with ipip and sit. */
				p.i_flags &= ~(GRE_KEY | GRE_CSUM | GRE_SEQ);
				p.o_flags &= ~(GRE_KEY | GRE_CSUM | GRE_SEQ);
			}
			return do_add_ioctl (SIOCADDTUNNEL, &p); /* add new one! */
		}
	}
	return 0;
}

void dump_tunnel_interface (FILE *out, int conf_format, char *name)
{
	int err;
	struct in_addr address;
	struct ip_tunnel_parm p;

	if (libconfig_dev_exists (name)) {
		if ((err = do_get_ioctl (name, &p)))
			return;
		if (conf_format) {
			switch (p.iph.protocol) {
			case IPPROTO_IPIP:
				fprintf (out, " tunnel mode ipip\n");
				break;
			case IPPROTO_GRE:
			default:
				fprintf (out, " tunnel mode gre\n");
				break;
			}
			//if (p.link) {
			//		fprintf(out, " tunnel source %s\n", linux_to_cish_dev_cmdline(p.linkname));
			//} else {
			if (p.iph.saddr) {
				address.s_addr = p.iph.saddr;
				fprintf (out, " tunnel source %s\n", inet_ntoa (
				                address));
			}
			//}
			if (p.iph.daddr) {
				address.s_addr = p.iph.daddr;
				fprintf (out, " tunnel destination %s\n",
				                inet_ntoa (address));
			}
			if (p.iph.protocol == IPPROTO_GRE) {
				if ((p.i_flags & GRE_CSUM) && (p.o_flags
				                & GRE_CSUM))
					fprintf (out, " tunnel checksum\n");
				if ((p.i_flags & GRE_KEY) && (p.o_flags
				                & GRE_KEY))
					fprintf (out, " tunnel key %u\n",
					                p.i_key);
				if ((p.i_flags & GRE_SEQ) && (p.o_flags
				                & GRE_SEQ))
					fprintf (out,
					                " tunnel sequence-datagrams\n");
			}
			if (p.iph.frag_off == htons (IP_DF))
				fprintf (out, " tunnel path-mtu-discovery\n");
			if (p.iph.ttl)
				fprintf (out, " tunnel ttl %u\n", p.iph.ttl);
		} else {
			fprintf (out, "  Encapsulation TUNNEL\n");
			if (p.iph.saddr || p.iph.daddr) {
				fprintf (out, "  Tunnel ");
				if (p.iph.saddr) {
					fprintf (out, "source ");
					address.s_addr = p.iph.saddr;
					//if (p.link) {
					//	fprintf(out, "%s (%s)", inet_ntoa(address), p.linkname);
					//} else {
					fprintf (out, "%s", inet_ntoa (address));
					//}
					if (p.iph.daddr)
						fprintf (out, ", ");
				}
				if (p.iph.daddr) {
					address.s_addr = p.iph.daddr;
					fprintf (out, "destination %s",
					                inet_ntoa (address));
				}
				fprintf (out, "\n");
			}
			switch (p.iph.protocol) {
			case IPPROTO_IPIP:
				fprintf (out,
				                "  Tunnel protocol/transport IP/IP\n");
				break;
			case IPPROTO_GRE:
			default:
				fprintf (out,
				                "  Tunnel protocol/transport GRE/IP");
				if (p.i_flags & GRE_KEY) {
					fprintf (out, ", key %d", p.i_key);
				} else {
					fprintf (out, ", key disabled");
				}
				if (p.i_flags & GRE_SEQ) {
					fprintf (out, ", sequencing enabled");
				} else {
					fprintf (out, ", sequencing disabled");
				}
				if (p.i_flags & GRE_CSUM) {
					fprintf (out,
					                ", checksumming of packets enabled");
				} else {
					fprintf (out,
					                ", checksumming of packets disabled");
				}
				fprintf (out, "\n");
				break;
			}
			if (p.iph.frag_off == htons (IP_DF))
				fprintf (out, "  Path MTU Discovery\n");
		}
	}
}

#if 0
void print_tunnel(struct ip_tunnel_parm *p)
{
	char s1[256];
	char s2[256];
	char s3[64];
	char s4[64];

	format_host(AF_INET, 4, &p->iph.daddr, s1, sizeof(s1));
	format_host(AF_INET, 4, &p->iph.saddr, s2, sizeof(s2));
	inet_ntop(AF_INET, &p->i_key, s3, sizeof(s3));
	inet_ntop(AF_INET, &p->o_key, s4, sizeof(s4));

	printf("%s: %s/ip  remote %s  local %s ",
			p->name,
			p->iph.protocol == IPPROTO_IPIP ? "ip" :
			(p->iph.protocol == IPPROTO_GRE ? "gre" :
					(p->iph.protocol == IPPROTO_IPV6 ? "ipv6" : "unknown")),
			p->iph.daddr ? s1 : "any", p->iph.saddr ? s2 : "any");
	if (p->link) {
		char *n = do_ioctl_get_ifname(p->link);
		if (n)
		printf(" dev %s ", n);
	}
	if (p->iph.ttl)
	printf(" ttl %d ", p->iph.ttl);
	else
	printf(" ttl inherit ");
	if (p->iph.tos) {
		SPRINT_BUF(b1);
		printf(" tos");
		if (p->iph.tos&1)
		printf(" inherit");
		if (p->iph.tos&~1)
		printf("%c%s ", p->iph.tos&1 ? '/' : ' ',
				rtnl_dsfield_n2a(p->iph.tos&~1, b1, sizeof(b1)));
	}
	if (!(p->iph.frag_off&htons(IP_DF)))
	printf(" nopmtudisc");

	if ((p->i_flags&GRE_KEY) && (p->o_flags&GRE_KEY) && p->o_key == p->i_key)
	printf(" key %s", s3);
	else if ((p->i_flags|p->o_flags)&GRE_KEY) {
		if (p->i_flags&GRE_KEY)
		printf(" ikey %s ", s3);
		if (p->o_flags&GRE_KEY)
		printf(" okey %s ", s4);
	}

	if (p->i_flags&GRE_SEQ)
	printf("%s  Drop packets out of sequence.\n", _SL_);
	if (p->i_flags&GRE_CSUM)
	printf("%s  Checksum in received packet is required.", _SL_);
	if (p->o_flags&GRE_SEQ)
	printf("%s  Sequence packets on output.", _SL_);
	if (p->o_flags&GRE_CSUM)
	printf("%s  Checksum output packets.", _SL_);
}
#endif

