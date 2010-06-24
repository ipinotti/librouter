/*
 * smcroute.h
 *
 *  Created on: May 31, 2010
 *      Author: PD3 Tecnologia (tgrande@pd3.com.br)
 */

#ifndef CONFIG_SMCROUTE_H_
#define CONFIG_SMCROUTE_H_

#define SMC_ROUTE_MAX 20
#define SMC_ROUTE_CONF "/etc/smcroute.conf"

struct smc_route_database {
	int valid;
	char origin[16], group[16], in[16], out[16];
};

void libconfig_smc_route_hup(void);
int libconfig_smc_route(int add, char *origin, char *group, char *in, char *out);
void libconfig_smc_route_dump(FILE *out);

#endif /* CONFIG_SMCROUTE_H_ */
