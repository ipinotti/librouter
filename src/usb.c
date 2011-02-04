/*
 * usb.c
 *
 *  Created on: Jun 9, 2010
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/autoconf.h>

#include <libusb/libusb.h>

#include "options.h"
#include "defines.h"
#include "ppp.h"
#include "usb.h"

/* type, port[NUMBER_OF_USBPORTS]*/
#if defined(CONFIG_DIGISTAR_3G)
port_family_usb _ports[] = {
	{ real,  {1,2,3} },
	{ alias, {1,2,0} },
	{ non,  {0,0,0} }
};
#elif defined(CONFIG_DIGISTAR_EFM)
port_family_usb _ports[] = {
	{ real,  {1} },
	{ alias, {0} },
	{ non,  {0} }
};
#endif
/**
 * Função retorna porta "alias" utilizada no cish e backupd através da
 * porta usb real correspondente passada por parâmetro
 *
 * @param port
 * @return aliasport if ok, -1 if not
 */
int librouter_usb_get_aliasport_by_realport(int port)
{
	int i;

	for (i=0; i < NUMBER_OF_USBPORTS; i++){
		if ( _ports[real].port[i] == port ){
			return _ports[alias].port[i];
		}
	}
	return -1;
}

/**
 * Função retorna porta usb correspondente através da porta "alias"
 * utilizada no cish e backupd
 *
 *
 * @param port
 * @return hubport if ok, -1 if not
 */
int librouter_usb_get_realport_by_aliasport(int port)
{
	int i;

	for (i=0; i < NUMBER_OF_USBPORTS; i++){
		if ( _ports[alias].port[i] == port ){
			return _ports[real].port[i];
		}
	}
	return -1;
}

/**
 * Função utilizada para verificar a existencia de dispositivo USB conectado a uma porta X
 * do HUB_USB do sistema
 *
 * É passado por parâmetro a porta desejada para verificação da existencia de dispositivo conectado
 * Retorna 1 caso exista dispositivo conectado
 * Retorna 0 caso não exista dispotivo conectado
 *
 * OBS: a verificação da existencia de disp. USB conectado é feita através da análise de diretórios
 * criados segundo a porta do HUB_USB, apontado pelo ADDR_USB
 *
 * @param port
 * @return 1 if device is connected, 0 if not
 */
int librouter_usb_device_is_connected(int port)
{
	struct DIR * target = NULL;
	char port_usb_num[2];
	int result = 0;
	char buff_addr[sizeof(ADDR_USB)];

#if defined(CONFIG_DIGISTAR_3G)
	sprintf(buff_addr, ADDR_USB, HUB_PORT, HUB_PORT, port);
#elif defined(CONFIG_DIGISTAR_EFM)
	sprintf(buff_addr, ADDR_USB, HUB_PORT, HUB_PORT);
#endif

	usb_dbg("Device address : %s\n", buff_addr);

	target = (struct DIR *) opendir(buff_addr);

	if (target != NULL)
		result = 1;

	closedir(target);

	return result;
}

/**
 * Função utilizada para verificar se o dispositivo USB conectado a uma porta X
 * do HUB_USB é um modem 3G.
 *
 * É passado por parâmetro a porta desejada para verificação da existencia de dispositivo conectado
 * Retorna numero do ttyUSB atribuido ao dispositivo caso o mesmo for um modem 3G
 * Retorna -1 se não houver dispositivo conectado ou não for um modem 3G
 *
 * OBS: a verificação da existencia de disp. USB conectado é feita através da análise de diretórios
 * criados segundo a porta do HUB_USB
 * @param port
 * @return number of ttyUSB if ok, -1 if not
 */
int librouter_usb_device_is_modem(int port)
{
	struct DIR * target = NULL;
	char buff_addr[sizeof(ADDR_PORT_USB)];
	int result = -1, i = 0;

	if (!librouter_usb_device_is_connected(port)) {
		goto res;
	}

	for (i = 0; i < 15; i++) {
#if defined(CONFIG_DIGISTAR_3G)
		sprintf(buff_addr, ADDR_PORT_USB, HUB_PORT, HUB_PORT, port, HUB_PORT, HUB_PORT, port, i);
#elif defined(CONFIG_DIGISTAR_EFM)
		sprintf(buff_addr, ADDR_PORT_USB, HUB_PORT, port, HUB_PORT, port, i);
#endif
		usb_dbg("Device address : %s\n", buff_addr);

		target = (struct DIR *) opendir(buff_addr);

		if (target != NULL) {
			result = i;
			closedir(target);
			break;
		}

		closedir(target);
	}

res:
	return result;

}

/**
 * Função utilizada para adquirir informações referentes ao dispositivo USB conectado ao sistema
 * É passado por parâmetro uma struct (lusb_dev), a qual é preenchida com informações retiradas do
 * dispositivo USB através da libusb
 *
 * Retorna 0 se a função conseguiu adquirir as informações da porta USB apontada
 * Retorna -1 caso não consiga adquirir as informações, devido a inexistencia do dispositivo apontado
 * @param usb
 * @return 0 if OK, -1 if not
 */
int librouter_usb_get_descriptor(librouter_usb_dev * usb)
{

	FILE * file;
	libusb_device *dev;
	libusb_device **devs;
	char id_prod_usb_ffile[64];
	char addr_file[64];
	char conv_temp[64];
	int err = 0, i = 0, id_prod_usb_ffc = 0, conv_id_prod = 0;
	struct libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;

#if defined(CONFIG_DIGISTAR_3G)
	sprintf(addr_file, ADDR_USB_IDPRODUCT, HUB_PORT, HUB_PORT, usb->port);
#elif defined(CONFIG_DIGISTAR_EFM)
	sprintf(addr_file, ADDR_USB_IDPRODUCT, HUB_PORT, usb->port);
#endif
	file = fopen(addr_file, "rt");
	if (!file)
		return -1;

	fgets(id_prod_usb_ffile, 64, file);
	fclose(file);

	id_prod_usb_ffc = atoi(id_prod_usb_ffile);

	err = libusb_init(NULL);
	if (err < 0) {
		fprintf(stderr, "Failed to libusb_init\n");
		return;
	}
	err = libusb_get_device_list(NULL, &devs);
	if (err < 0) {
		fprintf(stderr, "Failed to libusb_get_device_list\n");
		return;
	}

	while ((dev = devs[i++]) != NULL) {
		err = libusb_get_device_descriptor(dev, &desc);
		if (err < 0) {
			fprintf(stderr, "Failed to get device descriptor\n");
			return;
		}

		sprintf(conv_temp, "%04x", desc.idProduct);
		conv_id_prod = atoi(conv_temp);

		if ((librouter_usb_device_is_connected(usb->port))
		                && (desc.idVendor != IDVENDOR_LINUX_HUB)
		                && (id_prod_usb_ffc == conv_id_prod)) {

			err = libusb_open(dev, &handle);

			if (err < 0) {
				printf("Coudn't open handle\n");
				libusb_close(handle);
				return;
			}

			err = libusb_get_string_descriptor_ascii(handle, desc.iProduct, usb->product_str, sizeof(usb->product_str));
			if (err < 0) {
				printf("Coudn't get iProduct string\n");
				libusb_close(handle);
				return;
			}

			err = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, usb->manufacture_str, sizeof(usb->manufacture_str));
			if (err < 0) {
				printf("Coudn't get iManufacturer string\n");
				libusb_close(handle);
				return;
			}

			usb->bus = libusb_get_bus_number(dev);
			usb->product_id = desc.iProduct;
			usb->vendor_id = desc.idVendor;

			libusb_close(handle);
			libusb_free_device_list(devs, 1);
			libusb_exit(NULL);
			return 0;
		}
	}

	libusb_close(handle);
	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
	return -1;

}

