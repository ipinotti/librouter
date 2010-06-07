/* UNIX domain socket path. */
#define ZEBRA_PID "/var/quagga/zebra.pid"
#define ZEBRA_PATH "/var/quagga/zebra.vty"
#define RIP_PATH "/var/quagga/ripd.vty"
#define OSPF_PATH "/var/quagga/ospfd.vty"
#define BGP_PATH "/var/quagga/bgpd.vty"

#define ZEBRA_CONF "/etc/quagga/zebra.conf"
#define RIPD_CONF "/etc/quagga/ripd.conf"
#define OSPFD_CONF "/etc/quagga/ospfd.conf"
#define BGPD_CONF "/etc/quagga/bgpd.conf"

#define RIPD_RO_CONF "/etc.ro/quagga/ripd.conf"
#define OSPFD_RO_CONF "/etc.ro/quagga/ospfd.conf"
#define BGPD_RO_CONF "/etc.ro/quagga/bgpd.conf"

int daemon_connect (char *);
int daemon_client_execute (char *, FILE *, char *, int);
void fd_daemon_close (void);
FILE *zebra_show_cmd(const char *);
FILE *ospf_show_cmd(const char *);
FILE *rip_show_cmd(const char *);
FILE *bgp_show_cmd(const char *);
int cidr_to_classic(char *, char *);
int classic_to_cidr(char *, char *, char *);
int netmask_to_cidr(char *);
int cidr_to_netmask( unsigned int, char *);
char* u_int_to_octets( unsigned int, char*);
int octets_to_u_int( unsigned int *, char *);
unsigned int bitfill_from_left( unsigned int);
int bitcount(unsigned int);
int validatemask(unsigned int);
int validateip(char *ip);
int add_route_dev(char *target, char *netmask, char *device);
int get_ripd(void);
int set_ripd(int on_noff);
int get_ospfd(void);
int set_ospfd(int on_noff);
int get_bgpd(void);
int set_bgpd(int on_noff);
int zebra_hup(void);
int rota_flutuante(void);
int is_network_up(void);

FILE * lconfig_quagga_get_conf(char *filename, char *key);
FILE * lconfig_bgp_get_conf(int main_nip);
int lconfig_bgp_get_asn(void);
FILE *zebra_get_conf(int main_ninterf, char *intf);
void zebra_dump_static_routes_conf(FILE *out);
FILE *rip_get_conf(int main_ninterf, char *intf);
FILE *ospf_get_conf(int main_ninterf, char *intf);


