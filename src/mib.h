/*
 * mib.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef MIB_H_
#define MIB_H_

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>

#define MIBS_DIR	"/lib/mibs"
#define MAX_OID_LEN	128

struct obj_node {
	char *name;
	oid sub_oid;
	struct obj_node *father;
	unsigned int n_sons;
	struct obj_node **sons;
};

struct obj_node *libconfig_snmp_get_tree_node(char *name,
                                              struct obj_node *P_node);
int libconfig_snmp_translate_oid(char *oid_str,
                                 unsigned long *name,
                                 size_t *namelen);
void libconfig_snmp_adjust_shm_to_offset(struct obj_node *P_node,
                                         void *start_addr);
void libconfig_snmp_adjust_shm_to_static(struct obj_node *P_node,
                                         void *start_addr);
int libconfig_snmp_oid_to_str(char *oid_str, char *buf, int max_len);
int libconfig_snmp_oid_to_obj_name(char *oid_str, char *buf, int max_len);
int libconfig_snmp_dump_mibtree(void);

#endif /* MIB_H_ */
