#ifndef _LIB_ACL_H
#define _LIB_ACL_H

#define DEBUG
#ifdef DEBUG
#define acl_dbg(x,...) \
		syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define acl_dbg(x,...)
#endif

typedef enum {
	chain_in, chain_out, chain_both
} acl_chain;

typedef enum {
	acl_accept, acl_drop, acl_reject, acl_log, acl_tcpmss
} acl_action;

typedef enum {
	ip = 0, icmp = 1, tcp = 6, udp = 17
} acl_proto;

typedef enum {
	add_acl, insert_acl, remove_acl
} acl_mode;

typedef enum {
	st_established = 0x1, st_new = 0x2, st_related = 0x4
} acl_state;

struct acl_config {
	char *name;
	char src_address[32];
	char dst_address[32];
	char src_portrange[32];
	char dst_portrange[32];
	int src_cidr;
	int dst_cidr;

	char *tos;
	char *icmp_type;
	char *icmp_type_code;
	char *flags;
	char *tcpmss;

	acl_proto protocol;
	acl_action action;
	acl_mode mode;
	acl_state state;
};

struct acl_dump {
	char *acl;
	char *action;
	char *type;
	char *proto;
	char *input;
	char *output;
	char *src;
	char *dst;
	char *sports;
	char *dports;
	char *flags;
	char *tos;
	char *state;
	char *icmp_type;
	char *icmp_type_code;
	char *tcpmss;
	char *mac;
	char *mcount;
};

#define trimcolumn(x) tmp=strchr(x, ' '); if (tmp != NULL) *tmp=0;

int librouter_acl_apply_access_policy(char *policy_target);
int librouter_acl_apply_exist_chain_in_intf(char *dev, char *chain, int direction);
void librouter_acl_create_new(char *);
void librouter_acl_apply(struct acl_config *);
int librouter_acl_exists(char *);
int librouter_acl_delete_rule(char * name);
int librouter_acl_matched_exists(char *acl,
                                 char *iface_in,
                                 char *iface_out,
                                 char *chain);
int librouter_acl_get_iface_rules(char *iface, char *in_acl, char *out_acl);
int librouter_acl_get_refcount(char *acl);
int librouter_acl_clean_iface_acls(char *iface);
int librouter_acl_copy_iface_acls(char *src, char *trg);
int librouter_acl_interface_ipsec(int add_del,
                                  int chain,
                                  char *dev,
                                  char *listno);
void librouter_acl_cleanup_modules(void);
void librouter_acl_set_ports(const char *ports, char *str);
void librouter_acl_print_flags(FILE *out, char *flags);
void librouter_acl_dump(char *xacl, FILE *F, int conf_format);
void librouter_acl_dump_policy(FILE *F);

#endif

