/*
 * usb.h
 *
 *  Created on: Jun 9, 2010
 *      Author: ipinotti
 */

#ifndef USB_H_
#define USB_H_

typedef struct {
	int port;
	int bus;
	uint16_t idVendor;
	uint16_t idProduct;
	char iProduct_string[256];
	char iManufacture_string[256];

} lusb_dev;

int lusb_check_usb_connected (int port);
int lusb_get_descriptor (lusb_dev * usb);
int lusb_check_usb_ttyUSB (int port);


#endif /* USB_H_ */
