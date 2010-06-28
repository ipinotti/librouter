/*
 * ipsec.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef IPSEC_H_
#define IPSEC_H_

enum {
	STOP = 0, START, RESTART, RELOAD
};

enum {
	ADDR_DEFAULT = 1, ADDR_INTERFACE, ADDR_ANY, ADDR_IP, ADDR_FQDN
};

enum {
	RSA = 1, SECRET = 2
};

enum {
	AH = 1, ESP = 2
};

int libconfig_ipsec_is_running(void);
int libconfig_ipsec_exec(int opt);
int libconfig_l2tp_is_running(void);
int libconfig_l2tp_exec(int opt);
int libconfig_ipsec_create_conf(void);
int libconfig_ipsec_set_connection(int add_del, char *key, int fd);
int libconfig_ipsec_create_rsakey(int keysize);
int libconfig_ipsec_get_auth(char *ipsec_conn, char *buf);
int libconfig_ipsec_set_rsakey(char *ipsec_conn, char *token, char *rsakey);
int libconfig_ipsec_create_secrets_file(char *name, int type, char *shared);
int libconfig_ipsec_set_auth(char *ipsec_conn, int opt);
int libconfig_ipsec_get_link(char *ipsec_conn);
int libconfig_ipsec_set_ike_authproto(char *ipsec_conn, int opt);
int libconfig_ipsec_set_esp(char *ipsec_conn, char *cypher, char *hash);
int libconfig_ipsec_set_local_id(char *ipsec_conn, char *id);
int libconfig_ipsec_set_remote_id(char *ipsec_conn, char *id);
int libconfig_ipsec_set_local_addr(char *ipsec_conn, char *addr);
int libconfig_ipsec_set_remote_addr(char *ipsec_conn, char *addr);
int libconfig_ipsec_set_subnet_inf(int position,
                                   char *ipsec_conn,
                                   char *addr,
                                   char *mask);
int libconfig_ipsec_set_nexthop_inf(int position,
                                    char *ipsec_conn,
                                    char *subnet);
int libconfig_ipsec_set_protoport(char *ipsec_conn, char *protoport);
int libconfig_ipsec_set_pfs(char *ipsec_conn, int on);
int libconfig_ipsec_get_autoreload(void);
int libconfig_ipsec_get_nat_traversal(void);
int libconfig_ipsec_get_overridemtu(void);
int libconfig_ipsec_list_all_names(char ***rcv_p);
int libconfig_ipsec_get_id(int position, char *ipsec_conn, char *buf);
int libconfig_ipsec_get_subnet(int position, char *ipsec_conn, char *buf);
int libconfig_ipsec_get_local_addr(char *ipsec_conn, char *buf);
int libconfig_ipsec_get_nexthop(int position, char *ipsec_conn, char *buf);
int libconfig_ipsec_get_ike_authproto(char *ipsec_conn);
int libconfig_ipsec_get_esp(char *ipsec_conn, char *buf);
int libconfig_ipsec_get_pfs(char *ipsec_conn);
int libconfig_ipsec_set_link(char *ipsec_conn, int on_off);
int libconfig_ipsec_get_sharedkey(char *ipsec_conn, char **buf);
int libconfig_ipsec_get_rsakey(char *ipsec_conn, char *token, char **buf);
char *libconfig_ipsec_get_protoport(char *ipsec_conn);
int libconfig_ipsec_get_auto(char *ipsec_conn);
int libconfig_ipsec_get_remote_addr(char *ipsec_conn, char *buf);
int libconfig_ipsec_is_mpc180(void);

void libconfig_ipsec_dump(FILE *out);

#endif /* IPSEC_H_ */
