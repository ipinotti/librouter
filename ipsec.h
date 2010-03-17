int get_ipsec(void);
int set_ipsec(int opt);
int get_l2tp(void);
int set_l2tp(int opt);
int create_ipsec_conf(void);
int set_ipsec_connection(int add_del, char *key, int fd);
int create_rsakey(int keysize);
int get_ipsec_auth(char *ipsec_conn, char *buf);
int set_ipsec_rsakey(char *ipsec_conn, char *token, char *rsakey);
int create_secrets_conn_file(char *name, int type, char *shared);
int set_ipsec_auth(char *ipsec_conn, int opt);
int get_ipsec_link(char *ipsec_conn);
int set_ipsec_ike_authproto(char *ipsec_conn, int opt);
int set_ipsec_esp(char *ipsec_conn, char *cypher, char *hash);
int set_ipsec_local_id(char *ipsec_conn, char *id);
int set_ipsec_remote_id(char *ipsec_conn, char *id);
int set_ipsec_local_addr(char *ipsec_conn, char *addr);
int set_ipsec_remote_addr(char *ipsec_conn, char *addr);
int set_ipsec_subnet_inf(int position, char *ipsec_conn, char *addr, char *mask);
int set_ipsec_nexthop_inf(int position, char *ipsec_conn, char *subnet);
int set_ipsec_protoport(char *ipsec_conn, char *protoport);
int set_ipsec_pfs(char *ipsec_conn, int on);
int get_ipsec_autoreload(void);
int get_ipsec_nat_traversal(void);
int get_ipsec_overridemtu(void);
int list_all_ipsec_names(char ***rcv_p);
int get_ipsec_id(int position, char *ipsec_conn, char *buf);
int get_ipsec_subnet(int position, char *ipsec_conn, char *buf);
int get_ipsec_local_addr(char *ipsec_conn, char *buf);
int get_ipsec_nexthop(int position, char *ipsec_conn, char *buf);
int get_ipsec_ike_authproto(char *ipsec_conn);
int get_ipsec_esp(char *ipsec_conn, char *buf);
int get_ipsec_pfs(char *ipsec_conn);
int set_ipsec_link(char *ipsec_conn, int on_off);
int get_ipsec_sharedkey(char *ipsec_conn, char **buf);
int get_ipsec_rsakey(char *ipsec_conn, char *token, char **buf);
char *get_ipsec_protoport(char *ipsec_conn);
int get_ipsec_auto(char *ipsec_conn);
int get_ipsec_remote_addr(char *ipsec_conn, char *buf);
int get_mpc180(void);

enum
{
  STOP = 0,
  START,
  RESTART,
  RELOAD
};

enum
{
  ADDR_DEFAULT = 1,
  ADDR_INTERFACE,
  ADDR_ANY,
  ADDR_IP,
  ADDR_FQDN
};

enum
{
  RSA = 1,
  SECRET = 2	 
};

enum
{
  AH = 1,
  ESP = 2	 
};

