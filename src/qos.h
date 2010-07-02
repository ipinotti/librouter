/*
 * qos.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef QOS_H_
#define QOS_H_

#undef OPTION_INGRESS
#undef OPTION_HID_QOS
#undef OPTION_QOS_HTML

typedef struct {
	unsigned int cir;
	unsigned int eir;
} frts_cfg_t;

typedef enum {
	FIFO = 1, RED, SFQ, WFQ
} queue_type;

typedef enum {
	BOOL_FALSE, BOOL_TRUE
} boolean;

typedef struct {
	int mark;
	int bw;
	int bw_perc;
	int bw_remain_perc;
	int ceil;
	int ceil_perc;
	int ceil_remain_perc;
	queue_type queue;
	unsigned int fifo_limit;
	unsigned int sfq_perturb;
	unsigned int red_latency;
	unsigned int red_probability;
	unsigned char red_ecn;
	unsigned int wfq_hold_queue;
	boolean realtime;
	int rt_max_delay;
	int rt_max_unit;
} pmark_cfg_t;

typedef struct {
	char description[256];
	pmark_cfg_t *pmark;
	int n_mark; /* Number of marks configured in this policy map */
} pmap_cfg_t;

typedef struct {
	unsigned int bw;
	unsigned char max_reserved_bw; /* Percentage */
	char pname[32]; /* Policy Map name */
} intf_qos_cfg_t;

int librouter_qos_delete_policy_mark(char *name, int mark);
int librouter_qos_add_policy_mark(char *name, int mark);
int librouter_qos_create_policymap(char *policy);
int librouter_qos_get_policymap(char *policy, pmap_cfg_t **pmap);
void librouter_qos_destroy_policymap(char *policy);
int librouter_qos_get_mark_index(int mark, pmap_cfg_t *pmap);
void librouter_qos_free_policymap(pmap_cfg_t *pmap);
void librouter_qos_destroy_policymap_desc(char *name);
int librouter_qos_get_policymap_desc(char *name, pmap_cfg_t *pmap);
int librouter_qos_save_policymap_desc(char *name, pmap_cfg_t *pmap);
void librouter_qos_release_config(intf_qos_cfg_t *intf_cfg);
void librouter_qos_apply_policy(char *dev, char *policymap);
void librouter_qos_config_reserved_bw(char *dev, int reserved_bw);
void librouter_qos_config_interface_bw(char *dev, int bw);
int librouter_qos_get_interface_config(char *dev, intf_qos_cfg_t **intf_cfg);
int librouter_qos_create_interface_config(char *dev);

void librouter_qos_dump_config(FILE *out);
void librouter_qos_dump_interfaces(void);

char *librouter_qos_check_active_policy(char *policy);

void librouter_qos_clean_config(char *dev_name);
int librouter_qos_get_stats_by_devmark(char *dev_name, int mark);
int librouter_qos_tc_insert_all(char *dev_name);
int librouter_qos_tc_remove_all(char *dev_name);
unsigned int librouter_qos_class_to_dscp(const char *name);
const char *librouter_qos_dscp_to_name(unsigned int dscp);


int librouter_qos_get_frts_config(char *dev_name, frts_cfg_t **cfg);
int librouter_qos_release_frts_config(frts_cfg_t *cfg, int n);
int librouter_qos_add_frts_config(char *dev_name, frts_cfg_t *cfg);
int librouter_qos_del_frts_config(char *dev_name);

void qos_dump_all_policies_html(void);
void qos_dump_interfaces_stats_html(void);
void tc_add_remove_all(int add_nremove);
void eval_devs_qos_sanity(void);

#endif /* QOS_H_ */
