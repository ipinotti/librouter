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

int librouter_pim_dense_phyint(int add, char *dev);
int librouter_pim_sparse_phyint(int add, char *dev, int pref, int metric);

int librouter_pim_sparse_bsr_candidate(int add,
                                        char *dev,
                                        char *major,
                                        char *priority);

int librouter_pim_sparse_rp_address(int add, char *rp);
int librouter_pim_sparse_rp_candidate(int add,
                        char *dev,
                        char *major,
                        char *priority,
                        char *interval);

void librouter_pim_dump(FILE *out);
void librouter_pim_dump_interface(FILE *out, char *ifname);
int librouter_pim_sparse_verify_intf_enable(char *dev);
int librouter_pim_sparse_enable(int opt);


#endif /* PIM_H_ */
