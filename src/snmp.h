/*
 * snmp.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef SNMP_H_
#define SNMP_H_

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/system.h>
#include <net-snmp/library/mib.h>
#include <net-snmp/library/default_store.h>
#include <net-snmp/session_api.h>

#define TRAPCONF	"/etc/trap.cfg"
#define NUM_ALARMS	25
#define NUM_EVENTS	25

enum {
	SAMPLE_ABSOLUTE = 1, SAMPLE_DELTA
};

struct rmon_alarm_entry {
	unsigned int index;
	unsigned int interval;
	char str_oid[256];
	oid oid[MAX_OID_LEN];
	size_t oid_len;
	unsigned int sample_type;
	int last_value;
	int last_sample;
	int last_limit;
	int rising_threshold;
	unsigned int rising_event_index;
	int falling_threshold;
	unsigned int falling_event_index;
	unsigned int status;
	char owner[256];
};

struct rmon_event_entry {
	unsigned int index;
	unsigned int do_log;
	char community[256];
	unsigned int last_time_sent;
	unsigned int status;
	char description[256];
	char owner[256];
};

#define RMON_CONFIG_STATE_LEN	64

struct rmon_config {
	struct rmon_alarm_entry alarms[NUM_ALARMS];
	struct rmon_event_entry events[NUM_EVENTS];
	int valid_state;
	char state[RMON_CONFIG_STATE_LEN + 1];
};

struct trap_data_obj {
	char *oid;
	int type;
	char *value;
};

int librouter_snmp_get_contact(char *buffer, int max_len);
int librouter_snmp_get_location(char *buffer, int max_len);
int librouter_snmp_set_contact(char *contact);
int librouter_snmp_set_location(char *location);
int librouter_snmp_reload_config(void);
int librouter_snmp_is_running(void);
int librouter_snmp_start(void);
int librouter_snmp_stop(void);
int librouter_snmp_set_community(const char *community_name,
                                 int add_del,
                                 int ro);
int librouter_snmp_dump_communities(FILE *out);
int librouter_snmp_dump_versions(FILE *out);
int librouter_snmp_add_trapsink(char *addr, char *community);
int librouter_snmp_del_trapsink(char *addr);
int librouter_snmp_get_trapsinks(char ***sinks);

int librouter_snmp_itf_should_sendtrap(char *itf);

int librouter_snmp_rmon_event_log(int index, char *description);

int librouter_snmp_rmon_add_event(int num,
                                  int log,
                                  char *community,
                                  int status,
                                  char *descr,
                                  char *owner);

int librouter_snmp_rmon_add_alarm(int num,
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
                                  char *owner);

int librouter_snmp_rmon_remove_event(char *index);
int librouter_snmp_rmon_remove_alarm(char *index);
int librouter_snmp_rmon_show_event(char *index);
int librouter_snmp_rmon_show_alarm(char *index);
int librouter_snmp_rmon_send_signal(int sig);
int librouter_snmp_rmon_clear_events(void);
int librouter_snmp_create_pdu_data(struct trap_data_obj **data_p);

int librouter_snmp_add_pdu_data_entry(struct trap_data_obj **data_p,
                                      char *oid_str,
                                      int type,
                                      char *value);

int librouter_snmp_destroy_pdu_data(struct trap_data_obj **data_p);

int librouter_snmp_sendtrap(char *snmp_trap_version,
                            char *community_rcv,
                            char *trap_obj_oid,
                            struct trap_data_obj *data);

int librouter_snmp_rmon_get_access_cfg(struct rmon_config **shm_rmon_p);

int librouter_snmp_rmon_free_access_cfg(struct rmon_config **shm_rmon_p);

int librouter_snmp_add_user(char *user,
                            int rw,
                            char *authpriv,
                            char *authproto,
                            char *privproto,
                            char *authpasswd,
                            char *privpasswd);

int librouter_snmp_remove_user(char *user);

unsigned int librouter_snmp_list_users(char ***store);
void librouter_snmp_load_prepare_users(void);
void librouter_snmp_start_default(void);
void librouter_snmp_add_dev_trap(char *itf);
void librouter_snmp_del_dev_trap(char *itf);

#endif /* SNMP_H_ */
