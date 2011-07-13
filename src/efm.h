/*
 * efm.h
 *
 *  Created on: Nov 10, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef EFM_H_
#define EFM_H_

#include <ikanos/orionplus.h>

#define EFM_INDEX_OFFSET	1 /* efm0 -> eth1 */
#define EFM_NUM_INTERFACES	1 /* One EFM Interface on design */

int librouter_efm_set_mode(struct orionplus_conf *conf);
int librouter_efm_get_mode(struct orionplus_conf *conf);

int librouter_efm_get_force_bonding(void);
int librouter_efm_set_force_bonding(int enable);

int librouter_efm_enable(int enable);

int librouter_efm_get_status(struct orionplus_stat *st);
int librouter_efm_get_counters(struct orionplus_counters *cnt);
int librouter_efm_get_num_channels(void);

int librouter_efm_get_channel_state_string(enum channel_state st, char *buf, int len);

float librouter_efm_get_snr(int channel);
float librouter_efm_get_snr_margin(int channel);
float librouter_efm_get_data_mode_margin(int channel);
float librouter_efm_get_loop_attn(int channel);
float librouter_efm_get_xmit_power(int channel);
float librouter_efm_get_receiver_gain(int channel);

#endif /* EFM_H_ */
