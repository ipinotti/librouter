#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <ctype.h>
#include <sys/stat.h>
#include <libconfig/defines.h>
#include <libconfig/args.h>
#include <libconfig/exec.h>
#include <libconfig/process.h>
#include <libconfig/str.h>
#include <libconfig/snmp.h>
#include <libconfig/lock.h>
#include <libconfig/ppcio.h>
#include <libconfig/nv.h>

void start_default_snmp(void)
{
	snmp_set_community("public", 1, 0);	/* Start with default community */
	return;
}

#if 0
static int get_microcom_lock_pid(void)
{
	FILE *f;
	int pid = 0;
	char buf[128];
	arg_list argl = NULL;
	unsigned int found = 0;

	if( (f = fopen(FILE_MICROCOM_CONF, "r")) == NULL )
		return -1;
	while( !feof(f) && !found ) {
		if( fgets(buf, 127, f) == 0 )
			break;
		buf[127] = 0;
		if( parse_args_din(buf, &argl) == 3 ) {
			if( (strcmp(argl[0], "pid") == 0) && (strcmp(argl[1], "=") == 0) ) {
				pid = atoi(argl[2]);
				found = 1;
			}
		}
	}
	fclose(f);
	if( found == 0 )
		return 0;
	return ((pid > 0) ? pid : -1);
}

static int do_microcom_lock(void)
{
	FILE *f;
	arg_list argl = NULL;
	unsigned int found = 0;
	char buf[128], newline[32];

	sprintf(newline, "pid = %d\n", getpid());
	if( (f = fopen(FILE_MICROCOM_CONF, "r")) == NULL )
		return -1;
	while( !feof(f) ) {
		if( fgets(buf, 127, f) == 0 )
			break;
		buf[127] = 0;
		if( parse_args_din(buf, &argl) == 3 ) {
			if( (strcmp(argl[0], "pid") == 0) && (strcmp(argl[1], "=") == 0) ) {
				found = 1;
				break;
			}
		}
	}
	fclose(f);
	if( found )
		return replace_exact_string(FILE_MICROCOM_CONF, buf, newline);

	if( (f = fopen(FILE_MICROCOM_CONF, "a")) == NULL )
		return -1;
	fwrite(newline, 1, strlen(newline), f);
	fclose(f);
	return 0;
}

static int remove_microcom_lock(void)
{
	FILE *f;
	char buf[128];
	arg_list argl = NULL;
	unsigned int found = 0;

	if( (f = fopen(FILE_MICROCOM_CONF, "r")) == NULL )
		return -1;
	while( !feof(f) ) {
		if( fgets(buf, 127, f) == 0 )
			break;
		buf[127] = 0;
		if( parse_args_din(buf, &argl) == 3 ) {
			if( (strcmp(argl[0], "pid") == 0) && (strcmp(argl[1], "=") == 0) ) {
				found = 1;
				break;
			}
		}
	}
	fclose(f);
	if( found == 0 )
		return 0;
	return replace_exact_string(FILE_MICROCOM_CONF, buf, "");
}

unsigned int get_microcom_lock(void)
{
	int lpid = 0;
	unsigned int i;

	if( lock_microcom_conf_access() ) {
		for( i=0; i < 5; i++ ) {
			lpid = get_microcom_lock_pid();
			if( lpid < 0 )
				continue;
			break;
		}
		if( lpid < 0 ) { /* error */
			unlock_microcom_conf_access();
			return 0;
		}
		if( lpid > 0 ) {
			if( getpid() != lpid ) {
				unlock_microcom_conf_access();
				return 0;
			}
		}
		else { /* write our pid */
			if( do_microcom_lock() < 0 ) {
				unlock_microcom_conf_access();
				return 0;
			}
		}
		unlock_microcom_conf_access();
		return 1;
	}
	return 0;
}

unsigned int release_microcom_lock(void)
{
	unsigned int i;
	int ret = -1, lpid = -1;

	if( lock_microcom_conf_access() ) {
		for( i=0; i < 5; i++ ) {
			lpid = get_microcom_lock_pid();
			if( lpid < 0 )
				continue;
			break;
		}
		if( (lpid < 0) || (getpid() != lpid) ) {
			unlock_microcom_conf_access();
			return 0;
		}
		for( i=0; i < 5; i++ ) {
			ret = remove_microcom_lock();
			if( ret < 0 )
				continue;
			break;
		}
		unlock_microcom_conf_access();
		return ((ret < 0) ? 0 : 1);
	}
	return 0;
}

int get_microcom_mode(void)
{
	FILE *f;
	char buf[128];
	int mode = -1;
	arg_list argl = NULL;

	if( lock_microcom_conf_access() ) {
		if( (f = fopen(FILE_MICROCOM_CONF, "r")) == NULL ) {
			unlock_microcom_conf_access();
			return -1;
		}
		while( !feof(f) ) {
			if( fgets(buf, 127, f) == 0 )
				break;
			buf[127] = 0;
			if( parse_args_din(buf, &argl) == 3 ) {
				if( (strcmp(argl[0], "mode") == 0) && (strcmp(argl[1], "=") == 0) ) {
					if( strcmp(argl[2], "none") == 0 )
						mode = MICROCOM_MODE_NONE;
					else if( strcmp(argl[2], "local") == 0 )
						mode = MICROCOM_MODE_LOCAL;
					else if( strcmp(argl[2], "modem") == 0 )
						mode = MICROCOM_MODE_MODEM;
					else if( strcmp(argl[2], "listen") == 0 )
						mode = MICROCOM_MODE_LISTEN;
					else
						mode = MICROCOM_MODE_NONE;
					break;
				}
			}
		}
		fclose(f);
		unlock_microcom_conf_access();
		return mode;
	}
	return -1;
}

int set_microcom_mode(unsigned int mode)
{
	FILE *f;
	arg_list argl = NULL;
	unsigned int found = 0;
	char buf[128], newline[64];

	if( lock_microcom_conf_access() ) {
		if( (f = fopen(FILE_MICROCOM_CONF, "r")) == NULL ) {
			unlock_microcom_conf_access();
			return -1;
		}
		while( !feof(f) ) {
			if( fgets(buf, 127, f) == 0 )
				break;
			buf[127] = 0;
			if( parse_args_din(buf, &argl) == 3 ) {
				if( (strcmp(argl[0], "mode") == 0) && (strcmp(argl[1], "=") == 0) ) {
					found = 1;
					break;
				}
			}
		}
		fclose(f);
		switch( mode ) {
			case MICROCOM_MODE_NONE:
				strcpy(newline, "mode = none\n");
				break;
			case MICROCOM_MODE_LOCAL:
				strcpy(newline, "mode = local\n");
				break;
			case MICROCOM_MODE_MODEM:
				strcpy(newline, "mode = modem\n");
				break;
			case MICROCOM_MODE_LISTEN:
				strcpy(newline, "mode = listen\n");
				break;
		}
		if( found ) {
			unlock_microcom_conf_access();
			if( replace_exact_string(FILE_MICROCOM_CONF, buf, newline) < 0 )
				return -1;
		}
		else {
			if( (f = fopen(FILE_MICROCOM_CONF, "a")) == NULL ) {
				unlock_microcom_conf_access();
				return -1;
			}
			fwrite(newline, 1, strlen(newline), f);
			fclose(f);
			unlock_microcom_conf_access();
		}

		switch( mode ) {
			case MICROCOM_MODE_NONE:
			case MICROCOM_MODE_MODEM:
				kill_daemon(PROG_MICROCOM);
				sleep(1);
				break;
			case MICROCOM_MODE_LOCAL:
			case MICROCOM_MODE_LISTEN:
				if( is_daemon_running(PROG_MICROCOM) ) {
					/* Envia sinal SIGHUP para microcom */
					pid_t pid = get_pid(PROG_MICROCOM);
					if( pid > 0 )
						kill(pid, SIGHUP);
				}
				else
					exec_daemon(PROG_MICROCOM); /* Coloca microcom pra rodar */
				break;
		}
		return 0;
	}
	return -1;
}

unsigned int is_microcom_lock_from_remote(void)
{
	FILE *f;
	int n, len, lpid;
	arg_list argl = NULL;
	unsigned int i, ret = 0;
	char *p, local[256], filename[32];

	if( !lock_microcom_conf_access() )
		return 0;
	if( (lpid = get_microcom_lock_pid()) < 2 )
		goto err;
	sprintf(filename, "/proc/%d/cmdline", lpid);
	if( (f = fopen(filename, "r")) == NULL )
		goto err;
	if( (len = fread(local, 1, 255, f)) <= 0 ) {
		fclose(f);
		goto err;
	}
	fclose(f);
	local[len] = 0;
	for( i=0, p=local; i < len; i++, p++ ) {
		if( isprint(*p) == 0 )
			*p = ' ';
	}
	if( (n = parse_args_din(local, &argl)) > 0 ) {
		for( i=0; i < n; i++ ) {
			if( (strcmp(argl[i], "-h") == 0) || (strcmp(argl[i], "dmview_management") == 0) ) {
				ret = 1;
				break;
			}
		}
	}
	free_args_din(&argl);

err:
	unlock_microcom_conf_access();
	return ret;
}

unsigned int get_vcli_pid(void)
{
	FILE *f;
	char *p, local[64];
	arg_list argl = NULL;
	unsigned int pid = 0;

	if( (f = fopen(FILE_VCLI_PID, "r")) == NULL )
		return 0;
	memset(local, 0, 64);
	if( fread(local, 1, 63, f) <= 0 ) {
		fclose(f);
		return 0;
	}
	fclose(f);
	if( parse_args_din(local, &argl) > 0 ) {
		for( p=argl[0], pid=1; *p; p++ ) {
			if( isdigit(*p) == 0 ) {
				pid = 0;
				break;
			}
		}
	}
	if( pid == 0 ) {
		free_args_din(&argl);
		return 0;
	}
	pid = atoi(argl[0]);
	free_args_din(&argl);
	return pid;
}

int transfer_microcom_lock_to_us(void)
{
	int lpid, ret = -1;

	if( !lock_microcom_conf_access() )
		return -1;
	if( (lpid = get_microcom_lock_pid()) < 0 )
		goto err;
	unlock_microcom_conf_access();
	if( lpid > 1 ) { /* There is a lock */
		if( getpid() == lpid )
			return 0;
		if( get_vcli_pid() > 1 ) /* dmview management is up */
			kill(lpid, SIGTERM);
		else
			kill(lpid, SIGQUIT);
		sleep(2);
	}
	if( !lock_microcom_conf_access() )
		return -1;
	if( remove_microcom_lock() < 0 )
		goto err;
	if( do_microcom_lock() < 0 )
		goto err;
	ret = 0;

err:
	unlock_microcom_conf_access();
	return ret;
}

int get_process_fd_device(pid_t pid, int f_d, char *store, unsigned int max_len)
{
	FILE *f;
	int fd, found = 0;
	arg_list argl = NULL;
	char buf[128], filename[] = "/var/run/cish_stdio_tests_XXXXXX";

	if( (store == NULL) || (max_len == 0) )
		return -1;
	*store = 0;
	if( (fd = mkstemp(filename)) == -1 )
		return -1;
	close(fd);
	sprintf(buf, "ls -l /proc/%d/fd/ > %s", pid, filename);
	system(buf);
	if( (f = fopen(filename, "r")) == NULL ) {
		remove(filename);
		return -1;
	}
	while( !feof(f) && !found ) {
		if( fgets(buf, 127, f) != buf )
			break;
		buf[127] = 0;
		if( parse_args_din(buf, &argl) == 8 ) {
			if( atoi(argl[5]) == f_d ) {
				strncpy(store, argl[7], max_len-1);
				store[max_len-1] = 0;
				found = 1;
			}
		}
		free_args_din(&argl);
	}
	fclose(f);
	remove(filename);
	return (found ? 0 : -1);
}

unsigned int get_led_state(char *led_name)
{
	ppcio_data pd;

	read_ppcio(&pd);
	if( strcmp(led_name, "led_sys") == 0 )
		return (pd.nled_sys == 0 ? 1 : 0);
#ifdef CONFIG_BERLIN_SATROUTER
	if( strcmp(led_name, "wan_status") == 0 )
		return (pd.wan_st == 0 ? 1 : 0);
#endif
	return 0;
}

#ifdef CONFIG_BERLIN_SATROUTER
void generate_init_prompt(char *product_ident, char *store, unsigned int max_len)
{
	char buf[64], lvendor[50];

	gethostname(buf, sizeof(buf)-1);
	buf[sizeof(buf)-1] = 0;
	if( get_mb_info(MBINFO_VENDOR, lvendor, 50) ) {
		if( !strcasecmp(lvendor, "eletech") ) {
			switch( get_board_hw_id() ) {
				case BOARD_HW_ID_0:
					snprintf(store, max_len-1, "ET991CR-%s", buf);
					break;
				case BOARD_HW_ID_1:
					snprintf(store, max_len-1, "ET706CR-%s", buf);
					break;
				case BOARD_HW_ID_2:
					snprintf(store, max_len-1, "ET991CR-%s", buf);
					break;
				case BOARD_HW_ID_3:
					snprintf(store, max_len-1, "ET706CS-%s", buf);
					break;
				case BOARD_HW_ID_4:
					snprintf(store, max_len-1, "ET991CS-%s", buf);
					break;
				default:
					snprintf(store, max_len-1, "%s-%s", product_ident, buf);
					break;
			}
		}
		else
			snprintf(store, max_len-1, "%s-%s", product_ident, buf);
	}
	else
		snprintf(store, max_len-1, "%s-%s", product_ident, buf);
}
#endif
#endif
