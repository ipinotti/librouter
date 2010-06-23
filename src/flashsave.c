#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <netinet/in.h>		/* For host / network byte order conversions    */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/stat.h>

#include <linux/autoconf.h>

#include "options.h"

#include <u-boot/types.h>
#include <u-boot/image.h>
#include <u-boot/rtc.h>
#include <u-boot/zlib.h>

#include "defines.h"
#include "flashsave.h"
#include "crc32.h"
#include "nv.h"

static int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

image_header_t header;
image_header_t *hdr = &header;


/**
 * Calculates the day of the week based on the Gregorian Calendar.
 *
 * This only works for the Gregorian calendar - i.e. after 1752 (in the UK)
 *
 * @param tm
 */
static void gregorian_weekday(struct rtc_time *tm)
{
	int leapsToDate;
	int lastYear;
	int day;
	int MonthOffset[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304,
	                334 };

	lastYear = tm->tm_year - 1;

	/*
	 * Number of leap corrections to apply up to end of last year
	 */
	leapsToDate = lastYear / 4 - lastYear / 100 + lastYear / 400;

	/*
	 * This year is a leap year if it is divisible by 4 except when it is
	 * divisible by 100 unless it is divisible by 400
	 *
	 * e.g. 1904 was a leap year, 1900 was not, 1996 is, and 2000 will be
	 */
	if ((tm->tm_year % 4 == 0) && ((tm->tm_year % 100 != 0) || (tm->tm_year
	                % 400 == 0)) && (tm->tm_mon > 2)) {
		/*
		 * We are past Feb. 29 in a leap year
		 */
		day = 1;
	} else {
		day = 0;
	}

	day += lastYear * 365 + leapsToDate + MonthOffset[tm->tm_mon - 1]
	                + tm->tm_mday;

	tm->tm_wday = day % 7;
}

/**
 * Convert a timestamp to rtc_time structure.
 *
 * @param ts timestamp
 * @param tm rtc_time structure pointer
 */
static void timestamp_to_rtc_time(int ts, struct rtc_time *tm)
{
	register int i;
	register long hms, day;

	day = ts / SECDAY;
	hms = ts % SECDAY;

	/* Hours, minutes, seconds are easy */
	tm->tm_hour = hms / 3600;
	tm->tm_min = (hms % 3600) / 60;
	tm->tm_sec = (hms % 3600) % 60;

	/* Number of years in days */
	for (i = STARTOFTIME; day >= days_in_year(i); i++) {
		day -= days_in_year(i);
	}
	tm->tm_year = i;

	/* Number of months in days left */
	if (leapyear(tm->tm_year)) {
		days_in_month(FEBRUARY) = 29;
	}
	for (i = 1; day >= days_in_month(i); i++) {
		day -= days_in_month(i);
	}
	days_in_month(FEBRUARY) = 28;
	tm->tm_mon = i;

	/* Days are what is left over (+1) from all that. */
	tm->tm_mday = day + 1;

	/*
	 * Determine the day of week
	 */
	gregorian_weekday(tm);
}

/**
 * Print uboot image header information
 * @param hdr
 */
static void print_image_hdr(image_header_t *hdr)
{
	time_t timestamp = (time_t) hdr->ih_time;
	struct rtc_time tm;

	printf("  Image Name:   %.*s\n", IH_NMLEN, hdr->ih_name);

	timestamp_to_rtc_time(timestamp, &tm);
	printf("  Created:      %4d-%02d-%02d  %2d:%02d:%02d UTC\n",
	                tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour,
	                tm.tm_min, tm.tm_sec);

#if 0				/* Extra info disabled! */
	printf("  Image Type:   ");
	print_type(hdr);
	printf("\n");
	printf("  Data Size:    %d Bytes = %d kB = %d MB\n",
			hdr->ih_size, hdr->ih_size >> 10, hdr->ih_size >> 20);
	printf("  Load Address: %08x\n", hdr->ih_load);
	printf("  Entry Point:  %08x\n", hdr->ih_ep);

	if (hdr->ih_type == IH_TYPE_MULTI || hdr->ih_type == IH_TYPE_KCRAMFS) {
		int i;
		unsigned int *data;

		data = (unsigned int *)((unsigned char *)hdr + sizeof(image_header_t));
		printf("  Contents:\n");
		for (i = 0; data[i]; i++)
		printf("  Image %d: %8d Bytes = %d kB = %d MB\n", i,
				data[i], data[i] >> 10, data[i] >> 20);
	}
#endif
}

/**
 * Erase and write a chunk of data
 *
 * @param erase
 * @param data
 * @param len
 * @param offset
 * @return
 */
static int _flash_write_chunk (erase_info_t *erase, int fd, unsigned char *data, unsigned int len, int *offset)
{
	int offset_val = *offset;
	int size;
	int count;
	ssize_t retval;

	if (erase->start < len) {
		if (ioctl(fd, MEMERASE, erase) != 0) {
			perror("MTD Erase failure");
			return 1;
		}

		size = (len - offset_val > erase->length ? erase->length : len - offset_val);

		for (count = 0; count < size;) {

			retval = write(fd, (char *) data + offset_val + count, size - count);

			if (retval == -1) {
				if (errno == EAGAIN)
					continue;
				perror("Write Error");
				return 2;
			}
			count += retval;
		}
		*offset += size;
		printf("\b\b\b%02d%%", offset_val * 100 / len);
		fflush(stdout);
	}

	return 0;
}

/**
 * Write flash image
 *
 * @param mtd_dev flash device
 * @param data data pointer
 * @param len date length
 * @return 0 if successful
 */
static int flash_save(char *mtd_dev, unsigned char *data, unsigned int len)
{
	int output;
	mtd_info_t meminfo;
	int regcount, count, offset, size;
	ssize_t retval;

	if ((output = open(mtd_dev, O_RDWR)) < 0) {
		printf("%% could not open %s\n", mtd_dev);
		return -1;
	}

	if (ioctl(output, MEMGETINFO, &meminfo) != 0) {
		perror("unable to get MTD device info");
		goto error;
	}

	/* Check if image fits */
	if (meminfo.size < len) {
		printf("%%ERROR : Image size too big\n");
		goto error;
	}

	if (ioctl(output, MEMGETREGIONCOUNT, &regcount) == 0) {
		printf("  Writing flash ... 00%%");
		fflush(stdout);
		offset = 0;

		if (regcount > 0) {
			int i, start;
			region_info_t *reginfo;

			reginfo = calloc(regcount, sizeof(region_info_t));
			if (reginfo == NULL) {
				perror("unable to allocate memory");
				goto error;
			}

			for (i = 0; i < regcount; i++) {
				reginfo[i].regionindex = i;
				if (ioctl(output, MEMGETREGIONINFO, &(reginfo[i])) != 0) {
					free(reginfo);
					goto error;
				}
			}

			for (start = 0, i = 0; i < regcount;) {
				erase_info_t erase;
				region_info_t *r = &(reginfo[i]);

				erase.start = start;
				erase.length = r->erasesize;

				if (_flash_write_chunk (&erase, output, data, len, &offset))
					goto error;

				start += erase.length;
				if (start >= (r->offset + r->numblocks * r->erasesize)) {
					//We finished region i so move to region i+1
					i++;
				}
			}
			free(reginfo);
		} else {
			erase_info_t erase;

			erase.length = meminfo.erasesize;
			for (erase.start = 0; erase.start < meminfo.size; erase.start
			                += meminfo.erasesize) {

				if (_flash_write_chunk(&erase, output, data, len, &offset))
					goto error;
			}
		}
		printf("\b\b\b\bOK  \n");
	}

	close(output);
	return 0;
error:
	close(output);
	return -1;

}

/**
 * Check image integrity and validates it
 *
 * @param img image file
 * @return
 */
int libconfig_flash_check_image(char *img)
{
	struct stat sbuf;
	image_header_t *header;
	unsigned char *file_ptr, *file_data;
	int file;
	int rtn = 0;
	int len;
	uint32_t checksum;

	header = malloc(sizeof(image_header_t));
	if (header == NULL) {
		return -1;
	}

	file = open(img, O_RDONLY);
	if (file < 0)
		return NO_FILE;

	if (fstat(file, &sbuf) < 0)
		return NO_FILE;

	if (sbuf.st_size < sizeof(image_header_t))
		return WRONG_HEADER_SIZE;

	file_ptr = (unsigned char *) mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED,
	                file, 0);

#if 0
	/* does this ever happens? */
	if ((caddr_t) file_ptr == (caddr_t) -1)
	return -1;
#endif

	/*
	 * create copy of header so that we can blank out the
	 * checksum field for checking - this can't be done
	 * on the PROT_READ mapped data.
	 */
	memcpy(header, file_ptr, sizeof(image_header_t));

	if (ntohl(header->ih_magic) != IH_MAGIC) {
		rtn = WRONG_MAGIC_NUMBER;
		goto err;
	}

	file_data = (unsigned char *) header;
	len = sizeof(image_header_t);
	checksum = ntohl(header->ih_hcrc);
	header->ih_hcrc = htonl(0); /* clear for re-calculation */

	if (crc32(0, file_data, len) != checksum) {
		fprintf(stderr,
		        "*** Warning: \"%s\" has bad header checksum!\n",
		        img);
		rtn = BAD_HEADER_CRC;
		goto err;
	}

	if ((sbuf.st_size - sizeof(image_header_t)) != header->ih_size) {
		rtn = WRONG_SIZE;
		goto err;
	}

	/* for multi-file images we need the data part, too */
	print_image_hdr((image_header_t *) file_ptr);
	printf("  Verifying Checksum ... ");
	fflush(stdout);

	file_data = (unsigned char *) (file_ptr + sizeof(image_header_t));
	len = sbuf.st_size - sizeof(image_header_t);
	if (crc32(0, file_data, len) != ntohl(header->ih_dcrc)) {
		printf("  Bad Data CRC\n");
		fprintf(stderr,
		        "*** Warning: \"%s\" has corrupted data!\n",
		        img);
		rtn = BAD_DATA_CRC;
		goto err;
	} else {
		printf("OK\n");
	}

	if (strncmp((const char *) CFG_PRODUCT, (const char *) header->ih_name, strlen((const char *) CFG_PRODUCT))) {
		return WRONG_PROD_VERSION;
	}

	/* Check image type, if not allowed, return WRONG_IMG_TYPE */
	switch (header->ih_type) {
	case IH_TYPE_UBOOT:
	case IH_TYPE_SPECIAL:
	case IH_TYPE_KERNEL:
	case IH_TYPE_MULTI:
	case IH_TYPE_KCRAMFS:
		break;
#if defined(CFG_EXTRA_SIZE)
	case IH_TYPE_EXTRA:
		break;
#endif
	default:
		rtn = WRONG_IMG_TYPE;
	}

err:
	munmap((void *) file_ptr, sbuf.st_size);
	free(header);
	close(file);
	return rtn;
}

/**
 * Write image
 *
 * @param img image file
 * @return
 */
int libconfig_flash_write_image(char *img)
{
	struct stat sbuf;
	image_header_t * header;
	int err;
	int len;
	int file;
	char *data, *fptr;
	char mtddev[32];

	err = libconfig_flash_check_image(img);

	if (err > 0) {
		return IMAGE_ERROR;
	}

	file = open(img, O_RDONLY);
	fstat(file, &sbuf);

	fptr = (unsigned char *) mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED,
	                file, 0);

	header = (image_header_t *) fptr;

	/* Select address range! */
	switch (header->ih_type) {
	case IH_TYPE_UBOOT:
	case IH_TYPE_SPECIAL: /* Whole flash image! */
		strncpy(mtddev, CFG_BOOT_MTD, sizeof(mtddev));
		data = (unsigned char *) (fptr + sizeof(image_header_t)); /* Skip header! */
		len = header->ih_size;
		break;

	case IH_TYPE_KERNEL:
	case IH_TYPE_MULTI:
	case IH_TYPE_KCRAMFS:
		strncpy(mtddev, CFG_SYS_MTD, sizeof(mtddev));
		data = fptr;
		len = sizeof(image_header_t) + header->ih_size;
		break;
#if defined(CFG_EXTRA_SIZE)
	case IH_TYPE_EXTRA:
		addrflash = CFG_EXTRA_OFFSET;
		data = fptr;
		len = sizeof(image_header_t) + header->ih_size;
		if (len > CFG_EXTRA_SIZE) {
			printf("  Over sized image!\n");
			goto clean;
		}
		break;
#endif
	default:
		/* should never be reached */
		goto err;
	}

	/* Finally write to flash */
	if (flash_save(mtddev, data, len) < 0) {
		printf("%% Could not save image\n");
		goto err;
	}

	printf("Please wait for system reboot...");
	fflush(stdout);

	munmap((void *) fptr, sbuf.st_size);

	close(file);

#if 0
	sleep(3); /* Sleep... */
	reboot(0x01234567); /* RESET! */
#endif
	return 0; /* Never reached! */

err:
	munmap((void *) fptr, sbuf.st_size);
	close(file);
	return WRITE_ERROR;
}

/**
 * Original write image function
 *
 * @param burn ???
 */
void write_image(int burn)
{
	DIR *dir;
	struct dirent *entry;
	int input;
	struct stat sbuf;
	unsigned char *ptr, *data;
	int len;
	uint32_t checksum;
	char mtddev[32];

	/* Procura por um arquivo de imagem valido! */
	dir = opendir(IMAGE_FILE);
	if (dir == NULL) {
		fprintf(stderr, "%% Could not find %s\n", IMAGE_FILE);
		return;
	}

	chdir(IMAGE_FILE);

	input = 0;
	ptr = NULL;
	do {
		entry = readdir(dir);
		if (entry == NULL) {
			if (burn == 2) {
				sleep(3); /* Sleep... */
				rewinddir(dir);
				continue;
			} else {
				fprintf(stderr, "%% Could not find image file. You must upload by ftp first!\n");
				return;
			}
		}

		if (input > 0)
			close(input);

		if ((input = open(entry->d_name, O_RDONLY)) < 0)
			continue;

		/*
		 * list header information of existing image
		 */
		if (ptr != NULL)
			munmap((void *) ptr, sbuf.st_size);

		if (fstat(input, &sbuf) < 0)
			continue;

		if (sbuf.st_size < sizeof(image_header_t))
			continue;

		ptr = (unsigned char *) mmap(0, sbuf.st_size, PROT_READ,
		                MAP_SHARED, input, 0);

		if ((caddr_t) ptr == (caddr_t) -1)
			continue; /* !!! */

		/*
		 * create copy of header so that we can blank out the
		 * checksum field for checking - this can't be done
		 * on the PROT_READ mapped data.
		 */
		memcpy(hdr, ptr, sizeof(image_header_t));

		if (ntohl(hdr->ih_magic) != IH_MAGIC)
			continue;

		data = (unsigned char *) hdr;
		len = sizeof(image_header_t);
		checksum = ntohl(hdr->ih_hcrc);
		hdr->ih_hcrc = htonl(0); /* clear for re-calculation */

		if (crc32(0, data, len) != checksum) {
			fprintf(stderr, "*** Warning: \"%s\" has bad header checksum!\n", entry->d_name);
			continue;
		}

		if (burn == 2 && ((sbuf.st_size - sizeof(image_header_t)) != hdr->ih_size))
			continue; /* Wait for upload... */

		/* for multi-file images we need the data part, too */
		print_image_hdr((image_header_t *) ptr);
		printf("  Verifying Checksum ... ");
		fflush(stdout);

		data = (unsigned char *) (ptr + sizeof(image_header_t));
		len = sbuf.st_size - sizeof(image_header_t);
		if (crc32(0, data, len) != ntohl(hdr->ih_dcrc)) {
			printf("  Bad Data CRC\n");
			fprintf(stderr, "*** Warning: \"%s\" has corrupted data!\n", entry->d_name);
			continue;
		} else {
			printf("OK\n");
		}

		break; /* File OK! */
	} while (1);
	closedir(dir);

	/* Testa se grava? */
	if (!burn) {
		munmap((void *) ptr, sbuf.st_size);
		close(input);
		return;
	}

	if (strncmp((const char *) CFG_PRODUCT, (const char *) hdr->ih_name, strlen((const char *) CFG_PRODUCT))) {
		printf("  Incompatible image!\n");
		goto clean;
	}

	/* Select address range! */
	switch (hdr->ih_type) {
	case IH_TYPE_UBOOT:
	case IH_TYPE_SPECIAL: /* Whole flash image! */
		strncpy(mtddev, CFG_BOOT_MTD, sizeof(mtddev));
		data = (unsigned char *) (ptr + sizeof(image_header_t)); /* Skip header! */
		len = hdr->ih_size;
		break;

	case IH_TYPE_KERNEL:
	case IH_TYPE_MULTI:
	case IH_TYPE_KCRAMFS:
		strncpy(mtddev, CFG_SYS_MTD, sizeof(mtddev));
		data = ptr;
		len = sizeof(image_header_t) + hdr->ih_size;
#if 0				/* Eh necessario um fd para a flash!!! */
		if (!memcmp(data, (image_header_t *) addrflash, sizeof(image_header_t))) {
			if (crc32
					(0, (char *)addrflash + sizeof(image_header_t),
							((image_header_t *) addrflash)->ih_size) == ntohl(hdr->ih_dcrc)) {
				printf("  Image already written!\n");
				goto clean;
			}
		}
#endif
		break;
#if defined(CFG_EXTRA_SIZE)
	case IH_TYPE_EXTRA:
		addrflash = CFG_EXTRA_OFFSET;
		data = ptr;
		len = sizeof(image_header_t) + hdr->ih_size;
		if (len > CFG_EXTRA_SIZE) {
			printf("  Over sized image!\n");
			goto clean;
		}
		break;
#endif
	default:
		printf("  Bad image type!\n");
		goto clean;
	}

	/* Finally write to flash */
	if (flash_save(mtddev, data, len) < 0) {
		printf("%% Could not save image\n");
		goto clean;
	}

	printf("Please wait for system reboot...");
	fflush(stdout);
	munmap((void *) ptr, sbuf.st_size);
	close(input);
	sleep(3); /* Sleep... */
	reboot(0x01234567); /* RESET! */
	return; /* Never reached! */

clean:
	munmap((void *) ptr, sbuf.st_size);
	close(input);
	return;
}
