/*
 * ps.h
 *
 *  Created on: Jun 17, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef PS_H_
#define PS_H_

struct process_t {
	int pid;
	char name[64];
	char up_time[64];
	char user[64];
	struct process_t *next;
};

struct process_t *lconfig_get_ps_info(void);
void lconfig_free_ps_info(struct process_t *ps);

#endif /* PS_H_ */
