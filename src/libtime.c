#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/config.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "options.h"
#include "error.h"

#define	TZ_MAGIC	"TZif"

struct tzhead {
 	char	tzh_magic[4];		/* TZ_MAGIC */
	char	tzh_reserved[16];	/* reserved for future use */
	char	tzh_ttisgmtcnt[4];	/* coded number of trans. time flags */
	char	tzh_ttisstdcnt[4];	/* coded number of trans. time flags */
	char	tzh_leapcnt[4];		/* coded number of leap seconds */
	char	tzh_timecnt[4];		/* coded number of transition times */
	char	tzh_typecnt[4];		/* coded number of local time types */
	char	tzh_charcnt[4];		/* coded number of abbr. chars */
};

/*
** . . .followed by. . .
**
**	tzh_timecnt (char [4])s		coded transition times a la time(2)
**	tzh_timecnt (unsigned char)s	types of local time starting at above
**	tzh_typecnt repetitions of
**		one (char [4])		coded UTC offset in seconds
**		one (unsigned char)	used to set tm_isdst
**		one (unsigned char)	that's an abbreviation list index
**	tzh_charcnt (char)s		'\0'-terminated zone abbreviations
**	tzh_leapcnt repetitions of
**		one (char [4])		coded leap second transition times
**		one (char [4])		total correction after above
**	tzh_ttisstdcnt (char)s		indexed by type; if TRUE, transition
**					time is standard time, if FALSE,
**					transition time is wall clock time
**					if absent, transition times are
**					assumed to be wall clock time
**	tzh_ttisgmtcnt (char)s		indexed by type; if TRUE, transition
**					time is UTC, if FALSE,
**					transition time is local time
**					if absent, transition times are
**					assumed to be local time
*/

#define TZFILE "/etc/localtime"

int set_timezone(char *name, int hours, int mins)
{
	struct tzhead h;
	long offset;
	unsigned char zero, len;
	int fd;
	char str[16];

	if ((hours<-23)||(hours>23)) return (-1);
	if ((mins<0)||(mins>59)) return (-1);

	if (hours<0) mins = -mins;

	fd = open(TZFILE, O_CREAT|O_WRONLY);
	if (fd<0)
	{
		pr_error(1, "Can't open %s", TZFILE);
		return (-1);
	}

	offset = 3600*hours+60*mins;
	zero = 0;
	strncpy(str, name, 16);
	str[15] = 0;
	len = strlen(str);

	memset(&h, 0, sizeof(struct tzhead));
	memcpy(&h.tzh_magic, TZ_MAGIC, 4);
	h.tzh_ttisgmtcnt[3] = 1;
	h.tzh_ttisstdcnt[3] = 1;
	h.tzh_leapcnt[3] = 0;
	h.tzh_timecnt[3] = 0;
	h.tzh_typecnt[3] = 1;
	h.tzh_charcnt[3] = len+1;

	write(fd, &h, sizeof(struct tzhead));
	write(fd, &offset, 4);
	write(fd, &zero, 1);
	write(fd, &zero, 1);
	write(fd, str, len+1);
	write(fd, &zero, 1);
	write(fd, &zero, 1);
	close(fd);

	tzset(); /* !!! TZ from localtime */
	return 0;
}

int get_timezone(char *name, int *hours, int *mins)
{
	long offset;
	int fd, len;
	struct tzhead h;

	fd = open(TZFILE, O_RDONLY);
	if (fd<0)
		return (-1);

	read(fd, &h, sizeof(struct tzhead));
	read(fd, &offset, 4);
	len = h.tzh_charcnt[3];
	if (len>=16) len=16;
	lseek(fd, 2, SEEK_CUR);
	read(fd, name, len);
	name[len-1]=0;

	close(fd);

	*hours = offset/3600;
	*mins = (offset%3600)/60;
	return 0;
}

#define NEXT(date) { p=date; while ((*p)&&(*p!='/')&&(*p!='.')) p++; }
int parse_date(char *date, int *d, int *m, int *y)
{
	char *p;

	if (!date) return (-1);

	NEXT(date);
	if (*p==0) return (-1);
	*p = 0;
	*d = atoi(date);
	if ((*d<1)||(*d>31)) return (-1);

	date = p+1;
	if (*date==0) return (-1);
	NEXT(date);
	if (*p==0) return (-1);
	*p = 0;
	*m = atoi(date);
	if ((*m<1)||(*m>12)) return (-1);

	date = p+1;
	if (*date==0) return (-1);
	*y = atoi(date);
	if ((*y<1902)&&(*y>=2037)) return (-1);

	return 0;
}

#define NEXT_(time) { p=time; while ((*p)&&(*p!=':')) p++; }
int parse_time(char *time, int *h, int *m, int *s)
{
	char *p;
	if (!time) return (-1);

	NEXT_(time);
	if (*p==0) return (-1);
	*p = 0;
	*h = atoi(time);
	if ((*h<0)||(*h>23)) return (-1);

	time = p+1;
	if (*time==0) return (-1);
	NEXT_(time);
	if (*p==0) return (-1);
	*p = 0;
	*m = atoi(time);
	if ((*m<0)||(*m>59)) return (-1);

	time = p+1;
	if (*time==0) return (-1);
	*s = atoi(time);
	if ((*s<0)||(*s>59)) return (-1);

	return 0;
}

int get_date(char *buf, int size)
{
	time_t tm;
	struct tm tm_time;

	time(&tm);
	localtime_r(&tm, &tm_time);
	strftime(buf, size-1, "%a %b %e %H:%M:%S %Z %Y", &tm_time);
	puts(buf);
	return 0;
}

#define	I2C_RTC_ADDR		0x68

#define RTC_SEC_REG_ADDR	0x00
#define RTC_MIN_REG_ADDR	0x01
#define RTC_HR_REG_ADDR		0x02
#define RTC_DAY_REG_ADDR	0x03
#define RTC_DATE_REG_ADDR	0x04
#define RTC_MON_REG_ADDR	0x05
#define RTC_YR_REG_ADDR		0x06
#define RTC_CTL_REG_ADDR	0x07

#define RTC_SEC_BIT_CH		0x80	/* Clock Halt (in Register 0) */

/* DS1338 Register 7 (Control) */
#define RTC_CTL_BIT_RS0		0x01	/* Rate select 0 */
#define RTC_CTL_BIT_RS1		0x02	/* Rate select 1 */
#define RTC_CTL_BIT_SQWE	0x10	/* Square Wave Enable */
#define RTC_CTL_BIT_OSF		0x20	/* Oscillator Stop Flag (Only on DS1338 !!!) */
#define RTC_CTL_BIT_OUT		0x80	/* Output Control */

struct rtc_ds1338_regs
{
	unsigned char seconds;
	unsigned char minutes;
	unsigned char hour;
	unsigned char day;
	unsigned char date;
	unsigned char month;
	unsigned char year;
	unsigned char control;
	unsigned char ram[56];
};

#define USE_DEV_RTC
#ifdef USE_DEV_RTC
#include <linux/rtc.h>
#endif

#ifndef USE_DEV_RTC
static unsigned char bin2bcd(unsigned int n)
{
	return (((n / 10) << 4) | (n % 10));
}
#endif

int set_date(int day, int mon, int year, int hour, int min, int sec)
{
	time_t tm;
	struct tm tm_time;
	int fd, err, ret=0;
#ifdef USE_DEV_RTC
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/misc/rtc";
#else
	char device[] = "/dev/rtc";
#endif
#else
	struct rtc_ds1338_regs ds1338;
#if defined(CONFIG_DEVFS_FS)
	char device[] = "/dev/i2c/0";
#else
	char device[] = "/dev/i2c-0";
#endif
#endif

	/* local time */
	tm_time.tm_year = (year >= 1900 ? year-1900 : year%100); /* since 1900 */
	tm_time.tm_mon  = mon-1; /* 0-11 */
	tm_time.tm_mday = day;
	tm_time.tm_hour = hour;
	tm_time.tm_min  = min;
	tm_time.tm_sec  = sec;
	tm = mktime(&tm_time);
	if (tm < 0)
		return -1;
	/* UCT time */
	gmtime_r(&tm, &tm_time);
#ifndef USE_DEV_RTC
	ds1338.seconds=bin2bcd(tm_time.tm_sec);
	ds1338.minutes=bin2bcd(tm_time.tm_min);
	ds1338.hour=bin2bcd(tm_time.tm_hour);
	ds1338.day=bin2bcd(tm_time.tm_wday+1);
	ds1338.date=bin2bcd(tm_time.tm_mday);
	ds1338.month=bin2bcd(tm_time.tm_mon+1); /* 1-12 */
	ds1338.year=bin2bcd(tm_time.tm_year % 100); /* 00-99 */
#if 1
	ds1338.control=0;
#else /* ds1338 x m41t11 */
	ds1338.control=RTC_CTL_BIT_RS0|RTC_CTL_BIT_RS1;
#endif
#endif
	
	/* Sets the system's time and date */
	err = stime(&tm);
	
	/* Set RTC! */
	if ((fd = open(device, O_RDWR)) < 0)
		return -1;
	if (err < 0)
	{
		pr_error(1, "cannot set date");
	}
	else
	{	
#ifdef USE_DEV_RTC
		if (ioctl(fd, RTC_SET_TIME, &tm_time) < 0) goto error;
#else
		if (i2c_write_block_data(fd, I2C_RTC_ADDR, 0x00, 8, (__u8 *)&ds1338) < 0) goto error;
#endif
	}
exit_now:
	close(fd);
	return ret;
error:
	ret=-1;
	goto exit_now;
}

#ifdef OPTION_NTPD
int set_rtc_with_system_date(void)
{
	time_t tm;
	struct tm tm_time;
	int fd, ret=0;
#ifdef USE_DEV_RTC
	char device[] = "/dev/rtc";
#else
	struct rtc_ds1338_regs ds1338;
	char device[] = "/dev/i2c/0";
#endif

	time(&tm);
	gmtime_r(&tm, &tm_time);
#ifndef USE_DEV_RTC
	ds1338.seconds=bin2bcd(tm_time.tm_sec);
	ds1338.minutes=bin2bcd(tm_time.tm_min);
	ds1338.hour=bin2bcd(tm_time.tm_hour);
	ds1338.day=bin2bcd(tm_time.tm_wday+1);
	ds1338.date=bin2bcd(tm_time.tm_mday);
	ds1338.month=bin2bcd(tm_time.tm_mon+1); /* 1-12 */
	ds1338.year=bin2bcd(tm_time.tm_year % 100); /* 00-99 */
#if 1
	ds1338.control=0;
#else /* ds1338 x m41t11 */
	ds1338.control=RTC_CTL_BIT_RS0|RTC_CTL_BIT_RS1;
#endif
#endif
	/* Set RTC! */
	if ((fd = open(device, O_RDWR)) < 0)
		return -1;
#ifdef USE_DEV_RTC
	if (ioctl(fd, RTC_SET_TIME, &tm_time) < 0)
		goto error;
#else
	if (i2c_write_block_data(fd, I2C_RTC_ADDR, 0x00, 8, (__u8 *)&ds1338) < 0)
		goto error;
#endif
exit_now:
	close(fd);
	return ret;
error:
	ret=-1;
	goto exit_now;
}
#endif
