/*
 * dev.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef DEV_H_
#define DEV_H_

#define DESCRIPTION_DIR "/var/run/description/"
#define DESCRIPTION_FILE DESCRIPTION_DIR"%s"

int libconfig_dev_get_flags(char *dev, unsigned int *flags);
int libconfig_dev_set_qlen(char *dev, int qlen);
int libconfig_dev_get_qlen(char *dev);
int libconfig_dev_set_mtu(char *dev, int mtu);
int libconfig_dev_get_mtu(char *dev);
int libconfig_dev_set_link_down(char *dev);
int libconfig_dev_set_link_up(char *dev);
int libconfig_dev_get_link(char *dev);
int libconfig_dev_get_hwaddr(char *dev, unsigned char *hwaddr);
int libconfig_dev_change_name(char *ifname, char *new_ifname);
int libconfig_dev_exists(char *dev_name);
char *libconfig_dev_get_description(char *dev);
int libconfig_dev_add_description(char *dev, char *description);
int libconfig_dev_del_description(char *dev);
int libconfig_clear_interface_counters(char *dev);
int libconfig_arp_del(char *host);
int libconfig_arp_add(char *host, char *mac);
int notify_driver_about_shutdown(char *dev);

int libconfig_dev_shutdown(char *dev);
int libconfig_dev_noshutdown(char *dev);

#ifdef CONFIG_BUFFERS_USE_STATS
int libconfig_dev_interface_get_buffers_use(char *dev,
                                            char *tx,
                                            char *rx,
                                            unsigned int len);
#endif

#ifdef CONFIG_DEVELOPMENT
int libconfig_dev_set_rxring(char *dev, int size);
int libconfig_dev_get_rxring(char *dev);
int libconfig_dev_set_txring(char *dev, int size);
int libconfig_dev_get_txring(char *dev);
int libconfig_dev_set_weight(char *dev, int size);
int libconfig_dev_get_weight(char *dev);
#endif

#endif /* DEV_H_ */
