#include "defines.h"
#include "typedefs.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/netdevice.h>

#define DESCRIPTION_DIR "/var/run/description/"
#define DESCRIPTION_FILE DESCRIPTION_DIR"%s"

int dev_get_flags(char *dev, __u32 *flags);
int dev_set_qlen(char *dev, int qlen);
int dev_get_qlen(char *dev);
int dev_set_mtu(char *dev, int mtu);
int dev_get_mtu(char *dev);
int dev_set_link_down(char *dev);
int dev_set_link_up(char *dev);
int dev_get_link(char *dev);
int dev_get_hwaddr(char *dev, unsigned char *hwaddr);
int dev_change_name(char *ifname, char *new_ifname);
int dev_exists(char *dev_name);
char *dev_get_description(char *dev);
int dev_add_description(char *dev, char *description);
int dev_del_description(char *dev);
int clear_interface_counters(char *dev);
int arp_del(char *host);
int arp_add(char *host, char *mac);
int notify_driver_about_shutdown(char *dev);

int get_interface_buffers_use(char *, char *, char *, unsigned int);

int dev_set_rxring(char *, int);
int dev_get_rxring(char *);
int dev_set_txring(char *, int);
int dev_get_txring(char *);
int dev_set_weight(char *, int);
int dev_get_weight(char *);
