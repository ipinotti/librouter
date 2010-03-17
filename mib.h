#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>

#define MIBS_DIR	"/lib/mibs"
#define MAX_OID_LEN	128

struct obj_node
{
	char *name;
	oid sub_oid;
	struct obj_node *father;
	unsigned int n_sons;
	struct obj_node **sons;
};

struct obj_node *get_snmp_tree_node(char *name, struct obj_node *P_node);
int snmp_translate_oid(char *oid_str, unsigned long *name, size_t *namelen);
void adjust_shm_to_offset(struct obj_node *P_node, void *start_addr);
void adjust_shm_to_static(struct obj_node *P_node, void *start_addr);
int convert_oid_to_string_oid(char *oid_str, char *buf, int max_len);
int convert_oid_to_object_name(char *oid_str, char *buf, int max_len);
int dump_snmp_mibtree(void);

