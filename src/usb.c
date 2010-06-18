/*
 * usb.c
 *
 *  Created on: Jun 9, 2010
 *      Author: ipinotti
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
#include <libusb/libusb.h>
#include "options.h"
#include "defines.h"
#include "ppp.h"
#include "usb.h"

#define HUB_PORT 2
#define ADDR_USB "/sys/bus/usb/devices/%d-%d"
#define ADDR_PORT_USB "/sys/bus/usb/devices/%d-%d/%d-%d:1.0/ttyUSB%d"
#define ADDR_USB_IDPRODUCT "/sys/bus/usb/devices/2-%d/idProduct"
#define ADDR_teste "/sys/bus/usb/devices/2-1/idProduct"

#define IDVENDOR_LINUX_HUB 7531

/**
 * Função utilizada para verificar a existencia de dispositivo USB conectado a uma porta X
 * do HUB_USB do sistema
 *
 * É passado por parâmetro a porta desejada para verificação da existencia de dispositivo conectado
 * Retorna 1 caso exista dispositivo conectado
 * Retorna -1 caso não exista dispotivo conectado
 *
 * OBS: a verificação da existencia de disp. USB conectado é feita através da análise de diretórios
 * criados segundo a porta do HUB_USB, apontado pelo ADDR_USB
 * @param port
 * @return
 */
int lusb_check_usb_connected (int port){
	struct DIR * target=NULL;
	char port_usb_num[2];
	int result=0;
	char buff_addr[sizeof(ADDR_USB)];
	sprintf (buff_addr, ADDR_USB, HUB_PORT, port);

	target = (struct DIR *)opendir(buff_addr);

	if (target == NULL)
		result = -1;
	else
		result = 1;

	closedir(target);
	return result;
}

/**
 * Função utilizada para verificar se o dispositivo USB conectado a uma porta X
 * do HUB_USB é um modem 3G.
 *
 * É passado por parâmetro a porta desejada para verificação da existencia de dispositivo conectado
 * Retorna 1 caso o dispositivo for um modem 3G
 * Retorna -1 se não houver dispositivo conectado ou não for um modem 3G
 *
 * OBS: a verificação da existencia de disp. USB conectado é feita através da análise de diretórios
 * criados segundo a porta do HUB_USB
 * @param port
 * @return
 */
int lusb_check_usb_ttyUSB (int port){
	struct DIR * target=NULL;
	char buff_addr[sizeof(ADDR_PORT_USB)];
	int result=0, i=0;

	if (lusb_check_usb_connected(port) == -1){
		result = -1;
		goto res;
	}

	for (i=0;i<15;i++){
		sprintf (buff_addr, ADDR_PORT_USB, HUB_PORT, port, HUB_PORT, port, i);

		target = (struct DIR *)opendir(buff_addr);

		if (target != NULL){
			result = 1;
			break;
		}
		else
			result = -1;
	}

res:	closedir(target);
	return result;

}



/**
 * Função utilizada para adquirir informações referentes ao dispositivo USB conectado ao sistema
 * É passado por parâmetro uma struct (lusb_dev), a qual é preenchida com informações retiradas do
 * dispositivo USB através da libusb
 *
 * Retorna 1 se a função conseguiu adquirir as informações da porta USB apontada
 * Retorna -1 caso não consiga adquirir as informações, devido a inexistencia do dispositivo apontado
 * @param usb
 * @return
 */
int lusb_get_descriptor (lusb_dev * usb){

	FILE * file;
	libusb_device *dev;
	libusb_device **devs;
	char idProd_usb_ffile[64];
	char addr_file[64];
	char conv_temp[64];
	int err = 0, i = 0, idProd_usb_ffc = 0, conv_idProd = 0;
	struct libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;



	sprintf (addr_file, ADDR_USB_IDPRODUCT, usb->port);

	file = fopen (addr_file, "rt");
	if (!file)
		return -1;
	fgets(idProd_usb_ffile,64,file);
	fclose (file);

	idProd_usb_ffc = atoi(idProd_usb_ffile);



	err = libusb_init(NULL);
	if (err < 0) {
		fprintf(stderr, "Failed to libusb_init\n");
		return;
	}
	err = libusb_get_device_list(NULL, &devs);
	if (err < 0){
		fprintf(stderr, "Failed to libusb_get_device_list\n");
		return;
	}

	while ((dev = devs[i++]) != NULL) {
		err = libusb_get_device_descriptor(dev, &desc);
		if (err < 0) {
				fprintf(stderr, "Failed to get device descriptor\n");
				return;
		}


		sprintf(conv_temp,"%04x",desc.idProduct);
		conv_idProd = atoi(conv_temp);


		if ( (lusb_check_usb_connected(usb->port) == 1) && (desc.idVendor != IDVENDOR_LINUX_HUB) && (idProd_usb_ffc == conv_idProd)){

			err = libusb_open(dev,&handle);

			if (err < 0) {
				printf("Coudn't open handle\n");
				libusb_close(handle);
				return;
			}
			err = libusb_get_string_descriptor_ascii(handle,desc.iProduct,usb->iProduct_string,sizeof(usb->iProduct_string));
			if (err < 0) {
				printf("Coudn't get iProduct string\n");
				libusb_close(handle);
				return;
			}
			err = libusb_get_string_descriptor_ascii(handle,desc.iManufacturer,usb->iManufacture_string,sizeof(usb->iManufacture_string));
			if (err < 0) {
				printf("Coudn't get iManufacturer string\n");
				libusb_close(handle);
				return;
			}

			usb->bus = libusb_get_bus_number(dev);
			usb->idProduct = desc.iProduct;
			usb->idVendor = desc.idVendor;


			libusb_close(handle);
			libusb_free_device_list(devs, 1);
			libusb_exit(NULL);
			return 1;
		}
	}

	libusb_close(handle);
	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
	return -1;

}


