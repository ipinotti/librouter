/*
 * efm.h
 *
 *  Created on: Nov 10, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef EFM_H_
#define EFM_H_

int librouter_efm_init_chip(int num_of_channels);
int librouter_efm_get_num_channels(void);
int librouter_efm_set_mode(int mode);
int librouter_efm_get_mode(void);


#endif /* EFM_H_ */
