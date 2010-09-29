/*
 * usb.h
 *
 *  Created on: Jun 9, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef USB_H_
#define USB_H_

#define HUB_PORT 1
#define ADDR_USB "/sys/bus/usb/devices/%d-%d.%d"
#define ADDR_PORT_USB "/sys/bus/usb/devices/%d-%d.%d/%d-%d.%d:1.0/ttyUSB%d"
#define ADDR_USB_IDPRODUCT "/sys/bus/usb/devices/%d-%d.%d/idProduct"

#define IDVENDOR_LINUX_HUB 7531
#define USB_STR_SIZE 256
#define NUMBER_OF_USBPORTS 3 /*3 portas USB reais*/

typedef enum {
	real, alias, non
} port_type;

typedef struct {
	port_type type;
	const int port[NUMBER_OF_USBPORTS];
} port_family_usb;

extern port_family_usb _ports[];


typedef struct {
	int port;
	int bus;
	uint16_t vendor_id;
	uint16_t product_id;
	char product_str[USB_STR_SIZE];
	char manufacture_str[USB_STR_SIZE];

} librouter_usb_dev;

int librouter_usb_device_is_connected(int port);
int librouter_usb_get_descriptor(librouter_usb_dev * usb);
int librouter_usb_device_is_modem(int port);
int librouter_usb_get_aliasport_by_realport(int port);
int librouter_usb_get_realport_by_aliasport(int port);


#endif /* USB_H_ */
