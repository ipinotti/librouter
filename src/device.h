/*
 * device.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef DEVICES_H_
#define DEVICES_H_

typedef enum {
	none, eth, lo, ppp, tun, ipsec, efm
} device_type;

typedef enum {
	str_linux, str_cish, str_web
} string_type;

typedef struct {
	device_type type;
	const char *cish_string; /* Interface name as shown in CISH */
	const char *linux_string; /* Interface name in the linux kernel */
	const char *web_string; /* Interface name as shown in the web browser */
} dev_family;

extern dev_family _devices[];

dev_family *librouter_device_get_family_by_name(const char *name, string_type type);
dev_family *librouter_device_get_family_by_type(device_type type);
int librouter_device_get_major(const char *name, string_type type);
char *librouter_device_cli_to_linux(const char *device, int major, int minor);
char *librouter_device_linux_to_cli(const char *osdev, int mode);
char *librouter_device_to_linux_cmdline(char *cmdline);
char *librouter_device_from_linux_cmdline(char *cmdline);
char *librouter_zebra_from_linux_cmdline(char *cmdline);
char *librouter_zebra_to_linux_cmdline(char *cmdline);

#endif /* DEVICES_H_ */

