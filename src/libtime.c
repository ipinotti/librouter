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
#include <stdarg.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/rtc.h>

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

int librouter_time_set_timezone(char *name, int hours, int mins)
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
		librouter_pr_error(1, "Can't open %s", TZFILE);
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

int librouter_time_get_timezone(char *name, int *hours, int *mins)
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

int librouter_time_get_rtc_date(char *buf, int size)
{
	struct tm tm_time;
	int fd;
	char device[] = "/dev/rtc0";

	/* Set RTC! */
	if ((fd = open(device, O_RDONLY)) < 0)
		return -1;

	if (ioctl(fd, RTC_RD_TIME, &tm_time) < 0) {
		close(fd);
		return -1;
	}

	close(fd);
	strftime(buf, size-1, "%a %b %e %H:%M:%S %Z %Y", &tm_time);

	return 0;

}

int librouter_time_get_date(char *buf, int size)
{
	time_t tm;
	struct tm tm_time;

	time(&tm);
	localtime_r(&tm, &tm_time);
	strftime(buf, size-1, "%a %b %e %H:%M:%S %Z %Y", &tm_time);
	puts(buf);
	return 0;
}

int librouter_time_set_date(int day, int mon, int year, int hour, int min, int sec)
{
	time_t tm;
	struct tm tm_time;
	int fd, err, ret=0;
	char device[] = "/dev/rtc0";

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
	
	/* Sets the system's time and date */
	err = stime(&tm);
	
	/* Set RTC! */
	if ((fd = open(device, O_RDWR)) < 0)
		return -1;

	if (err < 0) {
		librouter_pr_error(1, "cannot set date");
	}
	else {
		if (ioctl(fd, RTC_SET_TIME, &tm_time) < 0)
			goto error;
	}
exit_now:
	close(fd);
	return ret;
error:
	ret=-1;
	goto exit_now;
}

static int uptime_sprintf(char * time_buf, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];

	buf[0] = 0;

	va_start(ap, fmt);
	vsnprintf(buf, 1023, fmt, ap);
	va_end(ap);
	buf[1023] = 0;

	sprintf(time_buf,"%s", buf);

	return 0;

}

int librouter_time_get_uptime(char * time_buf)
{
	FILE *tf;
	int ret = 0;
	char timeup [256];
	char *tmp;

	tf = popen("/bin/uptime", "r");

	if (tf == NULL){
		ret = -1;
		return ret;
	}

	while (!feof(tf)) {
		timeup[0] = 0;
		fgets(timeup, 255, tf);
		timeup[255] = 0;

		tmp = strchr(timeup, ',');
		if (tmp != NULL)
			*tmp = 0;

		if (strlen(timeup)){
			ret = uptime_sprintf(time_buf,"%s",&timeup[12]);
		}
	}
	if (tf)
		pclose(tf);

	return ret;
}

#ifdef OPTION_NTPD
int librouter_time_system_to_rtc(void)
{
	time_t tm;
	struct tm tm_time;
	int fd, ret=0;
	char device[] = "/dev/rtc0";

	time(&tm);
	gmtime_r(&tm, &tm_time);

	/* Set RTC! */
	if ((fd = open(device, O_RDWR)) < 0)
		return -1;

	if (ioctl(fd, RTC_SET_TIME, &tm_time) < 0)
		goto error;
exit_now:
	close(fd);
	return ret;
error:
	ret=-1;
	goto exit_now;
}
#endif
