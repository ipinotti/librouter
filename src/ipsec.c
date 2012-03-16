/*
 * ipsec.c
 *
 *  Created on: Jun 24, 2010
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>

#include "options.h"
#include "defines.h"
#include "device.h"
#include "exec.h"
#include "ipsec.h"
#include "str.h"
#include "nv.h"
#include "error.h"
#include "quagga.h"
#include "process.h"
#include "ppp.h"
#include "dhcp.h"

#ifdef OPTION_IPSEC

/*****************************************************/
/*****************  Misc Functions *******************/
/*****************************************************/
static int _check_ip(char *str)
{
	struct in_addr ip;

	return inet_aton(str, &(ip));
}

/*****************************************************/
/**********  Process Management Functions ************/
/*****************************************************/

int librouter_ipsec_get_ipsec_pid(void)
{
	int i;
	FILE *f = fopen(FILE_PLUTO_PID, "r");

	if (f) {
		fscanf(f, "%u\n", &i);
		fclose(f);
		return i;
	}
	return 0;
}

int librouter_ipsec_is_running(void)
{
	struct stat st;

	if (stat(FILE_PLUTO_PID, &st) == 0)
		return 1;

	return 0;
}

int librouter_ipsec_exec(int opt)
{
	int i, ret = 0;

	switch (opt) {
	case START:
		ret = librouter_exec_prog(1, PROG_IPSEC, "start", NULL);
		break;
	case STOP:
		ret = librouter_exec_prog(1, PROG_IPSEC, "stop", NULL);
		for (i = 0; i < 5; i++) {
			if (!librouter_ipsec_is_running())
				break;
			sleep(1); /* Wait... */
		}
		break;
	case RESTART:
	case RELOAD:
		ret = librouter_exec_prog(1, PROG_IPSEC, "reload", NULL);
		break;
	default:
		break;
	}

	return ret;
}

int librouter_l2tp_get_pid(void)
{
	int i;
	FILE *f = fopen(FILE_L2TPD_PID, "r");

	if (f) {
		fscanf(f, "%u\n", &i);
		fclose(f);
		return i;
	}
	return 0;
}

int librouter_l2tp_is_running(void)
{
	struct stat st;

	if (stat(FILE_L2TPD_PID, &st) == 0)
		return 1;
	else
		return 0;
}

int librouter_l2tp_exec(int opt)
{
	int pid;
	int ret = 0;

	pid = librouter_l2tp_get_pid();
	switch (opt) {
	case START:
		if (!pid)
			ret = librouter_exec_init_program(1, PROG_L2TPD);
		break;
	case STOP:
		if (pid)
			ret = librouter_exec_init_program(0, PROG_L2TPD);
		break;
	case RESTART:
		if (pid)
			ret = kill(pid, SIGHUP);
		break;
	default:
		break;
	}
	return ret;
}

/*************************************************/
/********* File configuration functions **********/
/*************************************************/

static int _ipsec_create_conn(char *name)
{
	int fd, n, ret = 0;
	struct ipsec_connection c;
	char filename[128];

	memset(&c, 0, sizeof(c));
	strncpy(c.name, name, sizeof(c.name));

	snprintf(filename, sizeof(filename), IPSEC_CONN_MAP_FILE, name);

	if ((fd = open(filename, O_RDWR | O_CREAT, 0664)) < 0) {
		librouter_pr_error(1, "Could not open IPSec configuration");
		return -1;
	}

	n = write(fd, &c, sizeof(c));
	if (n != sizeof(c)) {
		printf("Could not create connection bin file\n");
		ret = -1;
	}

	close(fd);

	return ret;
}

static int _ipsec_map_conn(char *name, struct ipsec_connection **c)
{
	int fd;
	char filename[128];

	ipsec_dbg("Mapping connection %s => Starting\n", name);

	snprintf(filename, sizeof(filename), IPSEC_CONN_MAP_FILE, name);

	if ((fd = open(filename, O_RDWR)) < 0) {
		librouter_pr_error(1, "Could not open IPSec configuration");
		return -1;
	}

	*c = mmap(NULL, sizeof(struct ipsec_connection), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (*c == ((void *) -1)) {
		librouter_pr_error(1, "Could not open IPSec configuration");
		return -1;
	}

	close (fd);

	ipsec_dbg("Mapping connection %s => Success\n", name);

	return 0;
}

static int _ipsec_unmap_conn(struct ipsec_connection *c)
{
	return munmap(c, sizeof(struct ipsec_connection));
}

static int _ipsec_delete_conn(char *name)
{
	char filename[128];

	snprintf(filename, sizeof(filename), IPSEC_CONN_MAP_FILE, name);

	unlink(filename);

	return 0;
}


static int _ipsec_write_conn_cfg(char *name)
{
	FILE *f;
	char buf[MAX_CMD_LINE];
	struct ipsec_connection *c = NULL;

	sprintf(buf, FILE_IKE_CONN_CONF, name);

	f = fopen(buf, "w+");
	if (f == NULL) {
		printf("%% could not create connection file\n");
		return -1;
	}

	if (_ipsec_map_conn(name, &c) < 0) {
		fclose(f);
		return -1;
	}

#ifdef IPSEC_STRONGSWAN
	fprintf(f, "\n"); /* Needs empty line at the beggining */
#endif

	fprintf(f, "conn %s\n", name);

#ifdef OBSOLETE
	/* AUTH */
	if (c->authtype == AH)
		fprintf(f, "\tauth=ah\n");
	else
		fprintf(f, "\tauth=esp\n");
#endif



	if (c->cypher != CYPHER_ANY) {
#ifdef IPSEC_OPENSWAN
		fprintf(f, "\tphase2alg=");
#else
		fprintf(f, "\tesp=");
#endif
		switch (c->cypher) {
		case CYPHER_3DES:
			fprintf(f, "3des-");
			break;
		case CYPHER_AES:
			fprintf(f, "aes-");
			break;
		case CYPHER_NULL:
			fprintf(f, "null-");
			break;
		case CYPHER_DES:
		default:
			fprintf(f, "des-");
			break;
		}
		switch (c->hash) {
		case HASH_MD5:
			fprintf(f, "md5\n");
			break;
		case HASH_SHA1:
		default:
			fprintf(f, "sha1\n");
			break;
		}
	}

#ifdef IPSEC_STRONGSWAN
	if (c->left.addr[0] != '%' || (!strcmp(c->left.addr, STRING_DEFAULTROUTE)))
		fprintf(f, "\tleft= %s\n", c->left.addr);
#else
	if (c->left.addr[0])
		fprintf(f, "\tleft= %s\n", c->left.addr);
#endif

	if (c->left.id[0])
		fprintf(f, "\tleftid= %s\n", c->left.id);

	if (c->left.network[0])
		fprintf(f, "\tleftsubnet= %s\n", c->left.network);

	if (c->left.gateway[0])
		fprintf(f, "\tleftnexthop= %s\n", c->left.gateway);

	if (c->left.protoport[0])
		fprintf(f, "\tleftprotoport= %s\n", c->left.protoport);

	if (c->right.addr[0])
		fprintf(f, "\tright= %s\n", c->right.addr);

	if (c->right.id[0])
		fprintf(f, "\trightid= %s\n", c->right.id);

	if (c->right.network[0])
		fprintf(f, "\trightsubnet= %s\n", c->right.network);

	if (c->right.gateway[0])
		fprintf(f, "\trightnexthop= %s\n", c->right.gateway);

	if (c->right.protoport[0])
		fprintf(f, "\trightprotoport= %s\n", c->right.protoport);


	/* AUTHBY */
	switch (c->authby) {
	case RSA:
		fprintf(f, "\tauthby=rsasig\n");
		if (c->left.rsa_public_key[0])
			fprintf(f, "\tleftrsasigkey= %s\n", c->left.rsa_public_key);

		if (c->right.rsa_public_key[0])
			fprintf(f, "\trightrsasigkey= %s\n", c->right.rsa_public_key);
		break;
	case X509:
		fprintf(f, "\tleftrsasigkey= %%cert\n");
		fprintf(f, "\tleftcert= %s\n", PKI_CERT_PATH);
		fprintf(f, "\tleftsendcert= always\n");
		fprintf(f, "\trightrsasigkey= %%cert\n");
		break;
	default:
		fprintf(f, "\tauthby=secret\n");
		break;
	}



	if (c->pfs)
		fprintf(f, "\tpfs= yes\n");
	else
		fprintf(f, "\tpfs= no\n");

	/* Always restart if dead peer is detected */
	fprintf(f, "\tdpdaction= restart\n");
	fprintf(f, "\tdpdtimeout= 120\n");
	fprintf(f, "\tdpddelay= 30\n");

	if (c->status)
		fprintf(f, "\tauto= start\n");
	else
		fprintf(f, "\tauto= ignore\n");

	fclose(f);

	return _ipsec_unmap_conn(c);
}

/*  Updade ipsec.conf
 *  0 -> para excluir
 *  1 -> para acrescentar
 */
int _ipsec_update_conf(char *name, int action)
{
	int fd, ret;
	char filename[128], key[128];

	/* Create General configuration if necessary */
	if ((fd = open(FILE_IPSEC_CONF, O_RDWR)) < 0) {
		if ((fd = librouter_ipsec_create_conf()) < 0) {
			printf("%% could not open file %s\n", FILE_IPSEC_CONF);
			return -1;
		}
	}

	snprintf(filename, 128, FILE_IKE_CONN_CONF, name);
	snprintf(key, 128, "include %s\n", filename);

	ret = librouter_ipsec_set_connection(action, key, fd);

	close(fd);
	return ret;
}

/*  Updade ipsec.secrets
 *  0 -> para excluir
 *  1 -> para acrescentar
 */
int _ipsec_update_secrets(char *name, int action)
{
	int fd, ret;
	char filename[128], key[128];

	if ((fd = open(FILE_IPSEC_SECRETS, O_RDWR | O_CREAT, 0600)) < 0) {
		printf("%% could not open file %s\n", FILE_IPSEC_SECRETS);
		return -1;
	}
	snprintf(filename, 128, FILE_CONN_SECRETS, name);
	snprintf(key, 128, "include %s\n", filename);
	ret = librouter_ipsec_set_connection(action, key, fd);
	close(fd);
	return ret;
}

int librouter_ipsec_create_conn(char *name)
{
	struct ipsec_connection *c;

	ipsec_dbg("Creating connection %s\n", name);

	if (_ipsec_update_secrets(name, 1) < 0)
		return -1;

	if (_ipsec_update_conf(name, 1) < 0)
		return -1;

	if (_ipsec_create_conn(name) < 0)
		return -1;

	return 0;
}

int librouter_ipsec_delete_conn(char *name)
{
	char buf[128];
	struct stat st;

	ipsec_dbg("Deleting connection %s\n", name);

	snprintf(buf, 128, FILE_IKE_CONN_CONF, name);
	if (stat(buf, &st) == 0)
		remove(buf);

	snprintf(buf, 128, FILE_CONN_SECRETS, name);
	if (stat(buf, &st) == 0)
		remove(buf);

	_ipsec_update_conf(name, 0); /* Updates /etc/ipsec.conf */
	_ipsec_update_secrets(name, 0); /* Updates /etc/ipsec.secrets */
	_ipsec_delete_conn(name);

	return 0;
}

int librouter_ipsec_create_conf(void)
{
	int fd;
	char buf[] = "config setup\n"
		"\tklipsdebug= none\n"
		"\tplutodebug= none\n"
		"\tuniqueids= yes\n"
		"\tnat_traversal= no\n"
		"\toverridemtu= 0\n"
		"\tauto_reload= 180\n"
		"conn %default\n"
		"\tkeyingtries=0\n";

	if ((fd = open(FILE_IPSEC_CONF, O_RDWR | O_CREAT, 0600)) < 0)
		return -1;
	write(fd, buf, strlen(buf));
	lseek(fd, 0, SEEK_SET);
	return fd;
}

int librouter_ipsec_set_connection(int add_del, char *key, int fd)
{
	int ret = 0;
	struct stat st;
	char *p, *buf;

	if (fstat(fd, &st) < 0) {
		librouter_pr_error(1, "fstat");
		return -1;
	}

	if (!(buf = malloc(st.st_size + 1))) {
		librouter_pr_error(1, "malloc");
		return -1;
	}

	/* read all data */
	read(fd, buf, st.st_size);
	*(buf + st.st_size) = 0;

	if ((p = strstr(buf, key)) != NULL) {
		if (!add_del) {
			/* rewind! */
			lseek(fd, 0, SEEK_SET);
			write(fd, buf, p - buf);

			/* remove key */
			write(fd, p + strlen(key), st.st_size - (p + strlen(key) - buf));

			if (ftruncate(fd, lseek(fd, 0, SEEK_CUR)) < 0)
			{
				/* clean file */
				librouter_pr_error(1, "ftruncate");
				ret = -1;
			}
		}
	} else {
		/* add key */
		if (add_del)
			write(fd, key, strlen(key));
	}
	free(buf);
	return ret;
}

#ifdef IPSEC_SUPPORT_RSA_RAW
int librouter_ipsec_create_rsakey(int keysize)
{
	FILE *f;
	int ret;
	long size;
	char *buf, line[128];
	struct stat st;


	sprintf(line, "/lib/ipsec/rsasigkey --random /dev/urandom %d > %s",
	                keysize, FILE_TMP_RSAKEYS);

	system(line);

	if (stat(FILE_TMP_RSAKEYS, &st) != 0)
		return -1;

	if (!(f = fopen(FILE_TMP_RSAKEYS, "rt")))
		return -1;

	fseek(f, 0, SEEK_END);
	size = ftell(f);

	fseek(f, 0, SEEK_SET);

	if ((buf = malloc(size + 1)) == NULL) {
		librouter_pr_error(1, "unable to allocate memory");
		fclose(f);
		return -1;
	}

	fread(buf, size, 1, f);
	buf[size] = 0;

	fclose(f);
	unlink(FILE_TMP_RSAKEYS);

	if (librouter_nv_save_ipsec_secret(buf) < 0) {
		ret = -1;
		librouter_pr_error(1, "unable to save key");
	} else {
		ret = 0;
	}

	free(buf);
	return ret;
}
#endif /* IPSEC_SUPPORT_RSA_RAW */

int librouter_ipsec_get_auth(char *ipsec_conn)
{
	int auth;
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	auth = c->authby;

	_ipsec_unmap_conn(c);

	return auth;
}

int librouter_ipsec_set_remote_rsakey(char *ipsec_conn, char *rsakey)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	strncpy(c->right.rsa_public_key, rsakey, sizeof(c->right.rsa_public_key));

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_local_rsakey(char *ipsec_conn, char *rsakey)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	strncpy(c->left.rsa_public_key, rsakey, sizeof(c->left.rsa_public_key));

	return _ipsec_unmap_conn(c);
}

static int _ipsec_update_secrets_file(char *name)
{
	int fd;
	char *rsa, filename[60], buf[MAX_KEY_SIZE];
	struct ipsec_connection *c;

	if (_ipsec_map_conn(name, &c) < 0)
		return -1;

	sprintf(filename, FILE_CONN_SECRETS, name);

	if (!(fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0660))) {
		fprintf(stderr, "Could not create secret file\n");
		return -1;
	}

#ifdef IPSEC_SUPPORT_RSA_RAW
	if (c->authby == RSA) {
		char token1[] = ": RSA	{\n";
		char token2[] = "	}\n";

		rsa = malloc(8192);

		if (!librouter_nv_load_ipsec_secret(rsa)) {
			fprintf(stderr,
			                "%% ERROR: You must create RSA keys first (key generate rsa 1024).\n");
			close(fd);
			return -1;
		}

		ipsec_dbg("Loaded RSA from NV : %s\n", rsa);

		if (c->left.id[0]) {
			write(fd, c->left.id, strlen(c->left.id));
			write(fd, " ", 1);
		}

		write(fd, token1, strlen(token1));
		write(fd, rsa, strlen(rsa));
		write(fd, token2, strlen(token2));
		write(fd, '\0', 1);

		close(fd);
		free(rsa);

		/* copia a chave publica para o arquivo de configuracao da conexao */
		if (librouter_str_find_string_in_file(filename, "#pubkey", buf, MAX_KEY_SIZE) < 0)
			return -1;

		if (librouter_ipsec_set_local_rsakey(name, buf) < 0)
			return -1;
	} else
#endif
	if (c->authby == X509) {
		sprintf(buf, ": RSA /etc/ipsec.d/private/my.key\n");
		write(fd, buf, strlen(buf));
		close(fd);
	} else {
		char token1[] = ": PSK \"";
		char token2[] = "\"\n";

		if (c->left.id[0] && c->right.id) {
			sprintf(buf, "%s %s ", c->left.id, c->right.id);
			write(fd, buf, strlen(buf));
		} else if (c->left.addr[0] && _check_ip(c->left.addr) && c->right.addr[0]) {
			sprintf(buf, "%s %s ", c->left.addr, c->right.addr);
			write(fd, buf, strlen(buf));
		}

		write(fd, token1, strlen(token1));
		write(fd, c->sharedkey, strlen(c->sharedkey));
		write(fd, token2, strlen(token2));
		write(fd, '\0', 1);
		close(fd);
	}

	_ipsec_unmap_conn(c);

	return 0;
}

int librouter_ipsec_set_auth(char *ipsec_conn, int auth)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	c->authby = auth;

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_secret(char *ipsec_conn, char *secret)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	strncpy(c->sharedkey, secret, sizeof(c->sharedkey));

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_get_link(char *ipsec_conn)
{
	struct ipsec_connection *c;
	int link = 0;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	link = c->status;

	_ipsec_unmap_conn(c);

	return link;
}

int librouter_ipsec_set_ike_authproto(char *ipsec_conn, int opt)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	c->authtype = opt;

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_esp(char *ipsec_conn, int cypher, int hash)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	ipsec_dbg("cypher %d hash %d\n", cypher, hash);

	c->cypher = cypher;
	c->hash = hash;

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_local_id(char *ipsec_conn, char *id)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	ipsec_dbg("Saving local id as %s\n", id);

	strcpy(c->left.id, id);

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_remote_id(char *ipsec_conn, char *id)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	ipsec_dbg("Saving remote id as %s\n", id);

	strcpy(c->right.id, id);

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_local_addr(char *ipsec_conn, char *addr)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	strncpy(c->left.addr, addr, sizeof(c->left.addr));

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_remote_addr(char *ipsec_conn, char *addr)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	strncpy(c->right.addr, addr, sizeof(c->right.addr));

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_nexthop_inf(int position,
                                    char *ipsec_conn,
                                    char *nexthop)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	switch (position) {
	case LOCAL:
		//memset(c->left.gateway, 0, sizeof(c->left.gateway));
		//memcpy(c->left.gateway, nexthop, strlen(nexthop));
		strncpy(c->left.gateway, nexthop, sizeof(c->left.gateway));
		break;
	case REMOTE:
		//memset(c->right.gateway, 0, sizeof(c->right.gateway));
		//memcpy(c->right.gateway, nexthop, strlen(nexthop));
		strncpy(c->right.gateway, nexthop, sizeof(c->right.gateway));
		break;
	default:
		break;
	}

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_subnet_inf(int position,
                                   char *ipsec_conn,
                                   char *addr,
                                   char *mask)
{
	int ret;
	struct ipsec_connection *c;
	char subnet[MAX_LINE] = "";

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	if (strlen(addr) && strlen(mask)) {
		if ((ret = librouter_quagga_classic_to_cidr(addr, mask, subnet)) < 0)
			return ret;
	}

	switch (position) {
	case LOCAL:
		//memset(c->left.network, 0, sizeof(c->left.network));
		//memcpy(c->left.network, subnet, strlen(subnet));
		strncpy(c->left.network, subnet, sizeof(c->left.network));
		break;
	case REMOTE:
		//memset(c->right.network, 0, sizeof(c->right.network));
		//memcpy(c->right.network, subnet, strlen(subnet));
		strncpy(c->right.network, subnet, sizeof(c->right.network));
		break;
	default:
		break;
	}

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_protoport(char *ipsec_conn, char *protoport)
{
	char protoport_sp1[] = "17/0", protoport_sp2[] = "17/1701";
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	memset(c->left.protoport, 0, sizeof(c->left.protoport));
	memset(c->right.protoport, 0, sizeof(c->right.protoport));


	if (!strcmp(protoport, "SP1")) {
		memcpy(c->left.protoport, protoport_sp1, strlen(protoport_sp1+1));
		memcpy(c->right.protoport, protoport_sp1, strlen(protoport_sp1+1));
	} else if (!strcmp(protoport, "SP2")) {
		memcpy(c->left.protoport, protoport_sp2, strlen(protoport_sp2+1));
		memcpy(c->right.protoport, protoport_sp2, strlen(protoport_sp2+1));
	}

	return _ipsec_unmap_conn(c);;
}

int librouter_ipsec_set_pfs(char *ipsec_conn, int on)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	c->pfs = on;

	return _ipsec_unmap_conn(c);
}

/****************************************************/
/************ General IPSec Configuration ***********/
/****************************************************/

int librouter_ipsec_get_autoreload(void)
{
	int ret;
	char buf[MAX_LINE];

	if ((ret = librouter_str_find_string_in_file(FILE_IPSEC_CONF, STRING_IPSEC_AUTORELOAD, buf, MAX_LINE)) < 0)
		return ret;

	if (!strlen(buf))
		return -1;

	return atoi(buf);
}

int librouter_ipsec_get_nat_traversal(void)
{
	int ret;
	char buf[MAX_LINE];

	if ((ret = librouter_str_find_string_in_file(FILE_IPSEC_CONF, STRING_IPSEC_NAT, buf, MAX_LINE)) < 0)
		return ret;

	if (!strlen(buf))
		return -1;

	if (strcmp(buf, "yes") == 0)
		return 1;

	return 0;
}

int librouter_ipsec_get_overridemtu(void)
{
	int ret;
	char buf[MAX_LINE];

	if ((ret = librouter_str_find_string_in_file(FILE_IPSEC_CONF, STRING_IPSEC_OMTU, buf, MAX_LINE)) < 0)
		return ret;

	if (!strlen(buf))
		return -1;

	return atoi(buf);
}

static int _ipsec_file_filter(const struct dirent *file)
{
	char *p1, *p2;

	if ((p1 = strstr(file->d_name, "ipsec.")) == NULL)
		return 0;

	if ((p2 = strstr(file->d_name, ".conf")) == NULL)
		return 0;

	if (p1 + 6 < p2)
		return 1; /* ipsec.[conname].conf */

	return 0;
}

int librouter_ipsec_list_all_names(char ***rcv_p)
{
	int i, n, count = 0;
	struct dirent **namelist;
	char **list, **list_ini;

	n = scandir("/etc/", &namelist, _ipsec_file_filter, alphasort);

	if (n < 0) {
		librouter_pr_error(0, "scandir failed");
		return -1;
	} else {
		list_ini = list = malloc(sizeof(char *) * IPSEC_MAX_CONN);

		if (list == NULL)
			return -1;

		for (i = 0; i < IPSEC_MAX_CONN; i++, list++)
			*list = NULL;

		list = list_ini;
		for (i = 0; i < n; i++) {
			char *p1, *p2;
			p1 = strchr(namelist[i]->d_name, '.') + 1;
			p2 = strrchr(namelist[i]->d_name, '.');
			if ((count < IPSEC_MAX_CONN) && (p1 < p2)) {
				*list = malloc((p2 - p1) + 1);
				if (*list != NULL) {
					*p2 = 0;
					strncpy(*list, p1, ((p2 - p1) + 1));
					list++;
					count++;
				}
			}
			free(namelist[i]);
		}
		free(namelist);
	}
	*rcv_p = list_ini;

	return 1;
}

int librouter_ipsec_get_id(int position, char *ipsec_conn, char *buf)
{
	int ret;
	struct ipsec_connection *c;
	struct ipsec_ep *p;

	*buf = '\0';
	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	if (position == LOCAL)
		p = &c->left;
	else
		p = &c->right;

	strncpy(buf, p->id, sizeof(p->id));

	_ipsec_unmap_conn(c);

	if (strlen(buf) > 0)
		return 1;

	return 0;
}

int librouter_ipsec_get_subnet(int position, char *ipsec_conn, char *buf)
{
	int ret;
	struct ipsec_connection *c;
	struct ipsec_ep *p;
	char subnet[64];

	*buf = '\0';
	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	if (position == LOCAL)
		p = &c->left;
	else
		p = &c->right;

	strncpy(subnet, p->network, sizeof(p->network));

	_ipsec_unmap_conn(c);

	if (strlen(subnet) > 0) {
		if ((ret = librouter_quagga_cidr_to_classic(subnet, buf)) >= 0)
			return 1;
		else
			return 0;
	}

	*buf = 0;
	return 0;
}

int librouter_ipsec_get_local_addr(char *ipsec_conn, char *buf)
{
	struct ipsec_connection *c;
	int pfs;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	strncpy(buf, c->left.addr, sizeof(c->left.addr));

	_ipsec_unmap_conn(c);

	if (_check_ip(buf)) {
		return ADDR_IP;
	} else if (strcmp(buf, STRING_DEFAULTROUTE) == 0) {
		*buf = 0;
		return ADDR_DEFAULT;
	} else if (strlen(buf) && buf[0] == '%') {
		return ADDR_INTERFACE;
	}

	*buf = 0;
	return 0;

}

int librouter_ipsec_get_remote_addr(char *ipsec_conn, char *buf)
{
	struct ipsec_connection *c;
	int pfs;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	strncpy(buf, c->right.addr, sizeof(c->right.addr));

	_ipsec_unmap_conn(c);

	if (_check_ip(buf)) {
		return ADDR_IP;
	} else if (strcmp(buf, STRING_DEFAULTROUTE) == 0) {
		*buf = 0;
		return ADDR_DEFAULT;
	} else if (strlen(buf)) {
		return ADDR_FQDN;
	}

	*buf = 0;
	return 0;

}

int librouter_ipsec_get_nexthop(int position, char *ipsec_conn, char *buf)
{
	int ret;
	struct stat st;
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (stat(filename, &st) != 0)
		return -1;

	if (position == LOCAL) {
		if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_L_NEXTHOP, buf, MAX_LINE)) < 0)
			return ret;
	} else {
		if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_R_NEXTHOP, buf, MAX_LINE)) < 0)
			return ret;
	}

	if (ret < 0) {
		*buf = 0;
		return 0;
	}

	if (strlen(buf))
		return 1;
	else
		return 0;
}

int librouter_ipsec_get_ike_authproto(char *ipsec_conn)
{
	/* Dummy function, always return ESP */
	return ESP;
}

int librouter_ipsec_get_esp(char *ipsec_conn, char *buf)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	ipsec_dbg("cypher = %d hash = %d\n", c->cypher, c->hash);

	switch(c->cypher) {
	case CYPHER_AES:
		strcpy(buf, "aes ");
		break;
	case CYPHER_3DES:
		strcpy(buf, "3des ");
		break;
	case CYPHER_NULL:
		strcpy(buf, "null ");
		break;
	case CYPHER_DES:
		strcpy(buf, "des ");
		break;
	case CYPHER_ANY:
		_ipsec_unmap_conn(c);
		return 0;
	}

	switch(c->hash) {
	case HASH_MD5:
		strcat(buf, "md5");
		break;
	case HASH_SHA1:
		strcat(buf, "sha1");
		break;
	default:
		break;
	}

	_ipsec_unmap_conn(c);

	return 1;
}

int librouter_ipsec_get_pfs(char *ipsec_conn)
{
	struct ipsec_connection *c;
	int pfs;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	pfs = c->pfs;

	_ipsec_unmap_conn(c);

	return pfs;
}

int librouter_ipsec_set_link(char *ipsec_conn, int on_off)
{
	struct ipsec_connection *c;
	char buf[64];

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	if (on_off) {
		/* For accepting road warrior connections, use add instead of start */
		if (librouter_ipsec_get_remote_addr(ipsec_conn, buf) == ADDR_ANY)
			c->status = AUTO_ADD;
		else
			c->status = AUTO_START;
	} else {
		c->status = AUTO_IGNORE;
	}

	_ipsec_unmap_conn(c);

	/* Update configuration file in this point */
	_ipsec_write_conn_cfg(ipsec_conn);

	/* Update secrets file in this point */
	_ipsec_update_secrets_file(ipsec_conn);
}

int librouter_ipsec_get_sharedkey(char *ipsec_conn, char **buf)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	if (strlen(c->sharedkey) == 0)
		return -1;

	*buf = malloc(strlen(c->sharedkey)+1);
	if (*buf == NULL) {
		_ipsec_unmap_conn(c);
		return -1;
	}

	strncpy(*buf, c->sharedkey, strlen(c->sharedkey)+1);

	_ipsec_unmap_conn(c);

	return 0;
}

int librouter_ipsec_get_rsakey(char *ipsec_conn, int type, char **buf)
{
	int ret;
	struct ipsec_connection *c;
	struct ipsec_ep *p;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	if (!(*buf = malloc(MAX_KEY_SIZE + 1))) {
		fprintf(stderr, "could not alloc memory");
		_ipsec_unmap_conn(c);
		return -1;
	}

	if (type == LOCAL)
		p = &c->left;
	else
		p = &c->right;

	strncpy(*buf, p->rsa_public_key, sizeof(p->rsa_public_key));

	_ipsec_unmap_conn(c);

	if (strlen(*buf) > 0)
		return 1;

	free(*buf);
	return 0;
}

char *librouter_ipsec_get_protoport(char *ipsec_conn)
{
	char tmp[60];
	char protoport_sp1[] = "17/0", protoport_sp2[] = "17/1701";
	static char sp1[] = "SP1", sp2[] = "SP2";
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return NULL;

	strncpy(tmp, c->left.protoport, sizeof(c->left.protoport));

	_ipsec_unmap_conn(c);

	if (!strcmp(tmp, protoport_sp1))
		return sp1;

	if (!strcmp(tmp, protoport_sp2))
		return sp2;

	return NULL;
}

static void _ipsec_dump_conn(FILE *out, char *name)
{
	char *pt;
	char netmask[16];
	ppp_config cfg;
	char buf[256];

	memset(&cfg, 0, sizeof(ppp_config));

	/* name */
	fprintf(out, " ipsec connection add %s\n", name);
	fprintf(out, " ipsec connection %s\n", name);

	/* authby */
	switch (librouter_ipsec_get_auth(name)) {
	case X509:
		fprintf(out, "  authby X.509\n");
		break;
	case RSA:
		fprintf(out, "  authby rsa\n");
		break;
	case SECRET: {
		pt = NULL;
		if (!librouter_ipsec_get_sharedkey(name, &pt)) {
			if (pt) {
				fprintf(out, "  authby secret %s\n", pt);
				free(pt);
			}
		}
		break;
	}
	default:
		break;
	}

	/* authproto and crypto */
	switch (librouter_ipsec_get_ike_authproto(name)) {
	case AH:
		fprintf(out, "  authproto transport\n");
		break;
	case ESP:
		fprintf(out, "  authproto tunnel\n");
		if (librouter_ipsec_get_esp(name, buf) > 0) {
			fprintf(out, "  esp %s\n", buf);
		}
		break;
	}

	/* leftid */
	buf[0] = '\0';
	if (librouter_ipsec_get_id(LOCAL, name, buf) > 0) {
		if (strlen(buf) > 0)
			fprintf(out, "  local id %s\n", buf);
	}

	/* left address */
	buf[0] = '\0';
	switch (librouter_ipsec_get_local_addr(name, buf)) {
	case ADDR_DEFAULT:
		fprintf(out, "  local address default-route\n");
		break;
	case ADDR_INTERFACE:
		if (strlen(buf) > 0)
			fprintf(out, "  local address interface %s\n", librouter_device_linux_to_cli(buf + 1, 0));
		break;
	case ADDR_IP:
		if (strlen(buf) > 0)
			fprintf(out, "  local address ip %s\n", buf);
		break;
#if 0
	case ADDR_FQDN:
		if (strlen(buf) > 0)
			fprintf(out, "  local address fqdn %s\n", buf);
		break;
#endif
	}

	/* leftsubnet */
	buf[0] = '\0';
	if (librouter_ipsec_get_subnet(LOCAL, name, buf) > 0) {
		if (strlen(buf) > 0)
			fprintf(out, "  local subnet %s\n", buf);
	}

	/* leftnexthop */
	buf[0] = '\0';
	if (librouter_ipsec_get_nexthop(LOCAL, name, buf) > 0) {
		if (strlen(buf) > 0)
			fprintf(out, "  local nexthop %s\n", buf);
	}

	/* rightid */
	buf[0] = '\0';
	if (librouter_ipsec_get_id(REMOTE, name, buf) > 0) {
		if (strlen(buf) > 0)
			fprintf(out, "  remote id %s\n", buf);
	}

	/* right address */
	buf[0] = '\0';
	switch (librouter_ipsec_get_remote_addr(name, buf)) {
	case ADDR_ANY:
		fprintf(out, "  remote address any\n");
		break;
	case ADDR_IP:
		if (strlen(buf) > 0)
			fprintf(out, "  remote address ip %s\n", buf);
		break;
	case ADDR_FQDN:
		if (strlen(buf) > 0)
			fprintf(out, "  remote address fqdn %s\n", buf);
		break;
	}

	/* rightsubnet */
	buf[0] = '\0';
	if (librouter_ipsec_get_subnet(REMOTE, name, buf) > 0) {
		if (strlen(buf) > 0)
			fprintf(out, "  remote subnet %s\n", buf);
	}

	/* rightnexthop */
	buf[0] = '\0';
	if (librouter_ipsec_get_nexthop(REMOTE, name, buf) > 0) {
		if (strlen(buf) > 0)
			fprintf(out, "  remote nexthop %s\n", buf);
	}

	/* rightrsasigkey */
	pt = NULL;
	if (librouter_ipsec_get_rsakey(name, REMOTE, &pt) > 0) {
		if (pt) {
			fprintf(out, "  remote rsakey %s\n", pt);
			free(pt);
		}
	}

	/* pfs */
	switch (librouter_ipsec_get_pfs(name)) {
	case 0:
		fprintf(out, "  no pfs\n");
		break;
	case 1:
		fprintf(out, "  pfs\n");
		break;
	}

	librouter_ppp_l2tp_get_config(name, &cfg);

	if (cfg.peer[0]) {
		if (librouter_quagga_cidr_to_netmask(cfg.peer_mask, netmask) != -1)
			fprintf(out, "  l2tp peer %s %s\n", cfg.peer, netmask);
	}

	if (cfg.auth_user[0])
		fprintf(out, "  l2tp ppp authentication user %s\n", cfg.auth_user);

	if (cfg.auth_pass[0])
		fprintf(out, "  l2tp ppp authentication pass %s\n", cfg.auth_pass);

	/* exibir ip unnumbered no show running config */
	if (cfg.ip_unnumbered != -1) {
		fprintf(out, "  l2tp ppp ip unnumbered ethernet %d\n", cfg.ip_unnumbered);
	} else {
		if (cfg.ip_addr[0])
			fprintf(out, "  l2tp ppp ip address %s\n", cfg.ip_addr);
		else
			fprintf(out, "  no l2tp ppp ip address\n");
	}

	if (cfg.ip_peer_addr[0])
		fprintf(out, "  l2tp ppp ip peer-address %s\n", cfg.ip_peer_addr);
	else
		fprintf(out, "  l2tp ppp ip peer-address pool\n");

	if (cfg.default_route)
		fprintf(out, "  l2tp ppp ip default-route\n");

	if (cfg.novj)
		fprintf(out, "  no l2tp ppp ip vj\n");
	else
		fprintf(out, "  l2tp ppp ip vj\n");

	if (cfg.echo_interval)
		fprintf(out, "  l2tp ppp keepalive interval %d\n", cfg.echo_interval);

	if (cfg.echo_failure)
		fprintf(out, "  l2tp ppp keepalive timeout %d\n", cfg.echo_failure);

	if (cfg.mtu)
		fprintf(out, "  l2tp ppp mtu %d\n", cfg.mtu);


	/* l2tp protoport SP1|SP2 */
	if ((pt = librouter_ipsec_get_protoport(name)) != NULL) {
		fprintf(out, "  l2tp protoport %s\n", pt);
	}

	/* auto */
	switch (librouter_ipsec_get_link(name)) {
	case AUTO_IGNORE:
		fprintf(out, "  shutdown\n");
		break;
	case AUTO_START:
	case AUTO_ADD:
		fprintf(out, "  no shutdown\n");
		break;
	}

	free(name);
}


void librouter_ipsec_dump(FILE *out)
{
	int idx, mtu, auto_reload;
	char **list = NULL, **list_ini = NULL;
	char buf[256];

	fprintf(out, "crypto\n");

	if ((auto_reload = librouter_ipsec_get_autoreload()) > 0) {
		fprintf(out, " auto-reload %d\n", auto_reload);
	}

	if (librouter_ipsec_get_nat_traversal() > 0) {
		fprintf(out, " nat-traversal\n");
	}

	if ((mtu = librouter_ipsec_get_overridemtu()) > 0) {
		fprintf(out, " overridemtu %d\n", mtu);
	}

	if (librouter_ipsec_list_all_names(&list) > 0) {
		if (*list != NULL) {
			list_ini = list;
			for (idx = 0; idx < IPSEC_MAX_CONN; idx++, list++) {
				/* process connection name */
				if (*list)
					_ipsec_dump_conn(out, *list);

			}
			free(list_ini);
		}
	}

	/* l2tp settings! */
	if (librouter_dhcp_get_local() == DHCP_SERVER) {
		if (librouter_dhcp_get_server_local(buf) == 0) {
			fprintf(out, " %s\n", buf);
		}
	} else {
		fprintf(out, " l2tp pool ethernet 0\n");
	}

	if (librouter_l2tp_is_running() > 0) {
		fprintf(out, " l2tp server\n");
	}

	fprintf(out, "!\n");
}

int librouter_ipsec_show_conn(char *name)
{
	char line[128];

#ifdef IPSEC_STRONGSWAN
	sprintf(line, "%s status %s", PROG_IPSEC, name);
#else
	sprintf(line, "/lib/ipsec/whack --status");
#endif
	system(line);

	return 0;
}

int librouter_ipsec_show_all(void)
{
	char line[128];

#ifdef IPSEC_STRONGSWAN
	sprintf(line, "%s status", PROG_IPSEC);
#else
	sprintf(line, "/lib/ipsec/whack --status");
#endif
	system(line);

	return 0;
}


#ifdef OPTION_PKI
/************************************************/
/****************** X.509 ***********************/
/************************************************/
int _read_file(char *path, char *buf, int buflen)
{
	int fd, n;
	struct stat st;

	ipsec_dbg("Reading file %s\n", path);

	if ((fd = open(path, O_RDONLY)) < 0)
		return -1;

	fstat(fd, &st);

	if (buflen < st.st_size) {
		printf("ERROR: buffer smaller than filesize of %s\n", path);
		close(fd);
		return -1;
	}

	n = read(fd, buf, st.st_size);
	if (n != st.st_size)
		printf("Wrong number of bytes read\n");

	close(fd);

	return 0;
}

int _write_file(char *path, char *buf, int buflen)
{
	int fd, n;
	struct stat st;

	ipsec_dbg("Writing file %s\n", path);

	if ((fd = open(path, O_WRONLY | O_CREAT, 0660)) < 0)
		return -1;

	n = write(fd, buf, buflen);
	if (n != buflen)
		printf("Wrong number of bytes written\n");

	close(fd);

	return 0;
}

int librouter_pki_dump_general_info(void)
{
#ifdef IPSEC_OPENSWAN
	system("/lib/ipsec/ipsec auto --listall");
#else /* STRONGSWAN */
	system("/sbin/ipsec listcerts");
#endif
}

/**********************************/
/******* Private RSA Key **********/
/**********************************/
int librouter_pki_get_privkey(char *buf, int len)
{
	return _read_file(PKI_PRIVKEY_PATH, buf, len);
}

static int _pki_set_privkey(char *buf, int len)
{
	return _write_file(PKI_PRIVKEY_PATH, buf, len);
}

int librouter_pki_flush_privkey(void)
{
	return unlink(PKI_PRIVKEY_PATH);
}

int librouter_pki_gen_privkey(int keysize)
{
	char line[256];

	sprintf(line, "%s -out %s %d >/dev/null 2>/dev/null",
	        OPENSSL_GENRSA, PKI_PRIVKEY_PATH, keysize);

	return system(line);
}

/*********************************/
/***** Certificate Authority *****/
/*********************************/
static int _filter_ca(const struct dirent *d)
{
	if (strstr(d->d_name, ".cert"))
		return 1;

	return 0;
}
static int _list_cas(struct dirent ***list)
{
	int i, n, count = 0;
	struct dirent **namelist;

	n = scandir("/etc/ipsec.d/cacerts", &namelist, _filter_ca, alphasort);

	*list = namelist;

	return n;
}

int librouter_pki_get_ca_num(void)
{
	struct dirent **l;
	int n, i;

	n = _list_cas(&l);
	if (n < 0) {
		librouter_logerr("Could not list CA Certs directory\n");
		return -1;
	}

	i = n;
	while (i--) {
		ipsec_dbg("Getting CA info: %s\n", l[i]->d_name);
		free(l[i]);
	}
	free(l);

	return n;
}

int librouter_pki_get_ca_name_by_index(int idx, char *name)
{
	struct dirent **l;
	int n, i;
	char *p;

	n = _list_cas(&l);
	if (n < 0) {
		librouter_logerr("Could not list CA Certs directory\n");
		return -1;
	}

	strcpy(name, l[idx]->d_name);
	p = strstr(name, ".cert");
	if (p) {
		p[0] = '\0';
	} else {
		librouter_logerr("Wrong file name inside cacerts directory : %s\n", name);
	}

	i = n;
	while (i--) {
		ipsec_dbg("Getting CA info: %s\n", l[i]->d_name);
		free(l[i]);
	}
	free(l);

	return 0;
}

int librouter_pki_get_cacert(char *name, char *buf, int buflen)
{
	char filename[64];

	sprintf(filename, PKI_CA_PATH, name);

	return _read_file(filename, buf, buflen);
}

int librouter_pki_set_cacert(char *name, char *buf, int buflen)
{
	char filename[64];

	sprintf(filename, PKI_CA_PATH, name);

	return _write_file(filename, buf, buflen);
}

int librouter_pki_del_cacert(char *name)
{
	char filename[64];

	sprintf(filename, PKI_CA_PATH, name);

	return unlink(filename);
}

/*********************************/
/** Certificate Signing Request **/
/*********************************/

int librouter_pki_flush_csr(void)
{
	return unlink(PKI_CSR_PATH);
}

int librouter_pki_get_csr(char *buf, int len)
{
	return _read_file(PKI_CSR_PATH, buf, len);
}

int librouter_pki_get_csr_contents(char *buf, int len)
{
	char line[256];

	if (librouter_pki_get_csr(buf, len) < 0)
		return -1;

	unlink(PKI_CONTENTS_FILE);
	sprintf(line, "%s -in %s -noout -text -out %s",
	        OPENSSL_REQ, PKI_CSR_PATH, PKI_CONTENTS_FILE);
	system(line);

	return _read_file(PKI_CONTENTS_FILE, buf, len);
}

int _pki_set_csr(char *buf, int len)
{
	return _write_file(PKI_CSR_PATH, buf, len);
}

int librouter_pki_gen_csr(void)
{
	struct pki_data pki;
	char buf[2048];

	if (librouter_pki_get_privkey(buf, sizeof(buf)) < 0) {
		printf("%% Please generate pki private key first\n");
		return -1;
	}

	sprintf(buf, "%s -new -key %s -out %s", OPENSSL_REQ, PKI_PRIVKEY_PATH, PKI_CSR_PATH);


	return system(buf);
}

#ifdef IPSEC_SUPPORT_SCEP
int librouter_pki_cert_enroll(char *url, char *ca)
{
	char line[256];
	char cacert[128];

	sprintf(cacert, PKI_CA_PATH, ca);

	sprintf(line, "%s -u %s -i pkcs1=%s -i cacert-enc=%s -i cacert-sig=%s -o cert=%s",
	        PROG_SCEPCLIENT, url, PKI_PRIVKEY_PATH, cacert, cacert, PKI_CERT_PATH);
	system(line);

	return 0;
}
#endif /* IPSEC_SUPPORT_SCEP */
/**********************************/
/** Host (Own) X.509 Certificate **/
/**********************************/
int librouter_pki_get_cert(char *buf, int len)
{
	return _read_file(PKI_CERT_PATH, buf, len);
}

int librouter_pki_get_cert_contents(char *buf, int len)
{
	char line[256];

	if (librouter_pki_get_cert(buf, len) < 0)
		return -1;

	unlink(PKI_CONTENTS_FILE);
	sprintf(line, "%s -in %s -noout -text -out %s",
	        OPENSSL_X509, PKI_CERT_PATH, PKI_CONTENTS_FILE);
	system(line);

	return _read_file(PKI_CONTENTS_FILE, buf, len);
}

int librouter_pki_flush_cert(void)
{
	return unlink(PKI_CERT_PATH);
}


int librouter_pki_set_cert(char *buf, int len)
{
	return _write_file(PKI_CERT_PATH, buf, len);
}


/**********************************/
/** PKI Load and Save functions ***/
/**********************************/
static struct pki_data *_alloc_pki_data(void)
{
	struct pki_data *pki;

	pki = malloc(sizeof(struct pki_data));
	if (pki == NULL) {
		librouter_logerr("Could not allocate pki structure\n");
		return NULL;
	}

	memset(pki, 0, sizeof(struct pki_data));

	return pki;
}

/**
 * librouter_pki_load
 *
 * Load PKI information from non-volatile memory to filesystem
 *
 */
int librouter_pki_load(void)
{
	int i, ret = 0;
	struct pki_data *pki;

	pki = _alloc_pki_data();
	if (pki == NULL)
		return -1;

	if (librouter_nv_load_pki(pki) < 0) {
		librouter_logerr("Could not load PKI information\n");
		goto pki_load_end;
	}

	if (!pki->privkey[0]) {
		ipsec_dbg("Unable to load private key, "
				"refusing to load certificates owned by us\n");
		goto pki_load_ca;
	}

	ret = _pki_set_privkey(pki->privkey, strlen(pki->privkey));
	if (ret < 0)
		goto pki_load_end;

	if (pki->cert[0]) {
		ret = librouter_pki_set_cert(pki->cert, strlen(pki->cert));
		if (ret < 0)
			goto pki_load_end;
	}

	if (pki->csr[0]) {
		ret = _pki_set_csr(pki->csr, strlen(pki->csr));
		if (ret < 0)
			goto pki_load_end;
	}

pki_load_ca:
	for (i = 0; (i < PKI_MAX_CA) && pki->ca[i].name[0]; i++) {
		ret = librouter_pki_set_cacert(pki->ca[i].name, pki->ca[i].cert, strlen(pki->ca[i].cert));
		if (ret < 0)
			goto pki_load_end;
	}

pki_load_end:
	free(pki);

	return ret;
}

/**
 * librouter_pki_save
 *
 * Save PKI information from filesystem to non-volatile memory
 */
int librouter_pki_save(void)
{
	int ret = 0, i, n;
	struct pki_data *pki;

	pki = _alloc_pki_data();
	if (pki == NULL)
		return -1;

	librouter_pki_get_privkey(pki->privkey, sizeof(pki->privkey));
	librouter_pki_get_csr(pki->csr, sizeof(pki->csr));
	librouter_pki_get_cert(pki->cert, sizeof(pki->cert));


	n = librouter_pki_get_ca_num();
	for (i = 0; i < n; i++) {
		librouter_pki_get_ca_name_by_index(i, pki->ca[i].name);
		librouter_pki_get_cacert(pki->ca[i].name,
		                         pki->ca[i].cert, sizeof(pki->ca[i].cert));
	}

	ret = librouter_nv_save_pki(pki);

pki_save_end:
	free(pki);

	return ret;
}
#endif /* OPTION_PKI */
#endif /* OPTION_IPSEC */
