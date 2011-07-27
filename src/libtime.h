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

int librouter_time_set_timezone(char *name, int hours, int mins);
int librouter_time_get_timezone(char *name, int *hours, int *mins);
int parse_date(char *date, int *d, int *m, int *y);
int parse_time(char *time, int *h, int *m, int *s);
int librouter_time_get_rtc_date(char *buf, int size);
int librouter_time_get_date(char *buf, int size);
int librouter_time_set_date(int day, int mon, int year, int hour, int min, int sec);
int librouter_time_get_uptime(char * time_buf);

int librouter_time_system_to_rtc(void);
int librouter_time_rtc_to_system(void);
