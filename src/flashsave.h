#define CFG_PRODUCT 		"3GRouter"

#define CFG_BOOT_MTD "/dev/mtd0"
#define CFG_SYS_MTD "/dev/mtd1"

#define IMAGE_FILE "/mnt/image"

#define FEBRUARY		2
#define	STARTOFTIME		1970
#define SECDAY			86400L
#define SECYR			(SECDAY * 365)
#define	leapyear(year)		((year) % 4 == 0)
#define	days_in_year(a) 	(leapyear(a) ? 366 : 365)
#define	days_in_month(a) 	(month_days[(a) - 1])

void write_image(int burn);

/**
 * Image check errors
 */
enum img_check_error {
	NO_FILE = 1,
	WRONG_HEADER_SIZE,
	WRONG_MAGIC_NUMBER,
	WRONG_SIZE,
	BAD_HEADER_CRC,
	BAD_DATA_CRC,
	WRONG_PROD_VERSION,
	WRONG_IMG_TYPE
};

/**
 * Write errors
 */
enum write_error {
	IMAGE_ERROR = 1, WRITE_ERROR,
};

int libconfig_flash_write_image(char *img);
int libconfig_flash_check_image(char *img);
