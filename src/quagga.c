/*
 * quagga.c
 *
 *  Created on: Jun 24, 2010
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/route.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <linux/autoconf.h>

#include "options.h"
#include "device.h"
#include "error.h"
#include "exec.h"
#include "process.h"
#include "quagga.h"
#include "str.h"
#include "args.h"
#include "sha1.h"

static int fd_daemon;

/*-------------------------------------------------------------------------------
 * functions created to handle the daemons of zebra package
 *-------------------------------------------------------------------------------
 */

/* Making connection to protocol daemon */
int librouter_quagga_connect_daemon(char *path)
{
	int i, ret = 0, sock, len;
	struct sockaddr_un addr;
	struct stat s_stat;

	fd_daemon = -1;

	/* Stat socket to see if we have permission to access it. */
	/* Wait for daemon with 3s timeout! */
	for (i = 0; i < 3; i++) {
		if (stat(path, &s_stat) < 0) {
			if (errno != ENOENT) {
				fprintf(stderr, "daemon_connect(%s): stat = %s\n", path, strerror(errno));
				return (-1);
			}
		} else
			break;

		sleep(1);
	}

	if (!S_ISSOCK(s_stat.st_mode)) {
		fprintf(stderr, "daemon_connect(%s): Not a socket\n", path);
		return (-1);
	}

	if (!(s_stat.st_mode & S_IWUSR) || !(s_stat.st_mode & S_IRUSR)) {
		fprintf(stderr, "daemon_connect(%s): No permission to access socket\n", path);
		return (-1);
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "daemon_connect(): socket = %s\n", strerror(errno));
		return (-1);
	}

	memset(&addr, 0, sizeof(struct sockaddr_un));

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, strlen(path));
	len = sizeof(addr.sun_family) + strlen(addr.sun_path);

	for (i = 0; i < 3; i++) {
		if ((ret = connect(sock, (struct sockaddr *) &addr, len)) == 0)
			break;
		sleep(1);
	}

	if (ret == -1) {
		fprintf(stderr, "daemon_connect(): connect = %s\n", strerror(errno));
		close(sock);
		return (-1);
	}

	fd_daemon = sock;
	return 0;
}

int librouter_quagga_execute_client(char *line, FILE *fp, char *buf_daemon, int show)
{
	int i, ret, nbytes;
	char buf[1024];

	if (fd_daemon < 0)
		return 0;

	ret = write(fd_daemon, line, strlen(line) + 1);
	if (ret <= 0) {
		fprintf(stderr, "daemon_client_execute(%s): write error!\n", line);
		librouter_quagga_close_daemon();
		return 0;
	}

	/* clean buf_daemon before writing to it */
	if (buf_daemon)
		strcpy(buf_daemon, buf);

	while (1) {
		nbytes = read(fd_daemon, buf, sizeof(buf) - 1);

		if (nbytes <= 0 && errno != EINTR) {
			fprintf(stderr, "daemon_client_execute(%s): read error!\n", line);
			librouter_quagga_close_daemon();
			return 0;
		}

		if (nbytes > 0) {
			buf[nbytes] = '\0';

			if (show) {
				fprintf(fp, "%s", buf);
				fflush(fp);
			}

			/* Uses strcat to not overwrite*/
			if (buf_daemon)
				strcat(buf_daemon, buf);

			if (nbytes >= 4) {
				i = nbytes - 4;
				if (buf[i] == '\0' &&
					buf[i + 1] == '\0' &&
					buf[i + 2] == '\0') {
					ret = buf[i + 3];
					break;
				}
			}
		}
	}

	return ret;
}

void librouter_quagga_close_daemon(void)
{
	if (fd_daemon > 0)
		close(fd_daemon);
	fd_daemon = -1;
}

#define ZEBRA_OUTPUT_FILE "/var/run/.zebra_out"
FILE *librouter_quagga_zebra_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (librouter_quagga_connect_daemon(ZEBRA_PATH) < 0)
		return NULL;

	f = fopen(ZEBRA_OUTPUT_FILE, "wt");
	if (!f) {
		librouter_quagga_close_daemon();
		return NULL;
	}

	new_cmdline = librouter_device_to_linux_cmdline((char*) cmdline);
	librouter_quagga_execute_client("enable", stdout, NULL, 0);
	librouter_quagga_execute_client(new_cmdline, f, NULL, 1);

	fclose(f);
	librouter_quagga_close_daemon();

	return fopen(ZEBRA_OUTPUT_FILE, "rt");
}

#define OSPF_OUTPUT_FILE "/var/run/.ospf_out"
FILE *librouter_quagga_ospf_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (librouter_quagga_connect_daemon(OSPF_PATH) < 0)
		return NULL;

	f = fopen(OSPF_OUTPUT_FILE, "wt");
	if (!f) {
		librouter_quagga_close_daemon();
		return NULL;
	}

	new_cmdline = librouter_device_to_linux_cmdline((char*) cmdline);
	librouter_quagga_execute_client("enable", stdout, NULL, 0);
	librouter_quagga_execute_client(new_cmdline, f, NULL, 1);

	fclose(f);
	librouter_quagga_close_daemon();

	return fopen(OSPF_OUTPUT_FILE, "rt");
}

#define RIP_OUTPUT_FILE "/var/run/.rip_out"
FILE *librouter_quagga_rip_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (librouter_quagga_connect_daemon(RIP_PATH) < 0)
		return NULL;

	f = fopen(RIP_OUTPUT_FILE, "wt");
	if (!f) {
		librouter_quagga_close_daemon();
		return NULL;
	}

	new_cmdline = librouter_device_to_linux_cmdline((char*) cmdline);
	librouter_quagga_execute_client("enable", stdout, NULL, 0);
	librouter_quagga_execute_client(new_cmdline, f, NULL, 1);

	fclose(f);
	librouter_quagga_close_daemon();

	return fopen(RIP_OUTPUT_FILE, "rt");
}

#define BGP_OUTPUT_FILE "/var/run/.bgp_out"
FILE *librouter_quagga_bgp_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (librouter_quagga_connect_daemon(BGP_PATH) < 0)
		return NULL;

	f = fopen(BGP_OUTPUT_FILE, "wt");
	if (!f) {
		librouter_quagga_close_daemon();
		return NULL;
	}

	new_cmdline = librouter_device_to_linux_cmdline((char*) cmdline);
	librouter_quagga_execute_client("enable", stdout, NULL, 0);
	librouter_quagga_execute_client(new_cmdline, f, NULL, 1);

	fclose(f);
	librouter_quagga_close_daemon();

	return fopen(BGP_OUTPUT_FILE, "rt");
}

/*-------------------------------------------------------------------------------
 * Functions to handle different netmask notations (classic and CIDR)
 *-------------------------------------------------------------------------------
 */

/* Takes an cidr address (10.5.0.1/8) and creates in  */
/* buf a classic address(10.5.0.1 255.0.0.0)          */
int librouter_quagga_cidr_to_classic(char *cidr_addr, char *buf)
{
	int i;
	char ip[20], mask[20];
	char cidr[3];
	char *p, *p2;
	struct in_addr tmp;

	p2 = cidr_addr;
	p = strstr(cidr_addr, "/");
	if (p == NULL)
		return (-1);

	for (i = 0; i < 20; i++) {
		if (*p2 == '/')
			break;
		ip[i] = *p2;
		p2++;
	}

	ip[i] = '\0';

	if (inet_aton(ip, &tmp) == 0)
		return (-1);

	/* to skip the "/". */
	p++;

	for (i = 0; i < 2; i++) {
		cidr[i] = *p;
		p++;
	}
	cidr[2] = '\0';

	librouter_quagga_cidr_to_netmask(atoi(cidr), mask);
	sprintf(buf, "%s %s", ip, mask);

	return 0;
}

/* Takes a network address using classic notation (10.5.0.1 255.0.0.0) */
/* and creates in buf a networ address using cidr notation(10.5.0.1/8) */
int librouter_quagga_classic_to_cidr(char *addr, char *mask, char *buf)
{
	int bits;

	bits = librouter_quagga_netmask_to_cidr(mask);

	if (bits < 0)
		return (-1);

	sprintf(buf, "%s/%d", addr, bits);

	return 0;
}

/* Takes an octet string mask (e.g. "255.255.255.192") and creates    */
/* an CIDR notation. The argument buf should be the string and myVal  */
/* is a pointer to an unsigned int. It's based on Whatmask util       */
/* source code.                                                       */
int librouter_quagga_netmask_to_cidr(char *buf)
{
	unsigned int tmp;

	if (librouter_quagga_octets_to_uint(&tmp, buf) < 0)
		return (-1);

	if (librouter_quagga_validate_mask(tmp) == 0)
		return (librouter_quagga_bitcount(tmp));

	return -1;
}

/* Takes the number of 1s bits (CIDR notation) and creates a   */
/* netmask string (e.g. "255.255.255.192"). The argument buf   */
/* should have at least 16 bytes allocated to it. It's based   */
/* on Whatmask util source code.                               */
int librouter_quagga_cidr_to_netmask(unsigned int cidr, char *buf)
{
	if ((0 <= cidr) && (cidr <= 32)) {
		librouter_quagga_bitfill_from_left(cidr);
		librouter_quagga_uint_to_octets(librouter_quagga_bitfill_from_left(cidr), buf);
		return 0;
	}

	return -1; //CIDR not valid!
}

/* Creates a netmask string (e.g. "255.255.255.192")  */
/* from an unsgined int. The argument buf should have */
/* at least 16 bytes allocated to it.                 */
char * librouter_quagga_uint_to_octets(unsigned int myval, char * buf)
{
	unsigned int val1, val2, val3, val4;

	val1 = myval & 0xff000000;
	val1 = val1 >> 24;
	val2 = myval & 0x00ff0000;
	val2 = val2 >> 16;
	val3 = myval & 0x0000ff00;
	val3 = val3 >> 8;
	val4 = myval & 0x000000ff;

	snprintf(buf, 16, "%u" "." "%u" "." "%u" "." "%u", val1, val2, val3, val4);

	return (buf);
}

/* Takes an octet string (e.g. "255.255.255.192")            */
/* and creates an unsgined int. The argument buf should be   */
/* the string and myVal is a pointer to an unsigned int      */
int librouter_quagga_octets_to_uint(unsigned int *myval, char *buf)
{
	unsigned int val1, val2, val3, val4;
	*myval = 0;

	/* failure */
	if (sscanf(buf, "%u" "." "%u" "." "%u" "." "%u", &val1, &val2, &val3, &val4) != 4) {
		return -1;
	}

	/* bad input */
	if (val1 > 255 || val2 > 255 || val3 > 255 || val4 > 255) {
		return -1;
	}

	*myval |= val4;
	val3 <<= 8;
	*myval |= val3;
	val2 <<= 16;
	*myval |= val2;
	val1 <<= 24;
	*myval |= val1;

	/* myval was passed by reference so it is already setup for the caller */
	/* success */
	return 0;
}

/* This will fill in numBits bits with ones into an unsigned int.    */
/* The fill starts on the left and moves toward the right like this: */
/* 11111111000000000000000000000000                                  */
/* The caller must be sure numBits is between 0 and 32 inclusive     */
unsigned int librouter_quagga_bitfill_from_left(unsigned int num_bits)
{
	unsigned int myval = 0;
	int i;

	/* fill up the bits by adding ones so we have numBits ones */
	for (i = 0; i < num_bits; i++) {
		myval = myval >> 1;
		myval |= 0x80000000;
	}

	return myval;
}

int librouter_quagga_bitcount(unsigned int my_int)
{
	int count = 0;

	while (my_int != 0) {
		count += my_int & 1;
		my_int >>= 1;
	}

	return count;
}

/* validatemask checks to see that an unsigned long    */
/* has all 1s in a row starting on the left like this */
/* Good: 11111111111111111111111110000000              */
int librouter_quagga_validate_mask(unsigned int myInt)
{
	int foundZero = 0;

	/* while more bits to count */
	while (myInt != 0) {

		/* test leftmost bit */
		if ((myInt & 0x80000000) != 0) {

			/* bad */
			if (foundZero)
				return (-1);
		} else {
			foundZero = 1;
		}

		/* shift off the left - zero fill */
		myInt <<= 1;
	}

	/* good */
	return 0;
}

int librouter_quagga_validate_ip(char *ip)
{
	unsigned int tmp;
	return (librouter_quagga_octets_to_uint(&tmp, ip));
}

#define PROG_RIPD "ripd"

int librouter_quagga_ripd_is_running(void)
{
	return librouter_exec_check_daemon(PROG_RIPD);
}

int librouter_quagga_ripd_exec(int on_noff)
{
	int ret;

	ret = librouter_exec_init_program(on_noff, PROG_RIPD);

	if (on_noff) {
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i = 0; i < 5; i++) {
			if (stat(RIP_PATH, &s_stat) < 0) {
				if (errno != ENOENT) {
					fprintf(stderr, "set_ripd: stat = %s\n", strerror(errno));
					return (-1);
				}
			} else
				break;

			/* Wait for daemon with 3s timeout! */
			sleep(1);
		}

		/* to be nice! */
		sleep(1);
	}

	return ret;
}

#define PROG_BGPD "bgpd"

int librouter_quagga_bgpd_is_running(void)
{
	return librouter_exec_check_daemon(PROG_BGPD);
}

int librouter_quagga_bgpd_exec(int on_noff)
{
	int ret;

	ret = librouter_exec_init_program(on_noff, PROG_BGPD);

	if (on_noff) {
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i = 0; i < 5; i++) {
			if (stat(BGP_PATH, &s_stat) < 0) {
				if (errno != ENOENT) {
					fprintf(stderr, "set_bgpd: stat = %s\n", strerror(errno));
					return (-1);
				}
			} else
				break;

			/* Wait for daemon with 3s timeout! */
			sleep(1);
		}

		/* to be nice! */
		sleep(1);
	}

	return ret;
}

#define PROG_OSPFD "ospfd"

int librouter_quagga_ospfd_is_running(void)
{
	return librouter_exec_check_daemon(PROG_OSPFD);
}

int librouter_quagga_ospfd_exec(int on_noff)
{
	int ret;

	ret = librouter_exec_init_program(on_noff, PROG_OSPFD);

	if (on_noff) {
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i = 0; i < 5; i++) {
			if (stat(OSPF_PATH, &s_stat) < 0) {
				if (errno != ENOENT) {
					fprintf(stderr, "set_ospfd: stat = %s\n", strerror(errno));
					return (-1);
				}
			} else
				break;

			/* Wait for daemon with 3s timeout! */
			sleep(1);
		}

		/* to be nice! */
		sleep(1);
	}
	return ret;
}

int librouter_quagga_zebra_hup(void)
{
	FILE *F;
	char buf[16];

	F = fopen(ZEBRA_PID, "r");
	if (!F)
		return 0;

	fgets(buf, 16, F);
	fclose(F);

	if (kill((pid_t) atoi(buf), SIGHUP)) {
		fprintf(stderr, "%% zebra[%i] seems to be down\n", atoi(buf));
		return (-1);
	}

	return 0;
}

/*  Testa presenca de rota default dinamica por RIP ou OSPF!
 -1 daemon nao carregado
 0 sem rota default dinamica
 1 com rota default dinamica
 R>* 0.0.0.0/0 [120/2] via 10.0.0.2, serial 0, 00:00:02

 A rota precisa estar selecionada: *>
 Cuidado com "ip default-route" no ppp
 */
#define ZEBRA_SYSTTY_OUTPUT_FILE "/var/run/.zebra_systty_out"
int librouter_quagga_floating_route(void)
{
	FILE *f;
	char buf[128];
	int ret = 0;

	if (!librouter_quagga_ripd_is_running() && !librouter_quagga_ospfd_is_running())
		return -1;

	if (librouter_quagga_connect_daemon(ZEBRA_PATH) < 0)
		return -1;

	f = fopen(ZEBRA_SYSTTY_OUTPUT_FILE, "wt");

	if (!f) {
		librouter_quagga_close_daemon();
		return -1;
	}

	librouter_quagga_execute_client("enable", stdout, NULL, 0);
	librouter_quagga_execute_client("show ip route", f, NULL, 1);

	fclose(f);

	librouter_quagga_close_daemon();
	f = fopen(ZEBRA_SYSTTY_OUTPUT_FILE, "rt");
	if (!f)
		return -1;

	while (!feof(f) && !ret) {
		if (fgets(buf, 128, f)) {
			if (strlen(buf) > 13) {
				if (strncmp(buf, "R>*", 3) == 0 || strncmp(buf,"O>*", 3) == 0) {
					if (strncmp(buf + 4, "0.0.0.0/0", 9) == 0) {
						ret = 1;
					}
				}
			}
		}
	}

	fclose(f);
	return ret;
}

/**
 *	lconfig_quagga_get_conf		Abre o arquivo de 'filename' e posiciona o file descriptor na linha:
 *  	- igual a 'key'
 *  	Retorna o file descriptor, ou NULL se nao for possivel abrir o arquivo
 *  	ou encontrar a posicao desejada.
 */
FILE * librouter_quagga_get_conf(char *filename, char *key)
{
	FILE *f;
	int len, found = 0;
	char buf[1024];

	f = fopen(filename, "r");

	if (!f)
		return f;

	while (!feof(f)) {
		fgets(buf, 1024, f);
		len = strlen(buf);
		librouter_str_striplf(buf);

		if (strncmp(buf, key, strlen(key)) == 0) {
			found = 1;
			fseek(f, -len, SEEK_CUR);
			break;
		}
	}

	if (found)
		return f;

	fclose(f);
	return NULL;
}

#ifdef OPTION_BGP
/**
 *  lconfig_bgp_get_conf	Abre o arquivo de configuracao do BGP e posiciona o file descriptor
 *
 *  main_ninterf = 1 -> posiciona no inicio da configuracao geral ('router bgp "nÃºmero do as"');
 nesse caso o argumento intf eh ignorado
 *  main_ninterf = 0 -> posiciona no inicio da configuracao da interface 'intf',
 *			sendo que 'intf' deve estar no formato linux (ex.: 'eth0')
 *
 */
FILE * librouter_quagga_bgp_get_conf(int main_nip)
{
	char key[64];

	if (main_nip)
		strcpy(key, "router bgp");
	else
		sprintf(key, "ip as-path");

	return librouter_quagga_get_conf(BGPD_CONF, key);
}

/* Search the ASN in bgpd configuration file */
int librouter_quagga_bgp_get_asn(void)
{
	FILE *bgp_conf;
	const char router_bgp[] = "router bgp ";
	char *buf, *asn_add;
	int bgp_asn = 0;

	if (!librouter_quagga_bgpd_is_running())
		return 0;

	bgp_conf = librouter_quagga_bgp_get_conf(1);

	if (bgp_conf) {
		buf = malloc(1024);
		asn_add = buf;
		while (!feof(bgp_conf)) {

			fgets(buf, 1024, bgp_conf);

			if (!strncmp(buf, router_bgp, strlen(router_bgp))) {
				asn_add += strlen(router_bgp); //move pointer to the AS number
				bgp_asn = atoi(asn_add);
				break;
			}
		}

		fclose(bgp_conf);
		free(buf);
	}

	return bgp_asn;
}
#endif /* OPTION_BGP */

/*  Abre o arquivo de configuracao do zebra e posiciona o file descriptor de
 *  acordo com o argumento:
 *  main_ninterf = 1 -> posiciona no inicio da configuracao geral (comandos 'ip route');
 *  			nesse caso o argumento intf eh ignorado
 *  main_ninterf = 0 -> posiciona no inicio da configuracao da interface 'intf',
 *			sendo que 'intf' deve estar no formato linux (ex.: 'eth0')
 */
FILE *librouter_quagga_zebra_get_conf(int main_ninterf, char *intf)
{
	char key[64];

	if (main_ninterf)
		strcpy(key, "ip route");
	else
		sprintf(key, "interface %s", intf);

	return librouter_quagga_get_conf(ZEBRA_CONF, key);
}

void librouter_quagga_zebra_dump_static_routes(FILE *out)
{
	FILE *f;
	char buf[1024];

	f = librouter_quagga_zebra_get_conf(1, NULL);

	if (!f)
		return;

	while (!feof(f)) {
		fgets(buf, 1024, f);

		if (buf[0] == '!')
			break;

		librouter_str_striplf(buf);

		fprintf(out, "%s\n", librouter_device_from_linux_cmdline(librouter_zebra_to_linux_cmdline(buf)));
	}

	fprintf(out, "!\n");

	fclose(f);
}

/*  Abre o arquivo de configuracao do RIP e posiciona o file descriptor de
 *  acordo com o argumento:
 *  main_ninterf = 1 -> posiciona no inicio da configuracao geral ('router rip');
 nesse caso o argumento intf eh ignorado
 *  main_ninterf = 0 -> posiciona no inicio da configuracao da interface 'intf',
 *			sendo que 'intf' deve estar no formato linux (ex.: 'eth0')
 */
FILE *librouter_quagga_rip_get_conf(int main_ninterf, char *intf)
{
	char key[64];

	if (main_ninterf)
		strcpy(key, "router rip");
	else
		sprintf(key, "interface %s", intf);

	return librouter_quagga_get_conf(RIPD_CONF, key);
}

/*  Abre o arquivo de configuracao do OSPF e posiciona o file descriptor de
 *  acordo com o argumento:
 *  main_ninterf = 1 -> posiciona no inicio da configuracao geral ('router ospf');
 nesse caso o argumento intf eh ignorado
 *  main_ninterf = 0 -> posiciona no inicio da configuracao da interface 'intf',
 *			sendo que 'intf' deve estar no formato linux (ex.: 'eth0')
 */
FILE *librouter_quagga_ospf_get_conf(int main_ninterf, char *intf)
{
	char key[64];

	if (main_ninterf)
		strcpy(key, "router ospf");
	else
		sprintf(key, "interface %s", intf);

	return librouter_quagga_get_conf(OSPFD_CONF, key);
}

/* Higher level of abstraction functions - 18/06/2010 */

/**
 * librouter_quagga_get_routes
 *
 * Get routes from system and return a linked
 * list of struct routes_t
 *
 * @return
 */
struct routes_t * librouter_quagga_get_routes(void)
{
	FILE *f;
	char buf[256];
	struct routes_t *route = NULL;
	struct routes_t *cur = NULL, *next = NULL;
	char sha1[20];
	sha_ctx hashctx;
	int first = 1;

	f = fopen(CGI_TMP_FILE, "wt");
	if (f == NULL)
		return NULL;

	librouter_quagga_zebra_dump_static_routes(f);
	fclose(f);

	f = fopen(CGI_TMP_FILE, "r");
	if (f == NULL)
		return NULL;

	while (fgets(buf, sizeof(buf), f)) {
		arglist *args = librouter_make_args(buf);

		/* At least 5 arguments */
		if (args->argc < 5)
			break;

		next = malloc(sizeof(struct routes_t));

		if (next == NULL) {
			syslog(LOG_ERR, " Not enough space to store route information\n");
			return NULL;
		}

		memset(next, 0, sizeof(struct routes_t));

		if (first) {
			route = cur = next;
			first = 0;
		} else {
			cur->next = next;
			cur = next;
		}

		cur->network = strdup(args->argv[2]);
		cur->mask = strdup(args->argv[3]);

		if (isdigit(args->argv[4][0])) {
			cur->gateway = strdup(args->argv[4]);
		} else {
			/* FIXME Why is this so fucking difficult? We should not have a CLI string here */
			cur->interface = strdup(librouter_device_cli_to_linux(args->argv[4],
			                atoi(args->argv[5]), -1));
		}

		if (cur->gateway && args->argc == 6)
			cur->metric = atoi(args->argv[5]);
		else if (cur->interface && args->argc == 7)
			cur->metric = atoi(args->argv[6]);

		/* Unique route identifier */
		sha1_init(&hashctx);
		sha1_update(&hashctx, (unsigned char *) cur->network, strlen(cur->network));
		sha1_update(&hashctx, (unsigned char *) cur->mask, strlen(cur->mask));

		if (cur->gateway)
			sha1_update(&hashctx, (unsigned char *) cur->gateway, strlen(cur->gateway));

		if (cur->interface)
			sha1_update(&hashctx, (unsigned char *) cur->interface,strlen(cur->interface));

		sha1_final((unsigned char *) sha1, &hashctx);

		cur->hash = strdup(sha1_to_hex((unsigned char *) sha1));
	}

	fclose(f);

	return route;
}

/**
 * librouter_quagga_free_routes
 *
 * Free a linked list of struct routes_t
 *
 * @param route
 */
void librouter_quagga_free_routes(struct routes_t *route)
{
	struct routes_t *next;

	while (route != NULL) {
		if (route->network)
			free(route->network);

		if (route->mask)
			free(route->mask);

		if (route->interface)
			free(route->interface);

		if (route->gateway)
			free(route->gateway);

		if (route->hash)
			free(route->hash);

		next = route->next;
		free(route);
		route = next;
	}
}

/**
 * librouter_quagga_add_route
 *
 * Apply route to system from received structure
 *
 * @param route
 * @return 0 if success, -1 otherwise
 */
int librouter_quagga_add_route(struct routes_t *route)
{
	char buf_daemon[256];
	char zebra_cmd[256];

	if (librouter_quagga_connect_daemon(ZEBRA_PATH) < 0)
		return -1;

	if (route->gateway)
		sprintf(zebra_cmd, "ip route %s %s %s %d", route->network, route->mask,
	                route->gateway, route->metric ? route->metric : 1);
	else
		sprintf(zebra_cmd, "ip route %s %s %s %d", route->network, route->mask,
	                route->interface, route->metric ? route->metric : 1);

	librouter_quagga_execute_client("enable", stdout, buf_daemon, 0);
	librouter_quagga_execute_client("configure terminal", stdout, buf_daemon, 0);
	librouter_quagga_execute_client(zebra_cmd, stdout, buf_daemon, 0);
	librouter_quagga_execute_client("write file", stdout, buf_daemon, 0);

	librouter_quagga_close_daemon();

	return 0;
}

/**
 * __del_route
 *
 * Delete route from system
 *
 * @param route
 */
static void __del_route(struct routes_t *route)
{
	char buf_daemon[256];
	char zebra_cmd[256];

	if (librouter_quagga_connect_daemon(ZEBRA_PATH) < 0)
		return;

	if (route->gateway)
		sprintf(zebra_cmd, "no ip route %s %s %s", route->network, route->mask,
		                route->gateway);
	else if (route->interface)
		sprintf(zebra_cmd, "no ip route %s %s %s", route->network, route->mask,
		                route->interface);
	else {
		librouter_quagga_close_daemon();
		return;
	}

	librouter_quagga_execute_client("enable", stdout, buf_daemon, 0);
	librouter_quagga_execute_client("configure terminal", stdout, buf_daemon, 0);
	librouter_quagga_execute_client(zebra_cmd, stdout, buf_daemon, 0);
	librouter_quagga_execute_client("write file", stdout, buf_daemon, 0);

	librouter_quagga_close_daemon();
}

/**
 * librouter_quagga_del_route
 *
 * Exported function to deleted a route from system.
 * Receives the hash as a function, used by the web
 * configurator for now.
 *
 * @param hash
 * @return
 */
int librouter_quagga_del_route(char *hash)
{
	struct routes_t *route, *next;

	next = route = librouter_quagga_get_routes();
	if (route == NULL)
		return 0;

	while (next) {
		if (!strcmp(hash, next->hash)) {
			__del_route(next);
			break;
		}
		next = next->next;
	}

	librouter_quagga_free_routes(route);
	return 0;
}
