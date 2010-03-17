#define CFG_PRODUCT 		"AccessRouter"

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

void to_tm(int tim, struct rtc_time * tm);
void write_image(int burn);

