/*
 * nat.h
 *
 *  Created on: Jun 2, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef NAT_H_
#define NAT_H_

void libconfig_nat_dump(char *xacl, FILE *F, int conf_format);
void libconfig_nat_dump_helper(FILE *f, cish_config *cish_cfg);
int libconfig_nat_get_iface_rules(char *iface, char *in_acl, char *out_acl);

#endif /* NAT_H_ */
