#ifndef _DEVICES_H
#define _DEVICES_H 1

typedef enum {
	none,
	ethernet,
	loopback,
//	mobile /* 3G */ situação anterior a implementação do 3G
	m3G, /* 3G */
	tunnel,
	ipsec
} device_type;

typedef struct {
	device_type type;
	const char *cish_string;
} device_family;

extern device_family DEV_FAM[];

device_family *getfamily(const char *name);
char *convert_device(const char *device, int major, int minor);
char *convert_os_device (const char *osdev, int mode);
char *cish_to_linux_dev_cmdline(char *cmdline);
char *linux_to_cish_dev_cmdline(char *cmdline);
char *linux_to_zebra_network_cmdline(char *cmdline);
char *zebra_to_linux_network_cmdline(char *cmdline);

#endif

