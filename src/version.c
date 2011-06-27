/*
 * version.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "nv.h"

#define FILE_BOOT_IMG "/dev/uboot_image"
#define FILE_SYSTEM_IMG "/dev/system_image"

char *librouter_get_system_version(void)
{
	FILE *f;
	static char version[33];

#ifdef I2C_HC08_PRODUCT
	char *p;
	static char productversion[33];
#endif

	memset(version, 0, 33);
	if ((f = fopen(FILE_SYSTEM_IMG, "rb"))) {
		fseek(f, 32, 0); /* cabecalho: 32 */
		fread(version, 32, 1, f); /* BerlinRouter - 1.0.13 */
		fclose(f);
	}

#ifdef I2C_HC08_PRODUCT
	if( (p = get_product_name()) != NULL ) {
		sprintf(productversion, "%s %s", p, strchr(version, '-'));
		return productversion;
	}
#endif

	return version;
}

char *librouter_get_boot_version(void)
{
	int i;
	FILE *f;
	char *p, *l;
	static char version[64];

	memset(version, 0, 64);

	if ((f = fopen(FILE_BOOT_IMG, "rb"))) {
		fseek(f, 64, 0);
		fread(version, 63, 1, f);
		fclose(f);
		version[63] = 0;
	}

	for (i = 0, p = version; i < 63; i++, p++) {
		if (*p != 0)
			break;
	}

	if ((p = strstr(p, "U-Boot")) == NULL) {
		version[0] = 0;
		return version;
	}

	if ((p + 5) > (version + 63)) {
		version[0] = 0;
		return version;
	}

	/* Remove date of compilation */
	if (l = strstr(p, "("))
		*l = 0;

	strncpy(version, p, sizeof(version));

	return version;
}

