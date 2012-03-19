/*
 * ipsec.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef IPSEC_H_
#define IPSEC_H_

#include "options.h"
#include "defines.h"

#if 1
#undef IPSEC_OPENSWAN
#define IPSEC_STRONGSWAN
#else
#define IPSEC_OPENSWAN
#undef IPSEC_STRONGSWAN
#endif

#if defined(IPSEC_STRONGSWAN)
#define PROG_IPSEC			"/sbin/ipsec"
#define PROG_SCEPCLIENT			"/sbin/ipsec scepclient"
#define FILE_PLUTO_PID			"/var/run/pluto.pid"
#undef IPSEC_SUPPORT_LOCAL_ADDRESS_INTERFACE
#undef IPSEC_SUPPORT_LOCAL_ADDRESS_FQDN
#define IPSEC_SUPPORT_SCEP
#elif defined(IPSEC_OPENSWAN)
#define PROG_IPSEC			"/bin/ipsec_run"
#define FILE_PLUTO_PID			"/var/run/pluto/pluto.pid"
#define IPSEC_SUPPORT_LOCAL_ADDRESS_INTERFACE
#undef IPSEC_SUPPORT_LOCAL_ADDRESS_FQDN
#undef IPSEC_SUPPORT_SCEP
#else
#error "No IPSec solution defined!"
#endif

/* Whether we support raw RSA key pair authentication (no X.509)
 * Openswan stil does, Strongswan has removed its support */
//#define IPSEC_SUPPORT_RSA_RAW

/* Whether we support transport mode instead of tunnel mode */
//#define IPSEC_SUPPORT_TRANSPORT_MODE

//#define IPSEC_DEBUG
#ifdef IPSEC_DEBUG
#define ipsec_dbg(x,...) \
		printf("%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define ipsec_dbg(x,...)
#endif

#define IPSEC_CONN_MAP_FILE "/var/run/ipsec.%s.bin"


enum {
	STOP = 0, START, RESTART, RELOAD
};

enum {
	ADDR_DEFAULT = 1, ADDR_INTERFACE, ADDR_ANY, ADDR_IP, ADDR_FQDN
};

enum {
	RSA = 1, SECRET = 2, X509 = 3
};

enum {
	AH = 1, ESP = 2
};

enum {
	CYPHER_ANY = 0, CYPHER_AES = 1, CYPHER_3DES = 2, CYPHER_DES = 3, CYPHER_NULL = 4
};

enum {
	 HASH_ANY = 0, HASH_MD5 = 1, HASH_SHA1 = 2
};

enum {
	AUTO_IGNORE = 0, AUTO_START = 1, AUTO_ADD = 2
};

struct pki_dn {
	char *c;
	char *state;
	char *city;
	char *org;
	char *section;
	char *name;
	char *email;
};


#define IPSEC_CONNECTION_NAME_LEN	32

struct ipsec_ep {
	char addr[32]; /* IP or interface */
	char id[MAX_ID_LEN];
	char network[16];
	char gateway[16];
	char protoport[16];
	char rsa_public_key[1024];
};

struct ipsec_connection {
	char name[IPSEC_CONNECTION_NAME_LEN];
	int status;

	int authby; /* RSA, SECRET, X509 */
	int authtype; /* ESP or AH */
	char sharedkey[512];

	int cypher; /* AES, 3DES, DES, etc. */
	int hash; /* MD5, SHA1, etc. */

	struct ipsec_ep left;
	struct ipsec_ep right;

	int pfs;
};

/* This is big stuff */
#define PKI_MAX_CA	4
struct pki_ca_data {
	char name[32];
	char cert[4096];
};

struct pki_data {
	char privkey[2048];
	char cert[2048];
	char csr[2048];
	struct pki_ca_data ca[PKI_MAX_CA];
};

#define PKI_PRIVKEY_PATH	"/etc/ipsec.d/private/my.key"
#define PKI_CSR_PATH		"/etc/ipsec.d/certs/my.csr"
#define PKI_CERT_PATH		"/etc/ipsec.d/certs/my.cert"
#define PKI_CA_PATH		"/etc/ipsec.d/cacerts/%s.cert"

#define PKI_CONTENTS_FILE	"/var/run/pki.contents"

#define OPENSSL_REQ			"/sbin/openssl req -config /etc/ipsec.d/openssl.cnf"
#define OPENSSL_GENRSA			"/sbin/openssl genrsa"
#define OPENSSL_X509			"/sbin/openssl x509"

int librouter_ipsec_is_running(void);
int librouter_ipsec_exec(int opt);

int librouter_l2tp_is_running(void);
int librouter_l2tp_exec(int opt);

int librouter_ipsec_create_conn(char *name);
int librouter_ipsec_delete_conn(char *name);

int librouter_ipsec_create_conf(void);

int librouter_ipsec_set_connection(int add_del, char *key, int fd);
#ifdef IPSEC_SUPPORT_RSA_RAW
int librouter_ipsec_create_rsakey(int keysize);
#endif

int librouter_ipsec_get_auth(char *ipsec_conn);

int librouter_ipsec_set_remote_rsakey(char *ipsec_conn, char *rsakey);
int librouter_ipsec_set_local_rsakey(char *ipsec_conn, char *rsakey);


int librouter_ipsec_create_secrets_file(char *name, int type, char *shared);

int librouter_ipsec_set_auth(char *ipsec_conn, int auth);
int librouter_ipsec_set_secret(char *ipsec_conn, char *secret);

int librouter_ipsec_get_link(char *ipsec_conn);
int librouter_ipsec_set_ike_authproto(char *ipsec_conn, int opt);
int librouter_ipsec_set_esp(char *ipsec_conn, int cypher, int hash);
int librouter_ipsec_set_local_id(char *ipsec_conn, char *id);
int librouter_ipsec_set_remote_id(char *ipsec_conn, char *id);
int librouter_ipsec_set_local_addr(char *ipsec_conn, char *addr);
int librouter_ipsec_set_remote_addr(char *ipsec_conn, char *addr);
int librouter_ipsec_set_subnet_inf(int position,
                                   char *ipsec_conn,
                                   char *addr,
                                   char *mask);
int librouter_ipsec_set_nexthop_inf(int position,
                                    char *ipsec_conn,
                                    char *subnet);
int librouter_ipsec_set_protoport(char *ipsec_conn, char *protoport);
int librouter_ipsec_set_pfs(char *ipsec_conn, int on);
int librouter_ipsec_get_autoreload(void);
int librouter_ipsec_get_nat_traversal(void);
int librouter_ipsec_get_overridemtu(void);
int librouter_ipsec_list_all_names(char ***rcv_p);
int librouter_ipsec_get_id(int position, char *ipsec_conn, char *buf);
int librouter_ipsec_get_subnet(int position, char *ipsec_conn, char *buf);
int librouter_ipsec_get_local_addr(char *ipsec_conn, char *buf);
int librouter_ipsec_get_nexthop(int position, char *ipsec_conn, char *buf);
int librouter_ipsec_get_ike_authproto(char *ipsec_conn);
int librouter_ipsec_get_esp(char *ipsec_conn, char *buf);
int librouter_ipsec_get_pfs(char *ipsec_conn);
int librouter_ipsec_set_link(char *ipsec_conn, int on_off);
int librouter_ipsec_get_sharedkey(char *ipsec_conn, char **buf);


int librouter_ipsec_get_rsakey(char *ipsec_conn, int type, char **buf);

char *librouter_ipsec_get_protoport(char *ipsec_conn);

#if 0
int librouter_ipsec_get_auto(char *ipsec_conn);
#endif

int librouter_ipsec_get_remote_addr(char *ipsec_conn, char *buf);


void librouter_ipsec_dump(FILE *out);
int librouter_ipsec_show_all(void);
int librouter_ipsec_show_conn(char *name);

#ifdef OPTION_PKI
int librouter_pki_dump_general_info(void);

int librouter_pki_get_cert_contents(char *buf, int len);
int librouter_pki_get_cert(char *buf, int len);
int librouter_pki_flush_cert(void);
int librouter_pki_set_cert(char *buf, int len);

int librouter_pki_get_csr_contents(char *buf, int len);
int librouter_pki_get_csr(char *buf, int len);
int librouter_pki_flush_csr(void);
int librouter_pki_gen_csr(void);
#ifdef IPSEC_SUPPORT_SCEP
int librouter_pki_cert_enroll(char *url, char *ca, struct pki_dn *dn);
#endif

int librouter_pki_gen_privkey(int keysize);
int librouter_pki_flush_privkey(void);
int librouter_pki_get_privkey(char *buf, int len);

int librouter_pki_get_ca_num(void);
int librouter_pki_get_ca_name_by_index(int idx, char *name);
int librouter_pki_del_cacert(char *name);
int librouter_pki_set_cacert(char *name, char *buf, int buflen);
int librouter_pki_get_cacert(char *name, char *buf, int buflen);

int librouter_pki_load(void);
int librouter_pki_save(void);
#endif /* OPTION_PKI */

#endif /* IPSEC_H_ */
