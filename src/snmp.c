/*
 * snmp.h
 *
 *  Created on: Jun 24, 2010
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "defines.h"
#include "args.h"
#include "error.h"
#include "str.h"
#include "exec.h"
#include "process.h"
#include "str.h"
#include "snmp.h"
#include "mib.h"
#include "lock.h"
#include "nv.h"

struct trap_sink {
	char *ip_addr;
	char *community;
	char *port;
};

oid objid_sysuptime[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };
oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };

int libconfig_snmp_get_contact(char *buffer, int max_len)
{
	return libconfig_str_find_string_in_file(FILE_SNMPD_CONF,
			"syscontact", buffer, max_len);
}

int libconfig_snmp_get_location(char *buffer, int max_len)
{
	return libconfig_str_find_string_in_file(FILE_SNMPD_CONF,
	                "syslocation", buffer, max_len);
}

int libconfig_snmp_set_contact(char *contact)
{
	return libconfig_str_replace_string_in_file(FILE_SNMPD_CONF,
	                "syscontact", contact);
}

int libconfig_snmp_set_location(char *location)
{
	return libconfig_str_replace_string_in_file(FILE_SNMPD_CONF,
	                "syslocation", location);
}

int libconfig_snmp_reload_config(void)
{
	pid_t pid;

	pid = libconfig_process_get_pid(PROG_SNMPD);

	if (pid) {
		kill(pid, SIGHUP);
		libconfig_snmp_rmon_send_signal(SIGUSR1);
	}

	return 0;
}

#define SNMP_DAEMON "snmpd"

int libconfig_snmp_is_running(void)
{
	return libconfig_exec_check_daemon(SNMP_DAEMON);
}

int libconfig_snmp_start(void)
{
	int ret;

	if (!libconfig_snmp_is_running()) {
		ret = libconfig_exec_daemon(SNMP_DAEMON);

		if (ret != -1)
			libconfig_snmp_rmon_send_signal(SIGUSR1);

		return ret;
	}

	return 0;
}

int libconfig_snmp_stop(void)
{
	if (libconfig_snmp_is_running()) {
#if 1
		int ret;
		pid_t pid;
		time_t timeout;

		pid = libconfig_process_get_pid(SNMP_DAEMON);

		if ((ret = libconfig_kill_daemon(SNMP_DAEMON)) == 0) {
			for (timeout = time(NULL) + 10; (libconfig_process_get_pid(SNMP_DAEMON) == pid) && (time(NULL) < timeout);)
				usleep(100000);
		}

		libconfig_snmp_rmon_send_signal(SIGUSR1);

		return ret;
#else
		return libconfig_kill_daemon(SNMP_DAEMON);
#endif
	}

	return 0;
}

int libconfig_snmp_set_community(const char *community_name, int add_del, int ro)
{
	char buf[1024];
	FILE *conf, *new_conf;

	conf = fopen(FILE_SNMPD_CONF, "r");
	if (!conf) {
		libconfig_pr_error(0, "Could not configure SNMP server");
		return (-1);
	}

	new_conf = fopen(FILE_SNMPD_CONF".new", "w");
	if (!new_conf) {
		libconfig_pr_error(0, "Could not reconfigure SNMP server");
		fclose(conf);
		return (-1);
	}

	while (!feof(conf)) {

		buf[0] = 0;
		if (fgets(buf, 1023, conf) == NULL)
			break;

		buf[1023] = 0;

		libconfig_str_striplf(buf);

		if (strncmp(buf, ro ? "rocommunity" : "rwcommunity", 11) == 0) {
			/* don't copy ours */
			if (!strcmp(buf + 12, community_name)) {

				if (!add_del) {
					/* delete community */
					continue;
				}

				/* community already configured */
				add_del = 0;
			}
		}

		fprintf(new_conf, "%s\n", buf);
	}

	if (add_del) {
		/* add community */
		fprintf(new_conf, "r%ccommunity %s\n", ro ? 'o' : 'w', community_name);
	}

	fclose(conf);
	fclose(new_conf);

	unlink(FILE_SNMPD_CONF);
	rename(FILE_SNMPD_CONF".new", FILE_SNMPD_CONF);

	return 0;
}

int libconfig_snmp_dump_communities(FILE *out)
{
	char buf[1024];
	int count = 0;
	FILE *conf;

	conf = fopen(FILE_SNMPD_CONF, "r");
	if (!conf) {
		libconfig_pr_error(0,
		                "Could not read SNMP server configuration");
		return (-1);
	}

	while (!feof(conf)) {

		buf[0] = 0;
		if (fgets(buf, 1023, conf) == NULL)
			break;

		buf[1023] = 0;
		libconfig_str_striplf(buf);

		if ((strncmp(buf, "rocommunity", 11) == 0) || (strncmp(buf, "rwcommunity", 11) == 0)) {
			fprintf(out, "snmp-server community %s r%c\n", buf + 12, buf[1]);
			count++;
		}
	}

	fclose(conf);
	return count;
}

int libconfig_snmp_dump_versions(FILE *out)
{
	int count = 0;
	char *p1, *p2, *p3, buf[8];

	if (libconfig_exec_get_inittab_lineoptions(PROG_SNMPD, "-J", buf, 8) > 0) {

		p1 = strstr(buf, "1");
		p2 = strstr(buf, "2c");
		p3 = strstr(buf, "3");

		if ((p1 != NULL) || (p2 != NULL) || (p3 != NULL)) {

			fprintf(out, "snmp-server version");

			if (p1 != NULL) {
				fprintf(out, " 1");
				count++;
			}

			if (p2 != NULL) {
				fprintf(out, " 2");
				count++;
			}

			if (p3 != NULL) {
				fprintf(out, " 3");
				count++;
			}

			fprintf(out, "\n");
		}
	}

	return count;
}

int libconfig_snmp_add_trapsink(char *addr, char *community)
{
	int fd, left;
	FILE *conf;
	struct stat st;
	char *p, *local, line[201];

	if ((fd = open(FILE_SNMPD_CONF, O_RDONLY)) < 0) {
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	if (fstat(fd, &st) < 0) {
		close(fd);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	/* space for new line! */
	st.st_size += 200;
	if (!(local = malloc(st.st_size))) {
		close(fd);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	local[0] = '\0';
	close(fd);

	if (!(conf = fopen(FILE_SNMPD_CONF, "r"))) {
		free(local);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	/* read with \n */
	while (fgets(line, 200, conf)) {
		if ((p = strstr(line, "trapsink"))) {
			if (strstr(p + 8, addr)) {
				/* match! overwrite community! */
				continue;
			}
		}

		left = st.st_size - strlen(local);
		strncat(local, line, left);
	}

	fclose(conf);
	left = st.st_size - strlen(local);

	snprintf(local + strlen(local), left, "trapsink %s %s 162\n", addr, community);

	remove(FILE_SNMPD_CONF);

	if ((fd = open(FILE_SNMPD_CONF, O_WRONLY | O_CREAT, st.st_mode)) < 0) {
		free(local);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	write(fd, local, strlen(local));
	close(fd);

	free(local);

	return 0;
}

int libconfig_snmp_del_trapsink(char *addr)
{
	int fd, left;
	FILE *conf;
	struct stat st;
	char *p, *local, line[201];

	if ((fd = open(FILE_SNMPD_CONF, O_RDONLY)) < 0) {
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	if (fstat(fd, &st) < 0) {
		close(fd);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	if (!(local = malloc(st.st_size))) {
		close(fd);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	local[0] = '\0';
	close(fd);

	if (!(conf = fopen(FILE_SNMPD_CONF, "r"))) {
		free(local);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	/* read with \n */
	while (fgets(line, 200, conf)) {
		if ((p = strstr(line, "trapsink"))) {
			/* match! */
			if (strstr(p + 8, addr)) {
				continue;
			}
		}

		left = st.st_size - strlen(local);
		strncat(local, line, left);
	}

	fclose(conf);

	remove(FILE_SNMPD_CONF);

	if ((fd = open(FILE_SNMPD_CONF, O_WRONLY | O_CREAT, st.st_mode)) < 0) {
		free(local);
		libconfig_pr_error(0, "Could not read SNMP configuration");
		return -1;
	}

	write(fd, local, strlen(local));
	close(fd);

	free(local);

	return 0;
}

int libconfig_snmp_get_trapsinks(char ***sinks)
{
	FILE *f;
	int i, len = 0;
	char *p, *aux, **list, line[200];

	*sinks = NULL;
	if ((f = fopen(FILE_SNMPD_CONF, "r")) == 0)
		return -1;

	while (fgets(line, 200, f)) {
		if (strstr(line, "trapsink"))
			len++;
	}

	if (len == 0)
		return 0;

	if ((list = malloc((len) * sizeof(char *))) == NULL) {
		fclose(f);
		return -1;
	}

	for (i = 0; i < len; i++)
		list[i] = NULL;

	fseek(f, 0, SEEK_SET);

	for (i = 0; (i < len) && fgets(line, 200, f);) {
		if ((p = strstr(line, "trapsink"))) {

			for (p += 8; (*p == ' ') || (*p == '\t'); p++)
				;

			if (*p != '\0') {

				for (aux = p; (*p == '.') || (isdigit(*p) != 0); p++)
					;

				for (; *p == ' '; p++)
					;

				for (; (*p != ' ') && (*p != '\0'); p++)
					;

				*p = '\0';

				if ((list[i] = malloc(strlen(aux) + 1)))
					strcpy(list[i++], aux);
			}
		}
	}

	fclose(f);

	if (i == 0) {
		free(list);
		return 0;
	}

	*sinks = list;

	return i;
}

int libconfig_snmp_itf_should_sendtrap(char *itf)
{
	FILE *f;
	int found = 0;
	char *p, *aux, buf[100];

	if ((f = fopen(TRAPCONF, "r"))) {
		while (fgets(buf, 100, f)) {
			for (aux = buf; *aux == ' '; aux++)
				;
			if ((p = strchr(aux, ' ')))
				*p = '\0';
			if ((p = strchr(aux, '#')))
				*p = '\0';
			if ((p = strchr(aux, '\n')))
				*p = '\0';
			if (strcmp(itf, aux) == 0) {
				found++;
				break;
			}
		}
		fclose(f);
	}
	return found;
}

int libconfig_snmp_input(int operation,
               netsnmp_session * session,
               int reqid,
               netsnmp_pdu *pdu,
               void *magic)
{
	return 1;
}

int libconfig_snmp_rmon_event_log(int index, char *description)
{
	char *buf;
	time_t the_time;
	struct tm *tm_ptr;

	if ((buf = malloc(32 + strlen(description))) == NULL)
		return -1;

	time(&the_time);
	tm_ptr = gmtime(&the_time);

	sprintf(buf, "%d %02d:%02d:%02d %s\n", index, tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, description);

	if (libconfig_exec_test_write_len(FILE_RMON_LOG_EVENTS, FILE_RMON_LOG_EVENTS_MAX_SIZE, buf, strlen(buf)) < 0) {
		free(buf);
		return -1;
	}

	free(buf);
	syslog(LOG_WARNING, "Event %d generated", index);

	return 0;
}

static int _rmon_create_shm(void)
{
	int shm_confid;
	void *shm_confid_l = (void *) 0;

	if ((shm_confid = shmget((key_t) RMON_SHM_KEY, sizeof(struct rmon_config), 0666)) != -1) {
		/* A memoria compartilhada jah havia sido criada anteriormente e nao precisamos fazer mais nada */
		return 0;
	}

	/* Como a memoria compartilhada ainda nao existe no sistema, nos a criamos */
	if ((shm_confid = shmget((key_t) RMON_SHM_KEY, sizeof(struct rmon_config), 0666 | IPC_CREAT)) == -1) {
		syslog(LOG_ERR, "Could not alloc memory for rmond");
		return -1;
	}

	if ((shm_confid_l = shmat(shm_confid, (void *) 0, 0)) == (void *) -1) {
		syslog(LOG_ERR, "Could not link memory for rmond");
		return -1;
	}

	memset((char *) shm_confid_l, '\0', sizeof(struct rmon_config));
	return 0;
}

int libconfig_snmp_rmon_get_access_cfg(struct rmon_config **shm_rmon_p)
{
	int shm_confid;
	void *shm_confid_l = (void *) 0;

	if (libconfig_lock_rmon_config_access()) {
		/* Point to shared memory */
		if ((shm_confid = shmget((key_t) RMON_SHM_KEY, sizeof(struct rmon_config), 0666)) == -1) {

			if (_rmon_create_shm() < 0) {
				libconfig_unlock_rmon_config_access();
				return 0;
			}

			if ((shm_confid = shmget((key_t) RMON_SHM_KEY, sizeof(struct rmon_config), 0666)) == -1) {
				libconfig_unlock_rmon_config_access();
				return 0;
			}
		}

		if ((shm_confid_l = shmat(shm_confid, (void *) 0, 0)) == (void *) -1) {
			libconfig_unlock_rmon_config_access();
			return 0;
		}

		*shm_rmon_p = (struct rmon_config *) shm_confid_l;
		return 1;
	}

	return 0;
}

int libconfig_snmp_rmon_free_access_cfg(struct rmon_config **shm_rmon_p)
{
	int ret = 1;

	if (shmdt((void *) *shm_rmon_p) == -1)
		ret = 0;

	if (libconfig_unlock_rmon_config_access() == 0)
		ret = 0;

	*shm_rmon_p = NULL;

	return ret;
}

int libconfig_snmp_rmon_add_event(int num,
                                  int log,
                                  char *community,
                                  int status,
                                  char *descr,
                                  char *owner)
{
	struct rmon_config *shm_rmon_p;
	int i, found = 0, ret_value = -1;

	if (libconfig_snmp_rmon_get_access_cfg(&shm_rmon_p)) {

		for (i = 0; i < NUM_EVENTS; i++) {
			if (shm_rmon_p->events[i].index == num) {
				found++;
				break;
			}
		}

		if (found) {
			shm_rmon_p->events[i].do_log = log;

			if (community) {
				strncpy(shm_rmon_p->events[i].community, community, 256);
				shm_rmon_p->events[i].community[255] = 0;
			} else {
				shm_rmon_p->events[i].community[0] = 0;
			}

			shm_rmon_p->events[i].last_time_sent = 0;
			shm_rmon_p->events[i].status = status;

			if (descr) {
				strncpy(shm_rmon_p->events[i].description, descr, 256);
				shm_rmon_p->events[i].description[255] = 0;
			} else {
				shm_rmon_p->events[i].description[0] = 0;
			}

			if (owner) {
				strncpy(shm_rmon_p->events[i].owner, owner, 256);
				shm_rmon_p->events[i].owner[255] = 0;
			} else {
				shm_rmon_p->events[i].owner[0] = 0;
			}

			ret_value = 0;
		} else {

			for (i = 0, found = 0; i < NUM_EVENTS; i++) {

				if (shm_rmon_p->events[i].index == 0) {
					shm_rmon_p->events[i].index = num;
					shm_rmon_p->events[i].do_log = log;
					if (community) {
						strncpy(shm_rmon_p->events[i].community, community, 256);
						shm_rmon_p->events[i].community[255] = 0;
					} else {
						shm_rmon_p->events[i].community[0] = 0;
					}

					shm_rmon_p->events[i].last_time_sent = 0;
					shm_rmon_p->events[i].status = status;

					if (descr) {
						strncpy(shm_rmon_p->events[i].description, descr, 256);
						shm_rmon_p->events[i].description[255] = 0;
					} else {
						shm_rmon_p->events[i].description[0] = 0;
					}

					if (owner) {
						strncpy(shm_rmon_p->events[i].owner, owner, 256);
						shm_rmon_p->events[i].owner[255] = 0;
					} else {
						shm_rmon_p->events[i].owner[0] = 0;
					}
					found++;
					break;
				}
			}

			if (found)
				ret_value = 0;
		}

		libconfig_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	return ret_value;
}

int libconfig_snmp_rmon_add_alarm(int num,
                                  char *var_oid,
                                  oid *name,
                                  size_t namelen,
                                  int interval,
                                  int var_type,
                                  int rising_th,
                                  int rising_event,
                                  int falling_th,
                                  int falling_event,
                                  int status,
                                  char *owner)
{
	struct rmon_config *shm_rmon_p;
	int i, k, found, ret_value = -1;

	if (libconfig_snmp_rmon_get_access_cfg(&shm_rmon_p)) {

		for (i = 0, found = 0; i < NUM_ALARMS; i++) {
			if (shm_rmon_p->alarms[i].index == num) {
				found++;
				break;
			}
		}

		if (found) {
			shm_rmon_p->alarms[i].interval = interval;

			if (var_oid) {
				strncpy(shm_rmon_p->alarms[i].str_oid, var_oid, 256);
				shm_rmon_p->alarms[i].str_oid[255] = 0;
			} else {
				shm_rmon_p->alarms[i].str_oid[0] = 0;
			}

			for (k = 0; k < MAX_OID_LEN; k++)
				shm_rmon_p->alarms[i].oid[k] = name[k];

			shm_rmon_p->alarms[i].oid_len = namelen;
			shm_rmon_p->alarms[i].sample_type = var_type;
			shm_rmon_p->alarms[i].rising_threshold = rising_th;
			shm_rmon_p->alarms[i].rising_event_index = rising_event;
			shm_rmon_p->alarms[i].falling_threshold = falling_th;
			shm_rmon_p->alarms[i].falling_event_index = falling_event;
			shm_rmon_p->alarms[i].status = status;

			if (owner) {
				strncpy(shm_rmon_p->alarms[i].owner, owner, 256);
				shm_rmon_p->alarms[i].owner[255] = 0;
			} else {
				shm_rmon_p->alarms[i].owner[0] = 0;
			}

			ret_value = 0;
		} else {

			for (i = 0, found = 0; i < NUM_ALARMS; i++) {

				if (shm_rmon_p->alarms[i].index == 0) {
					shm_rmon_p->alarms[i].index = num;
					shm_rmon_p->alarms[i].interval = interval;

					if (var_oid) {
						strncpy(shm_rmon_p->alarms[i].str_oid, var_oid, 256);
						shm_rmon_p->alarms[i].str_oid[255] = 0;
					}

					for (k = 0; k < MAX_OID_LEN; k++)
						shm_rmon_p->alarms[i].oid[k] = name[k];

					shm_rmon_p->alarms[i].oid_len = namelen;
					shm_rmon_p->alarms[i].sample_type = var_type;
					shm_rmon_p->alarms[i].rising_threshold = rising_th;
					shm_rmon_p->alarms[i].rising_event_index = rising_event;
					shm_rmon_p->alarms[i].falling_threshold = falling_th;
					shm_rmon_p->alarms[i].falling_event_index = falling_event;
					shm_rmon_p->alarms[i].status = status;

					if (owner) {
						strncpy(shm_rmon_p->alarms[i].owner, owner, 256);
						shm_rmon_p->alarms[i].owner[255] = 0;
					} else {
						shm_rmon_p->alarms[i].owner[0] = 0;
					}

					found++;
					break;
				}
			}

			if (found)
				ret_value = 0;
		}

		libconfig_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	return ret_value;
}

int libconfig_snmp_rmon_remove_event(char *index)
{
	struct rmon_config *shm_rmon_p;
	int i, idx, found = 0, ret_value = -1;

	if (libconfig_snmp_rmon_get_access_cfg(&shm_rmon_p)) {

		if (index) {
			idx = atoi(index);
			for (i = 0; i < NUM_EVENTS; i++) {

				if (shm_rmon_p->events[i].index == idx) {

					shm_rmon_p->events[i].index = 0;
					shm_rmon_p->events[i].do_log = 0;
					shm_rmon_p->events[i].community[0] = '\0';
					shm_rmon_p->events[i].last_time_sent = 0;
					shm_rmon_p->events[i].status = 0;
					shm_rmon_p->events[i].description[0] = '\0';
					shm_rmon_p->events[i].owner[0] = '\0';

					found++;
					break;
				}
			}

			if (found)
				ret_value = 0;
		} else {

			for (i = 0; i < NUM_EVENTS; i++) {
				shm_rmon_p->events[i].index = 0;
				shm_rmon_p->events[i].do_log = 0;
				shm_rmon_p->events[i].community[0] = '\0';
				shm_rmon_p->events[i].last_time_sent = 0;
				shm_rmon_p->events[i].status = 0;
				shm_rmon_p->events[i].description[0] = '\0';
				shm_rmon_p->events[i].owner[0] = '\0';
			}

			ret_value = 0;
		}

		libconfig_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	return ret_value;
}

int libconfig_snmp_rmon_remove_alarm(char *index)
{
	struct rmon_config *shm_rmon_p;
	int i, idx, found = 0, ret_value = -1;

	if (libconfig_snmp_rmon_get_access_cfg(&shm_rmon_p)) {

		if (index) {
			idx = atoi(index);
			for (i = 0; i < NUM_ALARMS; i++) {

				if (shm_rmon_p->alarms[i].index == idx) {
					memset(&shm_rmon_p->alarms[i], 0,
					                sizeof(struct rmon_alarm_entry));
					found++;
					break;
				}
			}

			if (found)
				ret_value = 0;
		} else {
			memset(&shm_rmon_p->alarms[0], 0, NUM_ALARMS * sizeof(struct rmon_alarm_entry));
			ret_value = 0;
		}

		libconfig_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	return ret_value;
}

void libconfig_snmp_rmon_event_info(struct rmon_config *shm_rmon_p, int i, int idx)
{
	FILE *f;
	arg_list argl = NULL;
	int j, args, count, exist;
	char tmp[5], line[300], last[300] = "";

	if (idx == 0)
		return;

	if (shm_rmon_p->events[i].status)
		printf("Event %d is active", idx);
	else
		printf("Event %d is disabled", idx);

	if (strlen(shm_rmon_p->events[i].owner))
		printf(", owned by %s", shm_rmon_p->events[i].owner);

	printf("\n");

	if (strlen(shm_rmon_p->events[i].description))
		printf("  Description is %s\n", shm_rmon_p->events[i].description);
	else
		printf("  No description\n");

	if (shm_rmon_p->events[i].do_log || strlen(shm_rmon_p->events[i].community)) {

		printf("  Event firing causes ");

		if (shm_rmon_p->events[i].do_log) {
			printf("log");

			if (strlen(shm_rmon_p->events[i].community))
				printf(" and trap to community %s", shm_rmon_p->events[i].community);
		} else {
			if (strlen(shm_rmon_p->events[i].community))
				printf("trap to community %s", shm_rmon_p->events[i].community);
		}

		if (shm_rmon_p->events[i].do_log) {
			if ((f = fopen(FILE_RMON_LOG_EVENTS, "r"))) {

				while (fgets(line, 300, f)) {
					if (libconfig_parse_args_din(line, &argl) > 1) {
						if (atoi(argl[0]) == idx)
							strcpy(last, line);
					}

					libconfig_destroy_args_din(&argl);
				}

				fclose(f);
			}
		}

		if (last[0]) {
			if (libconfig_parse_args_din(last, &argl) > 1)
				printf(", last fired %s", argl[1]);

			libconfig_destroy_args_din(&argl);
		}

		printf("\n");
	}

	if ((f = fopen(FILE_RMON_LOG_EVENTS, "r"))) {
		for (exist = 0; (exist == 0) && fgets(line, 300, f);) {
			if (libconfig_parse_args_din(line, &argl) > 1) {
				if (atoi(argl[0]) == idx)
					exist++;
			}
			libconfig_destroy_args_din(&argl);
		}

		if (exist) {
			fseek(f, 0, SEEK_SET);
			printf("  Current log entries:\n");
			printf("     Index     Time      Description\n");

			for (count = 1; fgets(line, 300, f);) {
				if ((args = libconfig_parse_args_din(line, &argl)) > 1) {
					if (atoi(argl[0]) == idx) {
						sprintf(tmp, "%d", count);
						printf("\033[%dC%s   %s   ", 10 - strlen(tmp),tmp, argl[1]);

						for (j = 2; j < args; j++)
							printf(" %s", argl[j]);

						printf("\n");
						count++;
					}
				}

				libconfig_destroy_args_din(&argl);
			}
		} else {
			printf("  No log entries\n");
		}

		fclose(f);
	}
}

int libconfig_snmp_rmon_show_event(char *index)
{
	int i, idx = 0, ret = -1;
	struct rmon_config *shm_rmon_p;

	if (libconfig_snmp_rmon_get_access_cfg(&shm_rmon_p)) {

		if (index)
			idx = atoi(index);
		else
			ret = 0;

		for (i = 0; i < NUM_EVENTS; i++) {
			if (idx) {
				if (shm_rmon_p->events[i].index == idx) {
					libconfig_snmp_rmon_event_info(shm_rmon_p, i, shm_rmon_p->events[i].index);
					ret = 0;
					break;
				}
			} else {
				libconfig_snmp_rmon_event_info(shm_rmon_p, i, shm_rmon_p->events[i].index);
			}
		}
		libconfig_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	return ret;
}

void libconfig_snmp_rmon_alarm_info(struct rmon_config *shm_rmon_p, int i, int idx)
{
	char buf[50];

	if (idx == 0)
		return;

	if (shm_rmon_p->alarms[i].status)
		printf("Alarm %d is active", idx);
	else
		printf("Alarm %d is disabled", idx);

	if (strlen(shm_rmon_p->alarms[i].owner))
		printf(", owned by %s", shm_rmon_p->alarms[i].owner);

	printf("\n");

	if (strlen(shm_rmon_p->alarms[i].str_oid) && shm_rmon_p->alarms[i].interval)
		printf("  Monitors %s every %d second(s)\n",
		                libconfig_snmp_oid_to_obj_name(shm_rmon_p->alarms[i].str_oid, buf, 50) ? buf : shm_rmon_p->alarms[i].str_oid,
		                shm_rmon_p->alarms[i].interval);

	switch (shm_rmon_p->alarms[i].sample_type) {
	case SAMPLE_ABSOLUTE:
		printf("  Taking absolute samples, last value was ");
		break;
	case SAMPLE_DELTA:
		printf("  Taking delta samples, last value was ");
		break;
	}

	printf("%d\n", shm_rmon_p->alarms[i].last_value);

	if (shm_rmon_p->alarms[i].rising_threshold) {

		printf("  Rising threshold is %d, ", shm_rmon_p->alarms[i].rising_threshold);

		if (shm_rmon_p->alarms[i].rising_event_index)
			printf("assigned to event %d\n", shm_rmon_p->alarms[i].rising_event_index);
		else
			printf("not assigned to any event\n");

	} else {
		printf("  Rising threshold not defined\n");
	}

	if (shm_rmon_p->alarms[i].falling_threshold) {

		printf("  Falling threshold is %d, ", shm_rmon_p->alarms[i].falling_threshold);

		if (shm_rmon_p->alarms[i].falling_event_index)
			printf("assigned to event %d\n", shm_rmon_p->alarms[i].falling_event_index);
		else
			printf("not assigned to any event\n");

	} else {
		printf("  Falling threshold not defined\n");
	}
}

int libconfig_snmp_rmon_show_alarm(char *index)
{
	int i, idx = 0, ret = -1;
	struct rmon_config *shm_rmon_p;

	if (libconfig_snmp_rmon_get_access_cfg(&shm_rmon_p)) {

		if (index)
			idx = atoi(index);
		else
			ret = 0;

		for (i = 0; i < NUM_ALARMS; i++) {
			if (idx) {
				if (shm_rmon_p->alarms[i].index == idx) {

					libconfig_snmp_rmon_alarm_info(
					                shm_rmon_p,
					                i,
					                shm_rmon_p->alarms[i].index);
					ret = 0;
					break;
				}
			} else {
				libconfig_snmp_rmon_alarm_info(shm_rmon_p, i,
				                shm_rmon_p->alarms[i].index);
			}
		}

		libconfig_snmp_rmon_free_access_cfg(&shm_rmon_p);
	}

	return ret;
}

int libconfig_snmp_rmon_send_signal(int sig)
{
	pid_t pid = libconfig_process_get_pid(RMON_DAEMON);

	if (pid > 1) {
		if (kill(pid, sig) == 0)
			return 1;
	}

	return 0;
}

int libconfig_snmp_rmon_clear_events(void)
{
	FILE *f;

	if ((f = fopen(FILE_RMON_LOG_EVENTS, "w")) == NULL)
		return -1;

	fclose(f);
	return 0;
}

int libconfig_snmp_create_pdu_data(struct trap_data_obj **data_p)
{
	if ((*data_p = (struct trap_data_obj *) malloc(sizeof(struct trap_data_obj)))) {
		memset(*data_p, '\0', sizeof(struct trap_data_obj));
		return 0;
	}

	return -1;
}

int libconfig_snmp_add_pdu_data_entry(struct trap_data_obj **data_p,
                                      char *oid_str,
                                      int type,
                                      char *value)
{
	int idx;
	struct trap_data_obj *p, *new_r;

	if (*data_p == 0)
		return -1;

	for (idx = 0, p = *data_p; p[idx].oid; idx++)
		;

	if ((new_r = (struct trap_data_obj *) malloc((idx + 2) * sizeof(struct trap_data_obj)))) {

		memset(new_r, '\0', (idx + 2) * sizeof(struct trap_data_obj));

		if (idx)
			memcpy(new_r, *data_p, idx * sizeof(struct trap_data_obj));

		if ((new_r[idx].oid = malloc(strlen(oid_str) + 1))) {

			strcpy(new_r[idx].oid, oid_str);
			new_r[idx].type = type;

			if ((new_r[idx].value = malloc(strlen(value) + 1))) {
				strcpy(new_r[idx].value, value);
			} else {
				free(new_r[idx].oid);
				free(new_r);
				return -1;
			}

		} else {
			free(new_r);
			return -1;
		}

		free(*data_p);
		*data_p = new_r;
		return 0;
	}

	return -1;
}

int libconfig_snmp_destroy_pdu_data(struct trap_data_obj **data_p)
{
	struct trap_data_obj *p = *data_p;

	if (p == NULL)
		return -1;

	for (; p->oid; p++) {
		free(p->oid);
		if (p->value)
			free(p->value);
	}

	free(*data_p);
	*data_p = NULL;

	return 0;
}

int libconfig_snmp_sendtrap(char *snmp_trap_version,
                            char *community_rcv,
                            char *trap_obj_oid,
                            struct trap_data_obj *trap_data)
{
	FILE *f;
	netsnmp_pdu *pdu;
	size_t name_length;
	char type, buf[300];
	arg_list argl = NULL;
	oid name[MAX_OID_LEN];
	struct trap_sink *sinks;
	netsnmp_session *ss, session;
	int k, idx, args, errors, len = 0;

	if ((f = fopen(FILE_SNMPD_CONF, "r")) == NULL)
		return -1;

	while (fgets(buf, 300, f)) {
		if (libconfig_parse_args_din(buf, &argl) > 1) {
			if (strcmp(argl[0], "trapsink") == 0)
				len++;
		}
		libconfig_destroy_args_din(&argl);
	}

	if ((sinks = malloc(len * sizeof(struct trap_sink))) == NULL) {
		fclose(f);
		return -1;
	}

	for (k = 0; k < len; k++) {
		sinks[k].ip_addr = NULL;
		sinks[k].community = NULL;
		sinks[k].port = NULL;
	}

	fseek(f, 0, SEEK_SET);

	for (k = 0; fgets(buf, 300, f);) {
		if ((args = libconfig_parse_args_din(buf, &argl)) > 1) {
			if (strcmp(argl[0], "trapsink") == 0) {
				if ((sinks[k].ip_addr = malloc(strlen(argl[1]) + 1))) {

					strcpy(sinks[k].ip_addr, argl[1]);

					if (args >= 3) {
						if ((sinks[k].community = malloc(strlen(argl[2])+ 1))) {

							strcpy(sinks[k].community, argl[2]);

							if (args > 3) {
								if ((sinks[k].port = malloc(strlen(argl[3])+ 1)))
									strcpy(sinks[k].port, argl[3]);
							}
						}
					}
					k++;
				}
			}
		}
		libconfig_destroy_args_din(&argl);
	}

	fclose(f);

	for (idx = 0; idx < len; idx++) {
		/* initialize session to default values */
		snmp_sess_init(&session);

		/* trap version */
		if (strcmp(snmp_trap_version, "1") == 0)
			session.version = SNMP_VERSION_1;
		else if (strcmp(snmp_trap_version, "2c") == 0)
			session.version = SNMP_VERSION_2c;
		else if (strcmp(snmp_trap_version, "2u") == 0)
			session.version = SNMP_VERSION_2u;
		else if (strcmp(snmp_trap_version, "3") == 0)
			session.version = SNMP_VERSION_3;

		/* define remote host */
		session.peername = sinks[idx].ip_addr;

		/* UDP port number of peer */
		session.remote_port = strcmp(sinks[idx].port, "162") ? atoi(sinks[idx].port) : SNMP_DEFAULT_REMPORT;

		/* my UDP port number, 0 for default, picked randomly */
		session.local_port = 0;

		/* function to interpret incoming data */
		session.callback = libconfig_snmp_input;

		/* pointer to data that the callback function may consider important */
		session.callback_magic = NULL;

		/* community for outgoing requests */
		session.community = community_rcv ? ((unsigned char *) community_rcv) : ((unsigned char *) sinks[idx].community);

		/* length of community name */
		session.community_len = community_rcv ? strlen(community_rcv) : strlen( sinks[idx].community);

		SOCK_STARTUP;

		netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DEFAULT_PORT, SNMP_TRAP_PORT);

		/* open an SNMP session */
		if ((ss = snmp_open(&session))) {
			switch (session.version) {
			case SNMP_VERSION_1:
				/* nao implementado ate o momento */
				break;
			case SNMP_VERSION_2c:
			case SNMP_VERSION_2u:
			case SNMP_VERSION_3:
				pdu = snmp_pdu_create(SNMP_MSG_TRAP2);
				snmp_add_var(pdu, objid_sysuptime, sizeof(objid_sysuptime) / sizeof(oid), 't', "\"\"");

				if (snmp_add_var(pdu, objid_snmptrap, sizeof(objid_snmptrap) / sizeof(oid), 'o', trap_obj_oid)) {
					snmp_free_pdu(pdu);
					break;
				}

				for (k = 0, errors = 0; trap_data[k].oid; k++) {
					name_length = MAX_OID_LEN;
					memset(name, '\0', MAX_OID_LEN * sizeof(oid));

					if (libconfig_snmp_translate_oid( trap_data[k].oid, name, &name_length)) {

						type = '\0';

						switch (trap_data[k].type) {
						case ASN_INTEGER:
							type = 'i';
							break;
						case ASN_BIT_STR:
							type = 'b';
							break;
						case ASN_OCTET_STR:
							type = 's';
							break;
						case ASN_OBJECT_ID:
							type = 'o';
							break;
						case ASN_BOOLEAN:
						case ASN_NULL:
						case ASN_SEQUENCE:
						case ASN_SET:
						case ASN_UNIVERSAL:
						case ASN_APPLICATION:
						case ASN_CONTEXT:
						case ASN_PRIVATE:
						case ASN_CONSTRUCTOR:
						case ASN_EXTENSION_ID:
							break;
						}

						if (type) {
							if (snmp_add_var(pdu, name, name_length, type, trap_data[k].value) != 0)
								errors++;
						} else {
							errors++;
						}
					} else {
						errors++;
					}
				}

				if (errors)
					snmp_free_pdu(pdu);
				else if (snmp_send(ss, pdu) == 0)
					snmp_free_pdu(pdu);

				break;
			}

			snmp_close(ss);
		}

		SOCK_CLEANUP;

		if (sinks[idx].ip_addr)
			free(sinks[idx].ip_addr);

		if (sinks[idx].community)
			free(sinks[idx].community);

		if (sinks[idx].port)
			free(sinks[idx].port);
	}

	free(sinks);
	return 0;
}

static int _snmp_config_add_user(char *user, int rw, char *seclevel)
{
	FILE *f;
	char buf[256];
	int found = 0;
	arg_list argl = NULL;

	if (user == NULL)
		return -1;

	if ((f = fopen(FILE_SNMPD_DATA_CONF, "r")) != NULL) {

		while ((feof(f) == 0) && (found == 0)) {

			if (fgets(buf, 255, f) != NULL) {
				buf[255] = 0;

				if (libconfig_parse_args_din(buf, &argl) == 3) {

					if (((strcmp(argl[0], "rwuser") == 0) ||
						(strcmp(argl[0], "rouser") == 0)) && (strcmp(argl[1], user) == 0))
						found = 1;
				}

				libconfig_destroy_args_din(&argl);
			}
		}

		fclose(f);

		if (found == 1) {
			if (libconfig_exec_replace_string_file( FILE_SNMPD_DATA_CONF, buf, "") < 0)
				return -1;
		}
	}

	if ((f = fopen(FILE_SNMPD_DATA_CONF, "a")) == NULL)
		return -1;

	snprintf(buf, 127, "%s %s %s\n", ((rw == 1) ? "rwuser" : "rouser"), user, seclevel);

	if (fwrite(buf, 1, strlen(buf), f) != strlen(buf)) {
		fclose(f);
		return -1;
	}

	fclose(f);

	return 0;
}

static int _snmp_config_remove_user(char *user)
{
	FILE *f;
	char buf[256];
	int found = 0;
	arg_list argl = NULL;

	if (user == NULL)
		return -1;

	if ((f = fopen(FILE_SNMPD_DATA_CONF, "r")) == NULL)
		return 0; /* Nada precisa ser feito */

	while ((feof(f) == 0) && (found == 0)) {

		if (fgets(buf, 255, f) != NULL) {
			buf[255] = 0;
			if (libconfig_parse_args_din(buf, &argl) == 3) {
				if (((strcmp(argl[0], "rwuser") == 0) ||
					(strcmp(argl[0], "rouser")== 0)) && (strcmp(argl[1], user) == 0))
					found = 1;
			}

			libconfig_destroy_args_din(&argl);
		}
	}

	fclose(f);

	if (found == 1) {
		if (libconfig_exec_replace_string_file(FILE_SNMPD_DATA_CONF, buf, "") < 0)
			return -1;
	}

	return 0;
}

static int _snmp_adjust_fdb(unsigned int add, char *line, int rw)
{
	FILE *f;
	char buf[256];
	arg_list argl = NULL;
	int n, ret = -1, found = 0;

	if (line == NULL)
		return -1;

	switch (add) {
	case 0:
		if ((f = fopen(SNMP_USERKEY_FILE, "r")) != NULL) {

			while ((feof(f) == 0) && (found == 0)) {

				if (fgets(buf, 255, f) != NULL) {
					buf[255] = 0;

					if (libconfig_parse_args_din(buf, &argl) >= 2) {
						if ((strcmp(argl[0], "createUser") == 0) &&
							(strcmp(argl[1], line) == 0) &&
							(strchr(buf, '\n') != NULL))
							found = 1;
					}

					libconfig_destroy_args_din(&argl);
				}
			}

			fclose(f);
		}

		if (found == 1) {
			/* Ajusta arquivo SNMP_USERKEY_FILE */
			libconfig_exec_replace_string_file(SNMP_USERKEY_FILE, buf, "");

			/* Ajusta arquivo FILE_SNMPD_STORE_CONF */
			libconfig_exec_copy_file(SNMP_USERKEY_FILE, FILE_SNMPD_STORE_CONF);
		}

		/* Ajusta arquivo FILE_SNMPD_DATA_CONF */
		_snmp_config_remove_user(line);

		ret = 0;
		break;

	case 1:
		/* Ajusta arquivo SNMP_USERKEY_FILE */
		if ((f = fopen(SNMP_USERKEY_FILE, "a")) == NULL)
			break;

		if (fwrite(line, 1, strlen(line), f) != strlen(line)) {
			fclose(f);
			break;
		}

		fclose(f);

		/* Ajusta arquivo FILE_SNMPD_STORE_CONF */
		if (libconfig_exec_copy_file(SNMP_USERKEY_FILE, FILE_SNMPD_STORE_CONF) < 0)
			break;

		/* Ajusta arquivo FILE_SNMPD_DATA_CONF */
		found = 0;
		if ((n = libconfig_parse_args_din(line, &argl)) >= 2) {
			if ((strcmp(argl[0], "createUser") == 0) && (strchr(line, '\n') != NULL)) {
				if (_snmp_config_add_user(argl[1], rw,
				                ((n == 3) ? "noauth" : ((n == 5) ? "auth" : "priv"))) >= 0)
					found = 1;
			}
		}

		libconfig_destroy_args_din(&argl);

		if (found == 0)
			break;

		ret = 0;

		break;
	}

	return ((ret == 0) ? libconfig_nv_save_snmp_secret(SNMP_USERKEY_FILE) : ret);
}

int libconfig_snmp_remove_user(char *user)
{
	int ret = 0, is_running = libconfig_snmp_is_running() ? 1 : 0;

	if (user == NULL)
		return -1;

	if (is_running == 1)
		libconfig_snmp_stop();

	ret = _snmp_adjust_fdb(0, user, 0);

	if (is_running == 1)
		libconfig_snmp_start();
	else
		libconfig_snmp_rmon_send_signal(SIGUSR1);

	return ret;
}

int libconfig_snmp_add_user(char *user,
                            int rw,
                            char *authpriv,
                            char *authproto,
                            char *privproto,
                            char *authpasswd,
                            char *privpasswd)
{
	FILE *f;
	arg_list argl = NULL;
	char auth[8], buf[256];
	int len, ret = 0, found = 0, is_running = libconfig_snmp_is_running() ? 1 : 0;

	if (is_running == 1)
		libconfig_snmp_stop();

	if ((user == NULL) || (authpriv == NULL)) {
		ret = -1;
		goto error;
	}

	len = 6 + strlen("createUser ") + strlen(user)
	                + ((authproto != NULL) ? (7 + strlen(authpasswd)) : 0)
	                + ((privpasswd != NULL) ? (5 + strlen(privpasswd)) : 0);

	if (len > 256) {
		ret = -1;
		goto error;
	}

	/* Exclui user se este jah existir */
	if ((f = fopen(SNMP_USERKEY_FILE, "r")) != NULL) {

		while ((feof(f) == 0) && (found == 0)) {
			if (fgets(buf, 255, f) != NULL) {
				buf[255] = 0;
				if (libconfig_parse_args_din(buf, &argl) >= 2) {
					if ((strcmp(argl[0], "createUser") == 0) &&
						(strcmp(argl[1], user) == 0))
						found = 1;
				}

				libconfig_destroy_args_din(&argl);
			}
		}

		fclose(f);
	}

	if (found == 1) {
		if (libconfig_snmp_remove_user(user) < 0) {
			ret = -1;
			goto error;
		}
	}

	/* Gera linha com novo usuario para o arquivo SNMP_USERKEY_FILE */
	sprintf(buf, "createUser %s", user);
	if (strcmp(authpriv, "noauthnopriv") != 0) {

		if ((authproto == NULL) || (authpasswd == NULL)) {
			ret = -1;
			goto error;
		}

		if (strcasecmp(authproto, "md5") == 0) {
			strcpy(auth, "MD5");
		} else if (strcasecmp(authproto, "sha") == 0) {
			strcpy(auth, "SHA");
		} else {
			ret = -1;
			goto error;
		}

		strcat(buf, " ");
		strcat(buf, auth);
		strcat(buf, " \"");
		strcat(buf, authpasswd);
		strcat(buf, "\"");

		if (strcmp(authpriv, "authpriv") == 0) {

			if (privpasswd == NULL) {
				ret = -1;
				goto error;
			}

			if (strcasecmp(privproto, "des") == 0) {
				strcpy(auth, " DES ");
			} else if (strcasecmp(privproto, "aes") == 0) {
				strcpy(auth, " AES ");
			} else {
				ret = -1;
				goto error;
			}

			strcat(buf, auth);
			strcat(buf, privpasswd);
		}
	}

	strcat(buf, ((rw == 1) ? " #rw\n" : " #ro\n"));
	ret = _snmp_adjust_fdb(1, buf, rw);

error:
	if (is_running == 1)
		libconfig_snmp_start();
	else
		libconfig_snmp_rmon_send_signal(SIGUSR1);

	return ret;
}

unsigned int libconfig_snmp_list_users(char ***store)
{
	FILE *f;
	arg_list argl = NULL;
	char **list, buf[256];
	unsigned int i, count = 0;

	*store = NULL;

	if ((f = fopen(SNMP_USERKEY_FILE, "r")) == NULL)
		return 0;

	while (feof(f) == 0) {
		if (fgets(buf, 255, f) != NULL) {
			buf[255] = 0;
			if (libconfig_parse_args_din(buf, &argl) >= 2) {
				if (strcmp(argl[0], "createUser") == 0)
					count++;
			}
			libconfig_destroy_args_din(&argl);
		}
	}

	fclose(f);

	if (count == 0)
		return 0;

	if ((list = malloc(count * sizeof(unsigned char *))) == NULL) {
		syslog(LOG_ERR, "not possible to alloc memory");
		return 0;
	}

	memset(list, 0, count * sizeof(unsigned char *));

	if ((f = fopen(SNMP_USERKEY_FILE, "r")) == NULL) {
		free(list);
		return 0;
	}

	for (i = 0; (i < count) && (feof(f) == 0);) {
		if (fgets(buf, 255, f) != NULL) {
			buf[255] = 0;
			if (libconfig_parse_args_din(buf, &argl) >= 2) {
				if (strcmp(argl[0], "createUser") == 0) {
					if ((list[i] = malloc(strlen(argl[1]) + 1)) != NULL)
						strcpy(list[i++], argl[1]);
				}
			}
			libconfig_destroy_args_din(&argl);
		}
	}

	fclose(f);

	if (i > 0)
		*store = list;

	return i;
}

void libconfig_snmp_load_prepare_users(void)
{
	int n;
	FILE *f;
	char buf[256];
	arg_list argl = NULL;

	if (libconfig_nv_load_snmp_secret(SNMP_USERKEY_FILE) > 0) {
		if ((f = fopen(SNMP_USERKEY_FILE, "r")) != NULL) {

			while (feof(f) == 0) {
				if (fgets(buf, 255, f) != NULL) {
					buf[255] = 0;
					if ((n = libconfig_parse_args_din(buf, &argl)) >= 2) {
						if ((strcmp(argl[0], "createUser") == 0) &&
							(strchr(buf, '\n') != NULL)) {
							/* Adiciona entrada de usuario dentro do arquivo "/etc/snmpd.conf" */
							_snmp_config_add_user(argl[1],
									((strcmp(argl[n - 1],"#rw") == 0) ? 1 : 0),
							                ((n == 3) ? "noauth" : ((n == 5) ? "auth" : "priv")));
						}
					}

					libconfig_destroy_args_din(&argl);
				}
			}

			fclose(f);
		}

		/* Ajusta arquivo FILE_SNMPD_STORE_CONF */
		libconfig_exec_copy_file(SNMP_USERKEY_FILE, FILE_SNMPD_STORE_CONF);
	}
}

void libconfig_snmp_start_default(void)
{
	/* Start with default community */
	libconfig_snmp_set_community("public", 1, 0);
	return;
}

void libconfig_snmp_add_dev_trap(char *itf)
{
	FILE *f;
	int fd, found = 0;
	arg_list argl = NULL;
	char *p, line[100];

	if ((fd = open(TRAPCONF, O_RDONLY | O_CREAT, 0600)) < 0)
		return;

	close(fd);

	if ((f = fopen(TRAPCONF, "r+"))) {
		while (!found && fgets(line, 100, f)) {

			if (libconfig_parse_args_din(line, &argl) > 0) {

				if ((p = strchr(argl[0], '#')))
					*p = '\0';

				if (strlen(argl[0])) {
					if (!strcmp(itf, argl[0]))
						found++;
				}

				libconfig_destroy_args_din(&argl);
			}
		}

		if (!found) {
			fseek(f, 0, SEEK_END);
			fwrite(itf, 1, strlen(itf), f);
			fwrite("\n", 1, 1, f);
		}

		fclose(f);
	}
}

void libconfig_snmp_del_dev_trap(char *itf)
{
	int fd;
	FILE *f;
	struct stat st;
	char *p, *aux, *local, buf[100], buf_l[100];

	if ((fd = open(TRAPCONF, O_RDONLY)) < 0)
		return;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return;
	}

	if (!(local = malloc(st.st_size + 1))) {
		close(fd);
		return;
	}

	local[0] = '\0';
	close(fd);

	if ((f = fopen(TRAPCONF, "r"))) {
		while (fgets(buf, 100, f)) {
			strcpy(buf_l, buf);

			for (aux = buf_l; *aux == ' '; aux++)
				;

			if ((p = strchr(aux, ' ')))
				*p = '\0';

			if ((p = strchr(aux, '#')))
				*p = '\0';

			if ((p = strchr(aux, '\n')))
				*p = '\0';

			if (strcmp(itf, aux))
				strcat(local, buf);
		}

		fclose(f);
	}

	remove(TRAPCONF);

	if ((fd = open(TRAPCONF, O_WRONLY | O_CREAT, st.st_mode)) < 0) {
		free(local);
		return;
	}

	write(fd, local, strlen(local));
	close(fd);

	free(local);
}

