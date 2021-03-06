/*
 * lock.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef LOCK_H_
#define LOCK_H_

int librouter_lock_rmon_config_access(void);
int librouter_unlock_rmon_config_access(void);
int librouter_lock_snmp_tree_access(void);
int librouter_unlock_snmp_tree_access(void);
int librouter_lock_del_mod_access(unsigned int wait_time);
int librouter_unlock_del_mod_access(void);

#endif /* LOCK_H_ */
