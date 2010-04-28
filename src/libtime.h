#if 0
struct rtc_time
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

int set_timezone(char *name, int hours, int mins);
int get_timezone(char *name, int *hours, int *mins);
int parse_date(char *date, int *d, int *m, int *y);
int parse_time(char *time, int *h, int *m, int *s);
int get_date(char *buf, int size);
int set_date(int day, int mon, int year, int hour, int min, int sec);
int set_rtc_with_system_date(void);
int set_system_date_with_rtc(void);
#endif
