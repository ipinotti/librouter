/*
 * dev.c
 *
 *  Created on: Jun 23, 2010
 */

#include "dev.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/hdlc.h>
#include <linux/sockios.h>
#include <linux/netlink.h>

#include "libnetlink/libnetlink.h"
/*#include <ll_map.h>*/
#include <fnmatch.h>

#include "typedefs.h"
#include "defines.h"
#include "ip.h"
#include "error.h"
#include "device.h"

#ifdef OPTION_MODEM3G
#include "modem3G.h"
#endif

static int _librouter_dev_get_ctrlfd(void)
{
	int s_errno;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;

	s_errno = errno;

	fd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;

	fd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;

	errno = s_errno;
	perror("Cannot create control socket");

	return -1;
}

static int _librouter_dev_chflags(char *dev, __u32 flags, __u32 mask)
{
	struct ifreq ifr;
	int fd;
	int err;

	strcpy(ifr.ifr_name, dev);
	fd = _librouter_dev_get_ctrlfd();
	if (fd < 0)
		return -1;

	err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		librouter_pr_error(1, "do_chflags %s SIOCGIFFLAGS",
		                ifr.ifr_name);
		close(fd);
		return -1;
	}

	if ((ifr.ifr_flags ^ flags) & mask) {
		ifr.ifr_flags &= ~mask;
		ifr.ifr_flags |= mask & flags;
		err = ioctl(fd, SIOCSIFFLAGS, &ifr);
		if (err)
			perror("SIOCSIFFLAGS");
	}

	close(fd);

	return err;
}

int librouter_dev_get_flags(char *dev, __u32 *flags)
{
	struct ifreq ifr;
	int fd;
	int err;

	strcpy(ifr.ifr_name, dev);
	fd = _librouter_dev_get_ctrlfd();

	if (fd < 0)
		return -1;

	err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		//pr_error(1, "dev_get_flags %s SIOCGIFFLAGS", ifr.ifr_name);
		close(fd);
		return -1;
	}

	*flags = ifr.ifr_flags;
	close(fd);

	return err;
}

//------------------------------------------------------------------------------

int librouter_dev_set_qlen(char *dev, int qlen)
{
	struct ifreq ifr;
	int s;

	s = _librouter_dev_get_ctrlfd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_qlen = qlen;

	if (ioctl(s, SIOCSIFTXQLEN, &ifr) < 0) {
		perror("SIOCSIFXQLEN");
		close(s);
		return -1;
	}

	close(s);

	return 0;
}

int librouter_dev_get_qlen(char *dev)
{
	struct ifreq ifr;
	int s;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFTXQLEN, &ifr) < 0) {
		//printf("dev_get_qlen: %s ", dev); /* !!! */
		//perror("SIOCGIFXQLEN");
		close(s);
		return 0;
	}

	close(s);

	return ifr.ifr_qlen;
}

int librouter_dev_set_mtu(char *dev, int mtu)
{
	struct ifreq ifr;
	int s;

	s = _librouter_dev_get_ctrlfd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_mtu = mtu;

	if (ioctl(s, SIOCSIFMTU, &ifr) < 0) {
		perror("SIOCSIFMTU");
		close(s);
		return -1;
	}

	close(s);

	return 0;
}

int librouter_dev_get_mtu(char *dev)
{
	struct ifreq ifr;
	int s;

	s = _librouter_dev_get_ctrlfd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFMTU, &ifr) < 0) {
		perror("SIOCGIFMTU");
		close(s);
		return -1;
	}

	close(s);

	return ifr.ifr_mtu;
}

int librouter_dev_set_link_down(char *dev)
{
	int ret = 0;
	dev_family *fam = librouter_device_get_family_by_name(dev, str_linux);

	switch (fam->type) {
#ifdef OPTION_MODEM3G
	case ppp:
		librouter_ppp_backupd_set_param_infile(dev,SHUTD_STR,"yes");
		ret = librouter_ppp_reload_backupd();
		break;
#endif
	default:
		ret = _librouter_dev_chflags(dev, 0, IFF_UP);
		break;
	}

	return ret;

}

int librouter_dev_set_link_up(char *dev)
{
	int ret = 0;
	dev_family *fam = librouter_device_get_family_by_name(dev, str_linux);

	switch (fam->type) {
#ifdef OPTION_MODEM3G
	case ppp:
		librouter_ppp_backupd_set_param_infile(dev,SHUTD_STR,"no");
		ret = librouter_ppp_reload_backupd();
		break;
#endif
	default:
		ret = _librouter_dev_chflags(dev, IFF_UP, IFF_UP);
		break;
	}

	return ret;
}

int librouter_dev_get_link(char *dev)
{
	u32 flags;

	if (librouter_dev_get_flags(dev, &flags) < 0)
		return (-1);

	return (flags & IFF_UP);
}

int librouter_dev_get_link_running(char *dev)
{
	u32 flags;

	if (librouter_dev_get_flags(dev, &flags) < 0)
		return (-1);

	return (flags & IFF_RUNNING);
}


int librouter_dev_get_hwaddr(char *dev, unsigned char *hwaddr)
{
	struct ifreq ifr;
	int skfd;

	if (hwaddr == NULL)
		return 0;

	/* Create a socket to the INET kernel. */
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (-1);
	}

	strcpy(ifr.ifr_name, dev);
	ifr.ifr_addr.sa_family = AF_INET;

	if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
		memset(hwaddr, 0, 6);
	else
		memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, 6);

	close(skfd);

	return 0;
}

int librouter_dev_change_name(char *ifname, char *new_ifname)
{
	struct ifreq ifr;
	int fd;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (-1);
	}

	strcpy(ifr.ifr_name, ifname);
	strcpy(ifr.ifr_newname, new_ifname);

	if (ioctl(fd, SIOCSIFNAME, &ifr)) {
		librouter_pr_error(1, "%s: SIOCSIFNAME", ifname);
		close(fd);
		return (-1);
	}

	close(fd);

	return 0;
}

int librouter_dev_exists(char *dev)
{
	struct ifreq ifr;
	int fd, found;

	/* Create a socket to the INET kernel. */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (0);
	}

	strcpy(ifr.ifr_name, dev);

	found = (ioctl(fd, SIOCGIFFLAGS, &ifr) >= 0);

	close(fd);

	return found;
}

int librouter_clear_interface_counters(char *dev)
{
#if 0
	struct ifreq ifr;
	int fd, err;

	if ((fd=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		librouter_pr_error(1, "socket");
		return (0);
	}

	strcpy(ifr.ifr_name, dev);
	err=ioctl(fd, SIOCCLRIFSTATS, &ifr);

	if ((err < 0) && (errno != ENODEV)) {
		librouter_pr_error(1, "SIOCCLRIFSTATS");
	}

	close(fd);

	return err;
#else
	return 0;
#endif
}

char *librouter_dev_get_description(char *dev)
{
	FILE *f;
	char file[240];
	long len;
	char *description;

	sprintf(file, DESCRIPTION_FILE, dev);

	f = fopen(file, "rt");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len == 0) {
		fclose(f);
		return NULL;
	}

	description = malloc(len + 1);

	if (description == 0) {
		fclose(f);
		return NULL;
	}

	fgets(description, len + 1, f);
	fclose(f);

	return description;
}

int librouter_dev_add_description(char *dev, char *description)
{
	FILE *f;
	char file[50];

	sprintf(file, DESCRIPTION_FILE, dev);
	f = fopen(file, "wt");

	if (!f)
		return -1;

	fputs(description, f);
	fclose(f);

	return 0;
}

int librouter_dev_del_description(char *dev)
{
	char file[50];

	sprintf(file, DESCRIPTION_FILE, dev);
	unlink(file);

	return 0;
}

/* arp */
/*
 arp -s x.x.x.x Y:Y:Y:Y:Y:Y:Y:Y

 ip neigh add x.x.x.x lladdr H.H.H dev ifname
 ip neigh show
 iproute2/ip/ipneigh.c: ipneigh_modify()
 net-tools/arp.c

 arp 1.1.1.1 ?
 H.H.H 48-bit hardware address of ARP entry
 */

/* Delete an entry from the ARP cache. */
/* <ipaddress> */
int librouter_arp_del(char *host)
{
	struct arpreq req;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	int flags = 0;
	int err;
	int sockfd = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}

	memset((char *) &req, 0, sizeof(req));
	sin = (struct sockaddr_in *) &sa;
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	if (inet_aton(host, &sin->sin_addr) == 0) {
		return (-1);
	}

	memcpy((char *) &req.arp_pa, (char *) &sa, sizeof(struct sockaddr));
	req.arp_flags = ATF_PERM;

	if (flags == 0)
		flags = 3;
	strcpy(req.arp_dev, "");
	err = -1;

	/* Call the kernel. */
	if (flags & 2) {
		if ((err = ioctl(sockfd, SIOCDARP, &req) < 0)) {
			if (errno == ENXIO) {
				if (flags & 1)
					goto nopub;
				printf("%% no ARP entry for %s\n", host);
				return (-1);
			}

			perror("%% error"); /* SIOCDARP(priv) */
			return (-1);
		}
	}

	if ((flags & 1) && (err)) {
		nopub: req.arp_flags |= ATF_PUBL;
		if (ioctl(sockfd, SIOCDARP, &req) < 0) {
			if (errno == ENXIO) {
				printf("%% no ARP entry for %s\n", host);
				return (-1);
			}
			printf("%% no ARP entry for %s\n", host);
#if 0
			perror("SIOCDARP(pub)");
#endif
			return (-1);
		}
	}

	return (0);
}

/* Input an Ethernet address and convert to binary. */
static int _librouter_arp_in_ether(char *bufp, struct sockaddr *sap)
{
	unsigned char *ptr;
	char c, *orig;
	int i;
	unsigned val;

	sap->sa_family = ARPHRD_ETHER;
	ptr = (unsigned char *) sap->sa_data;

	i = 0;
	orig = bufp;
	while ((*bufp != '\0') && (i < ETH_ALEN)) {
		val = 0;
		c = *bufp++;

		if (isdigit(c)) {
			val = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			val = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			val = c - 'A' + 10;
		} else {
			errno = EINVAL;
			return (-1);
		}

		val <<= 4;
		c = *bufp;

		if (isdigit(c)) {
			val |= c - '0';
		} else if (c >= 'a' && c <= 'f') {
			val |= c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			val |= c - 'A' + 10;
		} else if (c == ':' || c == 0) {
			val >>= 4;
		} else {
			errno = EINVAL;
			return (-1);
		}

		if (c != 0)
			bufp++;

		*ptr++ = (unsigned char) (val & 0377);
		i++;

		/* We might get a semicolon here - not required. */
		if (*bufp == ':') {
			if (i == ETH_ALEN) {
				; /* nothing */
			}
			bufp++;
		}
	}

	/* That's it.  Any trailing junk? */
	if ((i == ETH_ALEN) && (*bufp != '\0')) {
		errno = EINVAL;
		return (-1);
	}

	return (0);
}

/* Set an entry in the ARP cache. */
/* <ipaddress> <mac> */
int librouter_arp_add(char *host, char *mac)
{
	struct arpreq req;
	struct sockaddr sa;
	struct sockaddr_in *sin;
	int flags;
	int sockfd = 0;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}

	memset((char *) &req, 0, sizeof(req));
	sin = (struct sockaddr_in *) &sa;
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	if (inet_aton(host, &sin->sin_addr) == 0) {
		return (-1);
	}

	memcpy((char *) &req.arp_pa, (char *) &sa, sizeof(struct sockaddr));

	if (_librouter_arp_in_ether(mac, &req.arp_ha) < 0) {
		printf("%% invalid hardware address\n");
		return (-1);
	}

	/* Check out any modifiers. */
	flags = ATF_PERM | ATF_COM;

	/* Fill in the remainder of the request. */
	req.arp_flags = flags;
	strcpy(req.arp_dev, "");

	/* Call the kernel. */
	if (ioctl(sockfd, SIOCSARP, &req) < 0) {
		perror("%% error");
		return (-1);
	}

	return (0);
}

int librouter_dev_shutdown(char *dev)
{
	librouter_qos_tc_remove_all(dev);
	librouter_dev_set_link_down(dev);
	return 0;

}

int librouter_dev_noshutdown(char *dev)
{
	int major=0;
	dev_family *fam = librouter_device_get_family_by_name(dev, str_linux);

	if (librouter_dev_set_link_up(dev) < 0)
		return -1;

	if (fam == NULL)
		return 0;

	major = librouter_device_get_major(dev, str_linux);

	switch (fam->type) {
	case eth:
		librouter_udhcpd_reload(major); /* dhcp integration! force reload ethernet address */
		librouter_qos_tc_insert_all(dev);
		break;
	default:
		break;
	}

#ifdef OPTION_SMCROUTE
	librouter_smc_route_hup();
#endif
	return 0;
}

int notify_driver_about_shutdown(char *dev)
{
#if 0
	int sock;
	struct ifreq ifr;

	if( !dev || !strlen(dev) )
		return -1;

	strcpy(ifr.ifr_name, dev);

	/* Create a channel to the NET kernel. */
	if( (sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0 ) {
		librouter_pr_error(1, "if_shutdown_alert: socket");
		return (-1);
	}

	ifr.ifr_settings.type = IF_SHUTDOWN_ALERT;

	/* use linux/drivers/net/wan/hdlc_sppp.c */
	if( ioctl(sock, SIOCWANDEV, &ifr) ) {
		librouter_pr_error(1, "if_shutdown_alert: ioctl");
		return (-1);
	}

	close(sock);

	return 0;
#else
	return 0;
#endif
}

#ifdef CONFIG_BUFFERS_USE_STATS
int librouter_dev_interface_get_buffers_use(char *dev,
                                            char *tx,
                                            char *rx,
                                            unsigned int len)
{
	char *p;
	int sock;
	struct ifreq ifr;
	unsigned int total;
	struct _buffers_use info;

	if( dev == NULL )
		return -1;

	/* Somente buscamos as estatisticas das interfaces que realmente
	 * usam buffers fisicos de transmissao e recepcao do processador.
	 */
	if (((strncmp(dev, "ethernet", strlen("ethernet")) != 0) &&
			(strncmp(dev, "serial", strlen("serial")) != 0))
			|| (strchr(dev, '.') != NULL)
			|| (strchr(dev, ':') != NULL)) {

		if (tx != NULL)
			snprintf(tx, len, "%#5.1f%%", (float) 0);

		if (rx != NULL)
			snprintf(rx, len, "%#5.1f%%", (float) 0);

		return 0;
	}

	strcpy(ifr.ifr_name, dev);
	if (strncmp(dev, "ethernet", strlen("ethernet")) == 0) {
		if ((p = strchr(ifr.ifr_name, '.')) != NULL)
			*p = 0; /* vlan uses ethernetX status! */
	}

	/* Create a channel to the NET kernel. */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		perror("socket error");
		return -1;
	}

	ifr.ifr_settings.size = sizeof(struct _buffers_use);
	ifr.ifr_settings.ifs_ifsu.buffers_use = &info;

	if (ioctl(sock, SIOCGBUFUSESTATS, &ifr) < 0) {
		perror("ioctl error: SIOCGBUFUSESTATS");
		close(sock);
		return -1;
	}

	close(sock);

	/* TX */
	if (tx != NULL) {
		total = info.tx_used + info.tx_unused;
		snprintf(tx, len, "%#5.1f%%", (float) ((info.tx_used * 100.0) / ((total > 0) ? total : 1)));
	}

	/* RX */
	if (rx != NULL) {
		total = info.rx_used + info.rx_unused;
		snprintf(rx, len, "%#5.1f%%", (float) ((info.rx_used * 100.0) / ((total > 0) ? total : 1)));
	}

	return 0;
}
#endif /* CONFIG_BUFFERS_USE_STATS */

#ifdef CONFIG_DEVELOPMENT
int librouter_dev_set_rxring(char *dev, int size)
{
	int s;
	struct ifreq ifr;

	if ((s = _librouter_dev_get_ctrlfd()) < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFRXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFRXRING");
		return -1;
	}

	if (ifr.ifr_ifru.ifru_ivalue != size) {
		ifr.ifr_ifru.ifru_ivalue = size;
		if (ioctl(s, SIOCSIFRXRING, &ifr) < 0) {
			close(s);
			//perror("% SIOCSIFRXRING");
			printf("%% shutdown interface first\n");
			return -1;
		}
	}

	close(s);

	return 0;
}

int librouter_dev_get_rxring(char *dev)
{
	int s;
	struct ifreq ifr;

	if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFRXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFRXRING");
		return -1;
	}

	close(s);

	return ifr.ifr_ifru.ifru_ivalue;
}

int librouter_dev_set_txring(char *dev, int size)
{
	int s;
	struct ifreq ifr;

	if ((s = _librouter_dev_get_ctrlfd()) < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFTXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFRXRING");
		return -1;
	}

	if (ifr.ifr_ifru.ifru_ivalue != size) {
		ifr.ifr_ifru.ifru_ivalue = size;
		if (ioctl(s, SIOCSIFTXRING, &ifr) < 0) {
			close(s);
			//perror("% SIOCSIFTXRING");
			printf("%% shutdown interface first\n");
			return -1;
		}
	}

	close(s);

	return 0;
}

int librouter_dev_get_txring(char *dev)
{
	int s;
	struct ifreq ifr;

	if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFTXRING, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFTXRING");
		return -1;
	}

	close(s);

	return ifr.ifr_ifru.ifru_ivalue;
}

int librouter_dev_set_weight(char *dev, int size)
{
	int s;
	struct ifreq ifr;

	if ((s = _librouter_dev_get_ctrlfd()) < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);
	ifr.ifr_ifru.ifru_ivalue = size;

	if (ioctl(s, SIOCSIFWEIGHT, &ifr) < 0) {
		close(s);
		perror("% SIOCSIFWEIGHT");
		return -1;
	}

	close(s);

	return 0;
}

int librouter_dev_get_weight(char *dev)
{
	int s;
	struct ifreq ifr;

	if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);

	if (ioctl(s, SIOCGIFWEIGHT, &ifr) < 0) {
		close(s);
		perror("% SIOCGIFWEIGHT");
		return -1;
	}

	close(s);

	return ifr.ifr_ifru.ifru_ivalue;
}
#endif /* CONFIG_DEVELOPMENT */

