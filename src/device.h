/*
 * device.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef DEVICES_H_
#define DEVICES_H_

typedef enum {
	none, eth, lo, ppp, tun, ipsec
} device_type;

typedef struct {
	device_type type;
	const char *cish_string;
	const char *linux_string;
} dev_family;

extern dev_family _devices[];

dev_family *libconfig_device_get_family(const char *name);
char *libconfig_device_convert(const char *device, int major, int minor);
char *libconfig_device_convert_os(const char *osdev, int mode);
char *libconfig_device_to_linux_cmdline(char *cmdline);
char *libconfig_device_from_linux_cmdline(char *cmdline);
char *libconfig_zebra_from_linux_cmdline(char *cmdline);
char *libconfig_zebra_to_linux_cmdline(char *cmdline);

#endif /* DEVICES_H_ */

