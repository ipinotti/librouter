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
		ret = librouter_exec_prog(0, PROG_IPSEC, "start", NULL);
		break;
	case STOP:
		ret = librouter_exec_prog(0, PROG_IPSEC, "stop", NULL);
		for (i = 0; i < 5; i++) {
			if (!librouter_ipsec_is_running())
				break;
			sleep(1); /* Wait... */
		}
		break;
	case RESTART:
	case RELOAD:
		ret = librouter_exec_prog(0, PROG_IPSEC, "reload", NULL);
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


/*  Create ipsec.[connectioname].conf
 *  Type:
 *     - 0,  manual
 *     - 1,  ike
 */
static int _ipsec_write_conn_cfg(char *name)
{
#if 0
	int fd;
#else
	FILE *f;
#endif
	char buf[MAX_CMD_LINE];
	struct ipsec_connection *c = NULL;

	sprintf(buf, FILE_IKE_CONN_CONF, name);
#if 0
	if ((fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
#else
	f = fopen(buf, "w+");
	if (f == NULL) {
#endif
		printf("%% could not create connection file\n");
		return -1;
	}


	if (_ipsec_map_conn(name, &c) < 0) {
		fclose(f);
		return -1;
	}

	fprintf(f, "#active= no\nconn %s\n");

	/* AUTHBY */
	switch (c->authby) {
	case RSA:
	case X509:
		fprintf(f, "\tauthby=rsasig\n");
		break;
	default:
		fprintf(f, "\tauthby=secret\n");
		break;
	}

#ifdef OBSOLETE
	/* AUTH */
	if (c->authtype == AH)
		fprintf(f, "\tauth=ah\n");
	else
		fprintf(f, "\tauth=esp\n");
#endif



	if (c->cypher != CYPHER_ANY) {
		fprintf(f, "\tphase2alg=");
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

	if (c->left.addr[0])
		fprintf(f, "\tleft= %s\n", c->left.addr);

	if (c->left.id[0])
		fprintf(f, "\tleftid= %s\n", c->left.id);

	if (c->left.network[0])
		fprintf(f, "\tleftsubnet= %s\n", c->left.network);

	if (c->left.gateway[0])
		fprintf(f, "\tleftnexthop= %s\n", c->left.gateway);

	if (c->left.rsa_public_key[0])
		fprintf(f, "\tleftrsasigkey= %s\n", c->left.rsa_public_key);

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

	if (c->right.rsa_public_key[0])
		fprintf(f, "\trightrsasigkey= %s\n", c->right.rsa_public_key);

	if (c->right.protoport[0])
		fprintf(f, "\trightprotoport= %s\n", c->right.protoport);

	if (c->enabled)
		fprintf(f, "\tauto= start\n");
	else
		fprintf(f, "\tauto= ignore\n");

#if 0
		"\taggrmode=\n"
		"\tpfs=\n"
		"\tauto= ignore\n"
		"\tdpddelay= 30\n"
		"\tdpdtimeout= 120\n"
		"\tdpdaction= restart\n", name);
#endif


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

	ipsec_dbg("Creating connection ...\n");

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

#if 0
int librouter_ipsec_get_auth(char *ipsec_conn, char *buf)
{
	int ret;
	char *p, filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_AUTHBY, buf, MAX_CMD_LINE);

	if (ret < 0)
		return ret;

	if ((p = strstr(buf, "rsasig"))) {
		return RSA;
	} else {
		if ((p = strstr(buf, "secret")))
			return SECRET;
	}

	return 0;
}
#else
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
#endif

#if 0
char *librouter_ipsec_get_rsakeys_config(const char *file_name)
{
	FILE *file;
	int len = 0, tp;
	char *p, *buf = NULL, line[1024], last_line_flag[] = "Coefficient: ";

	line[1023] = '\0';
	file = fopen(file_name, "r");
	if (file) {
		while (!feof(file)) {
			fgets(line, 1023, file);
			if (strncmp(line, "rsa_keys", 8) == 0) {
				len = 0;
				while (!feof(file)) {
					if (strstr(line, last_line_flag)) {
						while (!feof(file)) {
							if ((tp = strlen(line)) < 1023) {
								len += tp;
								goto next_step;
							}
							len += tp;
							fgets(line, 1023, file);
						}
						goto next_step;
					} else
						len += 1023;

					fgets(line, 1023, file);
				}
			}
		}
next_step:
		if (len > 0) {
			fseek(file, 0, SEEK_SET);
			if ((buf = malloc(len + 1)) != NULL) {
				memset(buf, '\0', len + 1);
				while (!feof(file)) {
					fgets(line, 1023, file);
					if (strncmp(line, "rsa_keys", 8) == 0) {
						p = strstr(line, "rsa_keys") + 9;
						strcat(buf, p);
						while (!feof(file)) {
							fgets(line, 1023, file);
							strcat(buf, line);
							if (strchr(line, '\0'))
								goto go_out;
						}
					}
				}
			}
		}
go_out:
		fclose(file);
		if (buf && strlen(buf) > 0)
			return buf;
	} else
		printf("%% Could not find file: %s\n", file_name);

	return NULL;
}
#endif

#if 0
int librouter_ipsec_set_rsakey(char *ipsec_conn, char *token, char *rsakey)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	return librouter_str_replace_string_in_file(filename, token, rsakey);
}
#else
int librouter_ipsec_set_remote_rsakey(char *ipsec_conn, char *rsakey)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	memset(c->right.rsa_public_key, 0, sizeof(c->right.rsa_public_key));
	memcpy(c->right.rsa_public_key, rsakey, strlen(rsakey));

	return _ipsec_unmap_conn(c);
}
#endif

/*
 *  Type:
 *     - 0,  secret
 *     - 1,  rsa
 */
int librouter_ipsec_create_secrets_file(char *name, int type, char *shared)
{
	int fd;
	char *rsa, filename[60], buf[MAX_KEY_SIZE];

	sprintf(filename, FILE_CONN_SECRETS, name);

	if (!(fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600))) {
		fprintf(stderr, "Could not create secret file\n");
		return -1;
	}

	if (type) {
		char token1[] = ": RSA	{\n";
		char token2[] = "	}\n";

		rsa = malloc(8192);

		if (!librouter_nv_load_ipsec_secret(rsa)) {
			fprintf(stderr,
			                "%% ERROR: You must create RSA keys first (key generate rsa 1024).\n");
			close(fd);
			return -1;
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

		if (librouter_ipsec_set_remote_rsakey(name, buf) < 0)
			return -1;
	} else {
		char token1[] = ": PSK \"";
		char token2[] = "\"\n";

		write(fd, token1, strlen(token1));
		write(fd, shared, strlen(shared));
		write(fd, token2, strlen(token2));
		write(fd, '\0', 1);
		close(fd);
	}

	return 0;
}

#if 0
int librouter_ipsec_set_auth(char *ipsec_conn, int opt)
{
	char buf[MAX_LINE], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (opt == RSA)
		sprintf(buf, "rsasig");
	else
		sprintf(buf, "secret");

	return librouter_str_replace_string_in_file(filename,
	                STRING_IPSEC_AUTHBY, buf);
}
#else
int librouter_ipsec_set_auth(char *ipsec_conn, int auth)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	c->authby = auth;

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_get_link(char *ipsec_conn)
{
	int ret;
	struct stat st;
	char opt[5], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (stat(filename, &st) != 0) {
		librouter_pr_error(0, "Could not get ipsec link");
		return -1;
	}

	if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_LINK, opt, 5)) < 0)
		return ret;

	if (strstr(opt, "yes"))
		return 1;
	else
		return 0;
}
#else
int librouter_ipsec_get_link(char *ipsec_conn)
{
	struct ipsec_connection *c;
	int link = 0;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	link = c->enabled;

	_ipsec_unmap_conn(c);

	return link;
}
#endif

#if 0
int librouter_ipsec_set_ike_authproto(char *ipsec_conn, int opt)
{
	char buf[MAX_LINE], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (opt == AH)
		sprintf(buf, "ah");
	else
		sprintf(buf, "esp");

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_AUTHPROTO, buf);
}
#else
int librouter_ipsec_set_ike_authproto(char *ipsec_conn, int opt)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	c->authtype = opt;

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_set_esp(char *ipsec_conn, char *cypher, char *hash)
{
	char buf[MAX_LINE], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	*buf = 0;

	if (cypher) {
		if (hash)
			sprintf(buf, "%s-%s", cypher, hash);
		else
			sprintf(buf, "%s", cypher);
	}

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_ESP, buf);
}
#else
int librouter_ipsec_set_esp(char *ipsec_conn, int cypher, int hash)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	c->cypher = cypher;
	c->hash = hash;

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_set_local_id(char *ipsec_conn, char *id)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_L_ID, id);
}
#else
int librouter_ipsec_set_local_id(char *ipsec_conn, char *id)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	memset(c->left.id, 0, sizeof(c->left.id));
	memcpy(c->left.id, id, strlen(id));

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_set_remote_id(char *ipsec_conn, char *id)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_R_ID, id);
}
#else
int librouter_ipsec_set_remote_id(char *ipsec_conn, char *id)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	memset(c->right.id, 0, sizeof(c->right.id));
	memcpy(c->right.id, id, strlen(id));

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_set_local_addr(char *ipsec_conn, char *addr)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_L_ADDR, addr);
}

int librouter_ipsec_set_remote_addr(char *ipsec_conn, char *addr)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_R_ADDR, addr);
}
#else
int librouter_ipsec_set_local_addr(char *ipsec_conn, char *addr)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	memset(c->left.addr, 0, sizeof(c->left.addr));
	memcpy(c->left.addr, addr, strlen(addr));

	return _ipsec_unmap_conn(c);
}

int librouter_ipsec_set_remote_addr(char *ipsec_conn, char *addr)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	memset(c->right.addr, 0, sizeof(c->right.addr));
	memcpy(c->right.addr, addr, strlen(addr));

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_set_nexthop_inf(int position,
                                    char *ipsec_conn,
                                    char *nexthop)
{
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	switch (position) {
	case LOCAL:
		return librouter_str_replace_string_in_file(filename, STRING_IPSEC_L_NEXTHOP, nexthop);
	case REMOTE:
		return librouter_str_replace_string_in_file(filename, STRING_IPSEC_R_NEXTHOP, nexthop);
	}

	return -1;
}
#else
int librouter_ipsec_set_nexthop_inf(int position,
                                    char *ipsec_conn,
                                    char *nexthop)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	switch (position) {
	case LOCAL:
		memset(c->left.gateway, 0, sizeof(c->left.gateway));
		memcpy(c->left.gateway, nexthop, strlen(nexthop));
		break;
	case REMOTE:
		memset(c->right.gateway, 0, sizeof(c->right.gateway));
		memcpy(c->right.gateway, nexthop, strlen(nexthop));
		break;
	default:
		break;
	}

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_set_subnet_inf(int position,
                                   char *ipsec_conn,
                                   char *addr,
                                   char *mask)
{
	int ret;
	char subnet[MAX_LINE] = "", filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (strlen(addr) && strlen(mask)) {
		if ((ret = librouter_quagga_classic_to_cidr(addr, mask, subnet)) < 0)
			return ret;
	}

	switch (position) {
	case LOCAL:
		return librouter_str_replace_string_in_file(filename, STRING_IPSEC_L_SUBNET, subnet);
	case REMOTE:
		return librouter_str_replace_string_in_file(filename, STRING_IPSEC_R_SUBNET, subnet);
	}

	return -1;
}
#else
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
		memset(c->left.network, 0, sizeof(c->left.network));
		memcpy(c->left.network, subnet, strlen(subnet));
		break;
	case REMOTE:
		memset(c->left.network, 0, sizeof(c->left.network));
		memcpy(c->left.network, subnet, strlen(subnet));
		break;
	default:
		break;
	}

	return _ipsec_unmap_conn(c);
}
#endif

#if 0
int librouter_ipsec_set_protoport(char *ipsec_conn, char *protoport)
{
	char filename[60], protoport_sp1[] = "17/0", protoport_sp2[] =
	                "17/1701";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (protoport == NULL) {
		librouter_str_replace_string_in_file(filename, STRING_IPSEC_L_PROTOPORT, "");
		librouter_str_replace_string_in_file(filename, STRING_IPSEC_R_PROTOPORT, "");
	} else if (!strcmp(protoport, "SP1")) {
		librouter_str_replace_string_in_file(filename, STRING_IPSEC_L_PROTOPORT, protoport_sp1);
		librouter_str_replace_string_in_file(filename, STRING_IPSEC_R_PROTOPORT, protoport_sp2);
	} else if (!strcmp(protoport, "SP2")) {
		librouter_str_replace_string_in_file(filename, STRING_IPSEC_L_PROTOPORT, protoport_sp2);
		librouter_str_replace_string_in_file(filename, STRING_IPSEC_R_PROTOPORT, protoport_sp2);
	}

	return 0;
}
#else
int librouter_ipsec_set_protoport(char *ipsec_conn, char *protoport)
{
	char protoport_sp1[] = "17/0", protoport_sp2[] = "17/1701";
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	memset(c->left.protoport, 0, sizeof(c->left.protoport));
	memset(c->right.protoport, 0, sizeof(c->right.protoport));


	if (!strcmp(protoport, "SP1")) {
		memcpy(c->left.protoport, protoport_sp1, strlen(protoport_sp1));
		memcpy(c->right.protoport, protoport_sp1, strlen(protoport_sp1));
	} else if (!strcmp(protoport, "SP2")) {
		memcpy(c->left.protoport, protoport_sp2, strlen(protoport_sp2));
		memcpy(c->right.protoport, protoport_sp2, strlen(protoport_sp2));
	}

	return _ipsec_unmap_conn(c);;
}
#endif

#if 0
int librouter_ipsec_set_pfs(char *ipsec_conn, int on)
{
	char buf[MAX_LINE], filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);
	if (on)
		sprintf(buf, "yes");
	else
		sprintf(buf, "no");

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_PFS, buf);
}
#else
int librouter_ipsec_set_pfs(char *ipsec_conn, int on)
{
	struct ipsec_connection *c;

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	c->pfs = on;

	return _ipsec_unmap_conn(c);
}
#endif


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
	struct stat st;
	char filename[60];

	*buf = '\0';
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (stat(filename, &st) != 0)
		return -1;

	if (position == LOCAL) {
		if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_L_ID, buf, MAX_LINE)) < 0)
			return ret;
	} else {
		if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_R_ID, buf, MAX_LINE)) < 0)
			return ret;
	}

	if (strlen(buf) > 0)
		return 1;
	else
		return 0;
}

int librouter_ipsec_get_subnet(int position, char *ipsec_conn, char *buf)
{
	int ret;
	struct stat st;
	char subnet[MAX_LINE] = "", filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (stat(filename, &st) != 0)
		return -1;

	if (position == LOCAL) {
		if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_L_SUBNET, subnet, MAX_LINE)) < 0)
			return ret;
	} else {
		if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_R_SUBNET, subnet, MAX_LINE)) < 0)
			return ret;
	}

	if (strlen(subnet) > 0) {
		if ((ret = librouter_quagga_cidr_to_classic(subnet, buf)) >= 0)
			return 1;
		else
			return ret;
	}

	*buf = 0;
	return 0;
}

static int _check_ip(char *str)
{
	struct in_addr ip;

	return inet_aton(str, &(ip));
}

int librouter_ipsec_get_local_addr(char *ipsec_conn, char *buf)
{
	int ret;
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_L_ADDR, buf, MAX_LINE)) < 0) {
		*buf = 0;
		return ret;
	}

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
	int ret;
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_R_ADDR, buf, MAX_LINE)) < 0) {
		*buf = 0;
		return ret;
	}

	if (_check_ip(buf)) {
		return ADDR_IP;
	} else {
		if (strcmp(buf, STRING_ANY) == 0) {
			*buf = 0;
			return ADDR_ANY;
		} else if (strlen(buf)) {
			return ADDR_FQDN;
		}
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
	int ret;
	char *p, filename[60], buf[MAX_LINE];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_AUTHPROTO, buf, MAX_LINE);

	if (ret < 0)
		return ret;

	if ((p = strstr(buf, "ah")))
		return AH;
	else if ((p = strstr(buf, "esp")))
		return ESP;

	return 0;
}

int librouter_ipsec_get_esp(char *ipsec_conn, char *buf)
{
	int ret;
	char *p, filename[60];

	*buf = '\0';
	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_ESP, buf, MAX_LINE);

	if (ret < 0)
		return ret;

	if (strlen(buf) > 0) {
		/* remove '-' */
		if ((p = strchr(buf, '-')))
			*p = ' ';
		return 1;
	}

	return 0;
}

int librouter_ipsec_get_pfs(char *ipsec_conn)
{
	int ret;
	char *p, filename[60], buf[MAX_LINE] = "";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_PFS, buf, MAX_LINE)) < 0)
		return ret;

	if (strlen(buf) > 0) {
		if ((p = strstr(buf, "yes")))
			return 1;
		if ((p = strstr(buf, "no")))
			return 0;
	}

	return 0;
}

#if 0
int librouter_ipsec_set_link(char *ipsec_conn, int on_off)
{
	struct stat st;
	char opt[20], filename[60], buf[200];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (stat(filename, &st) != 0)
		return -1;

	if (on_off) {
		/* caso o endereco remoto seja "any", entao o paramentro "auto" dentro de ipsec.[conn name].conf deve ser "add" */
		if (librouter_ipsec_get_remote_addr(ipsec_conn, buf) == ADDR_ANY)
			sprintf(opt, "add");
		else
			sprintf(opt, "start");
	} else {
		sprintf(opt, "ignore");
	}

	librouter_str_replace_string_in_file(filename, STRING_IPSEC_AUTO, opt); /* auto= start/ignore/add */

#if 0
	/* PSK with %any uses aggresive mode */
	if (!strcmp(opt, "add") && (get_ipsec_auth(ipsec_conn, buf) == SECRET))
		sprintf(opt, "yes");
	else
#endif
	sprintf(opt, "no");

	/* aggrmode= yes/no */
	librouter_str_replace_string_in_file(filename, STRING_IPSEC_AGGRMODE, opt);

	if (on_off)
		sprintf(opt, "yes");
	else
		sprintf(opt, "no");

	return librouter_str_replace_string_in_file(filename, STRING_IPSEC_LINK, opt);
}
#else
int librouter_ipsec_set_link(char *ipsec_conn, int on_off)
{
	struct ipsec_connection *c;
	char buf[64];

	if (_ipsec_map_conn(ipsec_conn, &c) < 0)
		return -1;

	if (on_off) {
		/* For accepting road warrior connections, use add instead of start */
		if (librouter_ipsec_get_remote_addr(ipsec_conn, buf) == ADDR_ANY)
			c->enabled = AUTO_ADD;
		else
			c->enabled = AUTO_START;
	} else {
		c->enabled = AUTO_IGNORE;
	}

	_ipsec_unmap_conn(c);

	/* Update configuration file in this point */
	_ipsec_write_conn_cfg(ipsec_conn);
}
#endif

int librouter_ipsec_get_sharedkey(char *ipsec_conn, char **buf)
{
	int fd;
	int i;
	char *p, tmp[60];
	struct stat st;

	sprintf(tmp, FILE_CONN_SECRETS, ipsec_conn);

	if ((fd = open(tmp, O_RDONLY)) < 0) {
		fprintf(stderr, "could not get sharedkey");
		return -1;
	}

	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "could not read file propertys");
		return -1;
	}

	if (!(*buf = malloc(st.st_size + 1))) {
		close(fd);
		fprintf(stderr, "could not alloc memory");
		return -1;
	}

	read(fd, *buf, st.st_size);
	close(fd);
	*(*buf + st.st_size) = '\0';

	if ((p = strchr(*buf, '"'))) {
		for (p++, i = 0; *p != '"'; p++, i++)
			*(*buf + i) = *p;
		*(*buf + i) = '\0';
		return 1;
	} else {
		free(*buf);
		return -1;
	}
}

int librouter_ipsec_get_rsakey(char *ipsec_conn, char *token, char **buf)
{
	int ret;
	struct stat st;
	char filename[60];

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (stat(filename, &st) != 0) {
		fprintf(stderr, "could not get ipsec rsakey");
		return -1;
	}

	if (!(*buf = malloc(MAX_KEY_SIZE + 1))) {
		fprintf(stderr, "could not alloc memory");
		return -1;
	}

	if ((ret = librouter_str_find_string_in_file(filename, token, *buf, MAX_KEY_SIZE)) < 0) {
		free(*buf);
		*buf = NULL;

		if (ret < 0)
			return ret;
		else
			return 0;
	}

	return 1;
}

char *librouter_ipsec_get_protoport(char *ipsec_conn)
{
	struct stat st;
	char filename[60], tmp[60];
	char protoport_sp1[] = "17/0", protoport_sp2[] = "17/1701";
	static char sp1[] = "SP1", sp2[] = "SP2";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if (stat(filename, &st) != 0) {
		fprintf(stderr, "could not get ipsec protoport");
		return NULL;
	}

	if (librouter_str_find_string_in_file(filename, STRING_IPSEC_L_PROTOPORT, tmp, 60) < 0) {
		return NULL;
	}

	if (!strcmp(tmp, protoport_sp1))
		return sp1;

	if (!strcmp(tmp, protoport_sp2))
		return sp2;

	return NULL;
}

int librouter_ipsec_get_auto(char *ipsec_conn)
{
	int ret;
	char filename[60], buf[MAX_LINE] = "";

	sprintf(filename, FILE_IKE_CONN_CONF, ipsec_conn);

	if ((ret = librouter_str_find_string_in_file(filename, STRING_IPSEC_AUTO, buf, MAX_LINE)) < 0)
		return ret;

	if (strlen(buf) > 0) {
		if (strncmp(buf, "ignore", 6) == 0)
			return AUTO_IGNORE;
		if (strncmp(buf, "start", 6) == 0)
			return AUTO_START;
		if (strncmp(buf, "add", 3) == 0)
			return AUTO_ADD;
	}

	return 0;
}

void librouter_ipsec_dump(FILE *out)
{
	int idx, mtu, auto_reload;
	char **list = NULL, **list_ini = NULL;
	ppp_config cfg;
	char buf[256];

	memset(&cfg, 0, sizeof(ppp_config));

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
				if (*list) {
					char *pt;

					/* name */
					fprintf(out, " ipsec connection add %s\n", *list);
					fprintf(out, " ipsec connection %s\n", *list);

					/* authby */
					switch (librouter_ipsec_get_auth(*list)) {
					case RSA:
						fprintf(out, "  authby rsa\n");
						break;
					case SECRET: {
						pt = NULL;
						if (librouter_ipsec_get_sharedkey(*list, &pt) > 0) {
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
					switch (librouter_ipsec_get_ike_authproto(*list)) {
					case AH:
						fprintf(out, "  authproto transport\n");
						break;
					case ESP:
						fprintf(out, "  authproto tunnel\n");
						if (librouter_ipsec_get_esp(*list, buf) > 0) {
							fprintf(out, "  esp %s\n", buf);
						}
						break;
					}

					/* leftid */
					buf[0] = '\0';
					if (librouter_ipsec_get_id(LOCAL, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  local id %s\n", buf);
					}

					/* left address */
					buf[0] = '\0';
					switch (librouter_ipsec_get_local_addr(*list, buf)) {
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
					if (librouter_ipsec_get_subnet(LOCAL, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  local subnet %s\n", buf);
					}

					/* leftnexthop */
					buf[0] = '\0';
					if (librouter_ipsec_get_nexthop(LOCAL, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  local nexthop %s\n", buf);
					}

					/* rightid */
					buf[0] = '\0';
					if (librouter_ipsec_get_id(REMOTE, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  remote id %s\n", buf);
					}

					/* right address */
					buf[0] = '\0';
					switch (librouter_ipsec_get_remote_addr(*list, buf)) {
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
					if (librouter_ipsec_get_subnet(REMOTE, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  remote subnet %s\n", buf);
					}

					/* rightnexthop */
					buf[0] = '\0';
					if (librouter_ipsec_get_nexthop(REMOTE, *list, buf) > 0) {
						if (strlen(buf) > 0)
							fprintf(out, "  remote nexthop %s\n", buf);
					}

					/* rightrsasigkey */
					pt = NULL;
					if (librouter_ipsec_get_rsakey(*list, STRING_IPSEC_R_RSAKEY, &pt) > 0) {
						if (pt) {
							fprintf(out, "  remote rsakey %s\n", pt);
							free(pt);
						}
					}

					/* pfs */
					switch (librouter_ipsec_get_pfs(*list)) {
					case 0:
						fprintf(out, "  no pfs\n");
						break;
					case 1:
						fprintf(out, "  pfs\n");
						break;
					}

					/* l2tp ppp */
					{
						char netmask[16];

						librouter_ppp_l2tp_get_config(*list, &cfg);

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
					}

					/* l2tp protoport SP1|SP2 */
					if ((pt = librouter_ipsec_get_protoport(*list)) != NULL) {
						fprintf(out, "  l2tp protoport %s\n", pt);
					}

					/* auto */
					switch (librouter_ipsec_get_auto(*list)) {
					case AUTO_IGNORE:
						fprintf(out, "  shutdown\n");
						break;
					case AUTO_START:
					case AUTO_ADD:
						fprintf(out, "  no shutdown\n");
						break;
					}
					free(*list);
				}
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

#endif /* OPTION_IPSEC */

