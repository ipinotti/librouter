
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

#include "typedefs.h"
#include "vlan.h"
#include "ip.h"
#include "dev.h"
#include "device.h"
#include "error.h"
#include "defines.h"
#include "quagga.h"

int librouter_vlan_exists(int ethernet_no, int vid)
{
	char ifname[IFNAMSIZ];
	dev_family *fam = librouter_device_get_family_by_type(eth);


	sprintf(ifname, "%s%d.%d",fam->linux_string, ethernet_no, vid);
	return (librouter_dev_exists(ifname));
}

int librouter_vlan_add_vid(int ethernet_no, int vid)
{
	struct vlan_ioctl_args if_request;
	int sock;
	dev_family *fam = librouter_device_get_family_by_type(eth);

	if ((vid < 2) || (vid > 4094)) {
		librouter_pr_error(0, "vlan: invalid vid: %d", vid);
		return (-1);
	}

	/* Create a channel to the NET kernel. */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		librouter_pr_error(1, "vlan: socket");
		return (-1);
	}

	if_request.u.VID = vid;
	sprintf(if_request.device1, "%s%d", fam->linux_string, ethernet_no);
	if_request.cmd = ADD_VLAN_CMD;

	if (ioctl(sock, SIOCSIFVLAN, &if_request) < 0) {
		librouter_pr_error(1, "vlan: unable to create vlan");
		close(sock);
		return (-1);
	}

	close(sock);

	/* Tell quagga to monitor link on this interface */
	sprintf(if_request.device1, "%s%d.%d", fam->linux_string, ethernet_no, vid);
	librouter_quagga_add_link_detect(if_request.device1);

	return 0;
}

int librouter_vlan_del_vid(int ethernet_no, int vid)
{
	struct vlan_ioctl_args if_request;
	int sock;
	dev_family *fam = librouter_device_get_family_by_type(eth);

	if ((vid < 2) || (vid > 4094)) {
		librouter_pr_error(0, "vlan: invalid vid: %d", vid);
		return (-1);
	}

	/* Create a channel to the NET kernel. */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		librouter_pr_error(1, "vlan: socket");
		return (-1);
	}

	if_request.u.VID = vid;
	sprintf(if_request.device1, "%s%d.%d", fam->linux_string, ethernet_no, vid);
	if_request.cmd = DEL_VLAN_CMD;

	if (ioctl(sock, SIOCSIFVLAN, &if_request) < 0) {
		librouter_pr_error(1, "vlan: unable to delete vlan");
		close(sock);
		return (-1);
	}

	close(sock);

	return 0;
}

#if 0
/* Class of Service */
int librouter_vlan_set_cos(int ethernet_no, int vid, int cos)
{
	struct vlan_ioctl_args if_request;
	int sock;
	dev_family *fam = librouter_device_get_family_by_type(eth);

	if ((vid < 2) || (vid > 4094)) {
		librouter_pr_error(0, "vlan: invalid vid: %d", vid);
		return (-1);
	}

	/* Create a channel to the NET kernel. */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		librouter_pr_error(1, "vlan: socket");
		return (-1);
	}

	if_request.u.cos = cos;
	if_request.cmd = SET_VLAN_EGRESS_PRIO_MAP_CMD;
	sprintf(if_request.device1, "%s%d.%d", fam->linux_string, ethernet_no, vid);

	if (ioctl(sock, SIOCSIFVLAN, &if_request) < 0) {
		librouter_pr_error(1, "vlan: ioctl");
		close(sock);
		return (-1);
	}

	close(sock);
	return 0;
}

int librouter_vlan_get_cos(char *dev_name)
{
	struct vlan_ioctl_args if_request;
	int sock;

	/* Create a channel to the NET kernel. */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		librouter_pr_error(1, "vlan: socket");
		return (-1);
	}

	if_request.cmd = GET_VLAN_EGRESS_PRIO_MAP_CMD;
	strncpy(if_request.device1, (char *) dev_name,
	                sizeof(if_request.device1));

	if (ioctl(sock, SIOCSIFVLAN, &if_request) < 0) {
		librouter_pr_error(1, "vlan: ioctl");
		close(sock);
		return (-1);
	}

	close(sock);
	return if_request.u.cos;
}
#endif
