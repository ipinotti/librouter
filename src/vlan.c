#include <linux/config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <asm/types.h>
#include <linux/hdlc.h>
#include <netinet/in.h>
#include <linux/netdevice.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/if_vlan.h>
#include <libconfig/typedefs.h>
#include <libconfig/vlan.h>
#include <libconfig/ip.h>
#include <libconfig/dev.h>
#include <libconfig/error.h>
#include <libconfig/defines.h>

int vlan_exists(int ethernet_no, int vid)
{
	char ifname[IFNAMSIZ];

	sprintf(ifname, "ethernet%d.%d", ethernet_no, vid);
	return (dev_exists(ifname));
}

int vlan_vid(int ethernet_no, int vid, int add_del, int bridge)
{
	struct vlan_ioctl_args if_request;
	int sock;

	if ((vid < 2) || (vid > 4094))
	{
		pr_error(0, "vlan: invalid vid: %d", vid);
		return (-1);
	}
	/* Create a channel to the NET kernel. */
	if ((sock=socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		pr_error(1, "vlan: socket");
		return (-1);
	}
	if_request.u.VID=vid;
	if (add_del)
	{
		sprintf(if_request.device1, "ethernet%d", ethernet_no);
		if_request.cmd = ADD_VLAN_CMD;
	}
	else
	{
		sprintf(if_request.device1, "ethernet%d.%d", ethernet_no, vid);
		if_request.cmd = DEL_VLAN_CMD;
	}
	if (ioctl(sock, SIOCSIFVLAN, &if_request) < 0)
	{
		pr_error(1, "vlan: unable to %s vlan", add_del ? "create" : "delete");
		close(sock);
		return (-1);
	}
	close(sock);
	return 0;
}


#if 0
/* Class of Service */
int set_vlan_cos(int ethernet_no, int vid, int cos)
{
	struct vlan_ioctl_args if_request;
	int sock;

	if ((vid < 2) || (vid > 4094)) {
		pr_error(0, "vlan: invalid vid: %d", vid);
		return (-1);
	}
	/* Create a channel to the NET kernel. */
	if ((sock=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		pr_error(1, "vlan: socket");
		return (-1);
	}

	if_request.u.cos=cos;
	if_request.cmd = SET_VLAN_EGRESS_PRIO_MAP_CMD;
	sprintf(if_request.device1, "ethernet%d.%d", ethernet_no, vid);

	if (ioctl(sock, SIOCSIFVLAN, &if_request) < 0)	{
		pr_error(1, "vlan: ioctl");
		close(sock);
		return (-1);
	}

	close(sock);
	return 0;
}

int get_vlan_cos(char *dev_name)
{
	struct vlan_ioctl_args if_request;
	int sock;

	/* Create a channel to the NET kernel. */
	if ((sock=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		pr_error(1, "vlan: socket");
		return (-1);
	}

	if_request.cmd = GET_VLAN_EGRESS_PRIO_MAP_CMD;
	strncpy(if_request.device1, (char *)dev_name, sizeof(if_request.device1));
	if (ioctl(sock, SIOCSIFVLAN, &if_request) < 0)	{
		pr_error(1, "vlan: ioctl");
		close(sock);
		return (-1);
	}

	close(sock);
	return if_request.u.cos;
}
#endif
