/*
 * efm.h
 *
 *  Created on: Nov 10, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef EFM_H_
#define EFM_H_

#define EFM_INDEX_OFFSET	1 /* efm0 -> eth1 */
#define EFM_NUM_INTERFACES	1 /* One EFM Interface on design */

int librouter_efm_init_chip(int num_of_channels);
int librouter_efm_get_num_channels(void);
int librouter_efm_set_mode(int mode);
int librouter_efm_get_mode(void);


#endif /* EFM_H_ */
