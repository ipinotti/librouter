/*
 * nat.h
 *
 *  Created on: Jun 2, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef NAT_H_
#define NAT_H_

#include "config_mapper.h"

//#define DEBUG
#ifdef DEBUG
#define nat_dbg(x,...) \
		syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define nat_dbg(x,...)
#endif

typedef enum {
	nat_chain_in, nat_chain_out, nat_chain_both
} nat_chain;

typedef enum {
	snat, dnat
} nat_action;

typedef enum {
	add_nat, insert_nat, remove_nat
} nat_mode;

typedef enum {
	proto_ip = 0, proto_icmp = 1, proto_tcp = 6, proto_udp = 17
} nat_proto;

struct nat_config {
	char *name;
	char src_address[32];
	char dst_address[32];
	char src_portrange[32];
	char dst_portrange[32];
	char nat_addr1[32];
	char nat_addr2[32];
	char nat_port1[32];
	char nat_port2[32];
	int src_cidr;
	int dst_cidr;
	nat_proto protocol;
	nat_mode mode;
	nat_action action;
	int masquerade;
};

int librouter_nat_rule_exists(char *nat_rule);
int librouter_nat_apply_rule(struct nat_config *n);
int librouter_nat_check_interface_rule(char *acl, char *iface_in, char *iface_out, char *chain);
int librouter_nat_bind_interface_to_rule(char *interface, char *rulename, nat_chain chain);
int librouter_nat_delete_rule(char *name);
int librouter_nat_rule_refcount(char *nat_rule);
int librouter_nat_clean_iface_rules(char *iface);

void librouter_nat_dump(char *xacl, FILE *F, int conf_format);
void librouter_nat_dump_helper(FILE *f, struct router_config *cfg);
int librouter_nat_get_iface_rules(char *iface, char *in_acl, char *out_acl);

#endif /* NAT_H_ */
