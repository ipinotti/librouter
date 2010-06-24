/*
 * mangle.h
 *
 *  Created on: Jun 2, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef MANGLE_H_
#define MANGLE_H_

void libconfig_mangle_dump(char *xmangle, FILE *F, int conf_format);
int libconfig_mangle_get_iface_rules(char *iface,
                                     char *in_mangle,
                                     char *out_mangle);

#endif /* MANGLE_H_ */
