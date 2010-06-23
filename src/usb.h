/*
 * usb.h
 *
 *  Created on: Jun 9, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef USB_H_
#define USB_H_

#define HUB_PORT 2
#define ADDR_USB "/sys/bus/usb/devices/%d-%d"
#define ADDR_PORT_USB "/sys/bus/usb/devices/%d-%d/%d-%d:1.0/ttyUSB%d"
#define ADDR_USB_IDPRODUCT "/sys/bus/usb/devices/2-%d/idProduct"
#define ADDR_teste "/sys/bus/usb/devices/2-1/idProduct"

#define IDVENDOR_LINUX_HUB 7531

typedef struct {
	int port;
	int bus;
	uint16_t vendor_id;
	uint16_t product_id;
	char product_str[256];
	char manufacture_str[256];

} libconfig_usb_dev;

int libconfig_usb_device_is_connected(int port);
int libconfig_usb_get_descriptor(libconfig_usb_dev * usb);
int libconfig_usb_device_is_modem(int port);

#endif /* USB_H_ */
