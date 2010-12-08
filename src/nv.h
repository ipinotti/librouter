/*
 * nv.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef NV_H_
#define NV_H_

/*#include <linux/autoconf.h>*/
#include "options.h"
#include "typedefs.h"

#define DEV_STARTUP_CONFIG "/dev/startup-config"

#define MAGIC_CONFIG 0xf2572c30
#define MAGIC_SECRET 0xf2572c31
#define MAGIC_SLOT0 0xf2572c32
#define MAGIC_SLOT1 0xf2572c33
#define MAGIC_SLOT2 0xf2572c34
#define MAGIC_SLOT3 0xf2572c35
#define MAGIC_SLOT4 0xf2572c36
#define MAGIC_SSH 0xf2572c37
#define MAGIC_NTP 0xf2572c38
#define MAGIC_SNMP 0xf2572c39
#define MAGIC_UNUSED 0xFFFFFFFFUL

typedef struct {
	u32 magic_number;
	u32 id;
	u32 size;
	u32 crc;
} cfg_header;

typedef struct {
	int valid;
	cfg_header hdr;
	long offset;
} cfg_pack;

#define	IH_NMLEN	32

struct _nv {
	long next_header;
	cfg_pack config;
	cfg_pack previousconfig;
#ifdef OPTION_IPSEC
	cfg_pack secret;
#endif
#if 1
	cfg_pack slot[5];
#endif
	cfg_pack ssh;
	cfg_pack ntp;
	cfg_pack snmp;
};

/* features buffer */
struct _saved_feature {
	unsigned char pos;
	char key[16];
};

int librouter_nv_load_configuration(char *filename);
int librouter_nv_load_previous_configuration(char *filename);
#ifdef CONFIG_PROTOTIPO
int load_slot_configuration(char *filename, int slot);
#endif
int librouter_nv_save_configuration(char *filename);

#ifdef FEATURES_ON_FLASH
int load_features(void *buf, long size);
int save_features(void *buf, long size);
#else
int load_feature(struct _saved_feature *feature);
int save_feature(struct _saved_feature *feature);
char *get_serial_number(void);
char *get_system_ID(int);
char *librouter_nv_get_product_name(char * product_define);
#endif
#if defined(OPTION_IPSEC)
int librouter_nv_save_ipsec_secret(char *secret);
char *librouter_nv_get_rsakeys(void);
#endif
int librouter_nv_save_ssh_secret(char *filename);
int librouter_nv_load_ssh_secret(char *filename);
int librouter_nv_save_ntp_secret(char *filename);
int librouter_nv_load_ntp_secret(char *filename);
int librouter_nv_save_snmp_secret(char *filename);
int librouter_nv_load_snmp_secret(char *filename);
int get_starts_counter(char *store, unsigned int max_len);
int get_uboot_env(char *name, char *store, int max_len);
int get_trialminutes_counter(char *store, unsigned int max_len);
int get_release_date(unsigned char *buf, unsigned int max_len);
int change_uboot_env(char *name, char *new_value);
int board_change_to_licensed(void);
int inc_starts_counter(void);
int board_change_to_trial(unsigned int days);
unsigned int get_mb_info(unsigned int specific,
                         char *store,
                         unsigned int max_len);
int inc_trialminutes_counter(void);
int print_image_date(void);

#endif /* NV_H_ */
