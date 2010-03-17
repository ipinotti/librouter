
#undef OPTION_INGRESS
#undef OPTION_HID_QOS
#undef OPTION_QOS_HTML

#ifndef OPTION_NEW_QOS_CONFIG
#include "../cish/options.h"
#endif

typedef struct
{
	unsigned int cir;
	unsigned int eir;
} frts_cfg_t;

#ifdef OPTION_NEW_QOS_CONFIG
typedef enum { FIFO=1, RED, SFQ, WFQ } queue_type;

typedef enum { BOOL_FALSE, BOOL_TRUE } boolean;

typedef struct {
	int mark;
	int bw;
	int bw_perc;
	int bw_remain_perc;
	int ceil;
	int ceil_perc;
	int ceil_remain_perc;
	queue_type queue; 
	unsigned int	fifo_limit;
	unsigned int	sfq_perturb;
	unsigned int	red_latency;
	unsigned int	red_probability;
	unsigned char	red_ecn;
	unsigned int 	wfq_hold_queue;
	boolean realtime;		
	int rt_max_delay;	
	int rt_max_unit;
} pmark_cfg_t;

typedef struct {
	char description[256];
	pmark_cfg_t *pmark;
	int n_mark;	/* Number of marks configured in this policy map */
} pmap_cfg_t;

typedef struct 
{
	unsigned int bw;
	unsigned char max_reserved_bw;	/* Percentage */
	char pname[32];	/* Policy Map name */
} intf_qos_cfg_t;

int delete_policy_mark(char *name, int mark);
int add_policy_mark(char *name, int mark);
int create_policymap(char *policy);
int get_policymap(char *policy, pmap_cfg_t **pmap);
void destroy_policymap(char *policy);
int get_mark_index(int mark, pmap_cfg_t *pmap);
void free_policymap(pmap_cfg_t *pmap);
void destroy_policymap_desc(char *name);
int get_policymap_desc(char *name, pmap_cfg_t *pmap);
int save_policymap_desc(char *name, pmap_cfg_t *pmap);
void release_qos_config(intf_qos_cfg_t *intf_cfg);
void apply_policy(char *dev, char *policymap);
void cfg_interface_reserved_bw(char *dev, int reserved_bw);
void cfg_interface_bw(char *dev, int bw);
int get_interface_qos_config (char *dev, intf_qos_cfg_t **intf_cfg);
int create_interface_qos_config (char *dev);
void dump_qos_config(FILE *out);
void qos_dump_interface(char *dev_name);
void qos_dump_interfaces(void);
char *check_active_qos(char *policy);

#else /* OPTION_NEW_QOS_CONFIG */

typedef enum { queue_fifo, queue_sfq, queue_red, queue_wfq } policy_queue;

typedef struct
{
	unsigned long	mark;
	unsigned int	prio;
	unsigned int 	bandwidth_bps;
	unsigned int	bandwidth_perc;
	unsigned int	bandwidth_temp;
	unsigned int	burst;
	policy_queue	queue;
	unsigned int	fifo_limit;
	unsigned int	sfq_perturb;
	unsigned int	red_latency;
	unsigned int	red_probability;
	unsigned char	red_ecn;
	unsigned int 	wfq_hold_queue;
} qos_cfg_t;

int get_qos_cfg(char *dev_name, qos_cfg_t **cfg);
int release_qos_cfg(qos_cfg_t *cfg, int n);
int add_qos_cfg(char *dev_name, qos_cfg_t *cfg);
int del_qos_cfg(char *dev_name, unsigned int mark);
int check_qos_cfg_mark(char *dev_name, unsigned int mark);
int check_qos_cfg_totals(char *dev_name, int prio, unsigned int *bandwidth_bps, unsigned int *bandwidth_perc, unsigned int *bandwidth_temp);
#ifdef OPTION_QOS_HTML
void qos_dump_interface(char *dev_name, int html);
void qos_dump_interfaces(int html);
void qos_dump_all_policies_html(void);
void qos_dump_interfaces_stats_html(void);
#else
void qos_dump_interface(char *dev_name);
void qos_dump_interfaces(void);
#endif
#endif /* OPTION_NEW_QOS_CONFIG */

void clean_qos_cfg(char *dev_name);
int get_qos_stats_by_devmark(char *dev_name, int mark);
int tc_insert_all(char *dev_name);
int tc_remove_all(char *dev_name);
unsigned int class_to_dscp(const char *name);
const char *dscp_to_name(unsigned int dscp);
void qos_dump_all_policies_html(void);
void qos_dump_interfaces_stats_html(void);
void tc_add_remove_all(int add_nremove);

int get_frts_cfg(char *dev_name, frts_cfg_t **cfg);
int release_frts_cfg(frts_cfg_t *cfg, int n);
int add_frts_cfg(char *dev_name, frts_cfg_t *cfg);
int del_frts_cfg(char *dev_name);
void eval_devs_qos_sanity(void);

