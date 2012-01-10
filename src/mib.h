/*
 * mib.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef MIB_H_
#define MIB_H_

#include "snmp.h"

struct obj_node {
	char *name;
	oid sub_oid;
	struct obj_node *father;
	unsigned int n_sons;
	struct obj_node **sons;
};

struct obj_node *librouter_snmp_get_tree_node(char *name,
                                              struct obj_node *P_node);
int librouter_snmp_translate_oid(char *oid_str,
                                 oid *name,
                                 size_t *namelen);
void librouter_snmp_adjust_shm_to_offset(struct obj_node *P_node,
                                         void *start_addr);
void librouter_snmp_adjust_shm_to_static(struct obj_node *P_node,
                                         void *start_addr);
int librouter_snmp_oid_to_str(char *oid_str, char *buf, int max_len);
int librouter_snmp_oid_to_obj_name(char *oid_str, char *buf, int max_len);
int librouter_snmp_dump_mibtree(void);

#endif /* MIB_H_ */
