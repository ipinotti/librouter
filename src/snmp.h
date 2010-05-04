#include <stdio.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/session_api.h>
#include <net-snmp/library/system.h>
#include <net-snmp/library/mib.h>
#include <net-snmp/library/default_store.h>

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

int snmp_get_contact(char *buffer, int max_len);
int snmp_get_location(char *buffer, int max_len);
int snmp_set_contact(char *contact);
int snmp_set_location(char *location);
int snmp_reload_config(void);
int snmp_is_running(void);
int snmp_start(void);
int snmp_stop(void);
int snmp_set_community(const char *community_name, int add_del, int ro);
int snmp_dump_communities(FILE *out);
int snmp_dump_versions(FILE *out);
int snmp_add_trapsink(char *addr, char *community);
int snmp_del_trapsink(char *addr);
int snmp_get_trapsinks(char ***sinks);

int itf_should_sendtrap(char *itf);
int do_rmon_event_log(int index, char *description);
int do_rmon_add_event(int num,
                      int log,
                      char *community,
                      int status,
                      char *descr,
                      char *owner);
int do_rmon_add_alarm(int num,
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
int do_remove_rmon_event(char *index);
int do_remove_rmon_alarm(char *index);
int do_rmon_event_show(char *index);
int do_rmon_alarm_show(char *index);
int send_rmond_signal(int sig);
int do_rmon_events_clear(void);
int create_pdu_data(struct trap_data_obj **data_p);
int add_pdu_data_entry(struct trap_data_obj **data_p,
                       char *oid_str,
                       int type,
                       char *value);
int destroy_pdu_data(struct trap_data_obj **data_p);
int sendtrap(char *snmp_trap_version,
             char *community_rcv,
             char *trap_obj_oid,
             struct trap_data_obj *data);
int get_access_rmon_config(struct rmon_config **shm_rmon_p);
int loose_access_rmon_config(struct rmon_config **shm_rmon_p);
int add_snmp_user(char *user,
                  int rw,
                  char *authpriv,
                  char *authproto,
                  char *privproto,
                  char *authpasswd,
                  char *privpasswd);
int remove_snmp_user(char *user);
unsigned int list_snmp_users(char ***store);
void load_prepare_snmp_users(void);
void start_default_snmp(void);
void dev_add_snmptrap(char *itf);
void dev_del_snmptrap(char *itf);


