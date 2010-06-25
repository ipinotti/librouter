/*
 * pim.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef PIM_H_
#define PIM_H_

#define PIM_DENSE_PID "/var/run/pimdd.pid"
#define PIM_SPARSE_PID "/var/run/pimsd.pid"

#define PIMD_CFG_FILE "/etc/pimdd.conf"
#define PIMS_CFG_FILE "/etc/pimsd.conf"

#define MAX_LINES 50

int libconfig_pim_dense_phyint(int add, char *dev);
int libconfig_pim_sparse_phyint(int add, char *dev);

void libconfig_pim_sparse_bsr_candidate(int add,
                                        char *dev,
                                        char *major,
                                        char *priority);

void libconfig_pim_sparse_rp_address(int add, char *rp);
void libconfig_pim_sparse_rp_candidate(int add,
                        char *dev,
                        char *major,
                        char *priority,
                        char *interval);

void libconfig_pim_dump(FILE *out);
void libconfig_pim_dump_interface(FILE *out, char *ifname);

#endif /* PIM_H_ */
