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

#define addr_usb "/sys/bus/usb/devices/2-"
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
	char buff_addr[sizeof(addr_usb)+10] = addr_usb;
	char port_usb_num[10];
	int result=0;

	snprintf(port_usb_num,10,"%d",port);
	strcat(buff_addr,port_usb_num);
	target = opendir(buff_addr);

	if (target == NULL)
		result = -1;
	else
		result = 1;

	closedir(target);
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

	libusb_device *dev;
	libusb_device **devs;

	int err = 0, i=0;
	int busnumber=0;
	int busaddr=0;

	struct libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;

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
		busnumber = libusb_get_bus_number(dev);
		err = libusb_get_device_descriptor(dev, &desc);
		if (err < 0) {
				fprintf(stderr, "Failed to get device descriptor\n");
				return;
		}
		//printf("\nlusb check usb %d on port %d\n\n", lusb_check_usb_connected(usb->port), usb->port); //FIXME

		if ((lusb_check_usb_connected(usb->port) == 1) && (desc.idVendor != IDVENDOR_LINUX_HUB)){
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

