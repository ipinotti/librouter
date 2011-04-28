/*
 * dev.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef LIBROUTER_DEV_H_
#define LIBROUTER_DEV_H_

#define DESCRIPTION_DIR "/var/run/description/"
#define DESCRIPTION_FILE DESCRIPTION_DIR"%s"

#include <linux/autoconf.h>
#include "device.h"

//#define DEV_DEBUG
#ifdef DEV_DEBUG
#define dev_dbg(x,...) \
		syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define dev_dbg(x,...)
#endif

int librouter_dev_get_flags(char *dev, unsigned int *flags);
int librouter_dev_set_qlen(char *dev, int qlen);
int librouter_dev_get_qlen(char *dev);
int librouter_dev_set_mtu(char *dev, int mtu);
int librouter_dev_get_mtu(char *dev);
int librouter_dev_set_link_down(char *dev);
int librouter_dev_set_link_up(char *dev);
int librouter_dev_get_link(char *dev);
int librouter_dev_get_link_running(char *dev);
int librouter_dev_get_hwaddr(char *dev, unsigned char *hwaddr);
int librouter_dev_change_name(char *ifname, char *new_ifname);
int librouter_dev_exists(char *dev_name);
char *librouter_dev_get_description(char *dev);
int librouter_dev_add_description(char *dev, char *description);
int librouter_dev_del_description(char *dev);
int librouter_clear_interface_counters(char *dev);
int librouter_arp_del(char *host);
int librouter_arp_add(char *host, char *mac);
int notify_driver_about_shutdown(char *dev);

int librouter_dev_shutdown(char *dev, dev_family *fam);
int librouter_dev_noshutdown(char *dev, dev_family *fam);

#ifdef CONFIG_BUFFERS_USE_STATS
int librouter_dev_interface_get_buffers_use(char *dev,
                                            char *tx,
                                            char *rx,
                                            unsigned int len);
#endif

#ifdef CONFIG_DEVELOPMENT
int librouter_dev_set_rxring(char *dev, int size);
int librouter_dev_get_rxring(char *dev);
int librouter_dev_set_txring(char *dev, int size);
int librouter_dev_get_txring(char *dev);
int librouter_dev_set_weight(char *dev, int size);
int librouter_dev_get_weight(char *dev);
#endif

#endif /* LIBROUTER_DEV_H_ */
