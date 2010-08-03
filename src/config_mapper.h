/*
 * config_mapper.h
 *
 *  Created on: Aug 3, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef CONFIG_MAPPER_H_
#define CONFIG_MAPPER_H_

struct router_config {
	char login_secret[15];
	char enable_secret[15];
	int terminal_lines;
	int terminal_timeout;
	int debug[20]; /* debug persistent !!! Check debug.c */

	/* 11111,22222,33333,44444,55555,66666,77777,88888 */
	char nat_helper_ftp_ports[48]; /* !!! find new location! */
	char nat_helper_irc_ports[48]; /* !!! find new location! */
	char nat_helper_tftp_ports[48]; /* !!! find new location! */
};

struct router_config *librouter_config_mmap_cfg(void);
int librouter_config_munmap_cfg(struct router_config  *cfg);

#endif /* CONFIG_MAPPER_H_ */
