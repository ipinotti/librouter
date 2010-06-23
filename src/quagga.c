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
#include <linux/config.h>

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

//-------------------------------------------------------------------------------
// functions created to handle the daemons of zebra package  
//-------------------------------------------------------------------------------

/* Making connection to protocol daemon */
int daemon_connect(char *path)
{
	int i, ret = 0, sock, len;
	struct sockaddr_un addr;
	struct stat s_stat;

	fd_daemon = -1;

	/* Stat socket to see if we have permission to access it. */
	for (i = 0; i < 3; i++) /* Wait for daemon with 3s timeout! */
	{
		if (stat(path, &s_stat) < 0) {
			if (errno != ENOENT) {
				fprintf(
				                stderr,
				                "daemon_connect(%s): stat = %s\n",
				                path, strerror(errno));
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
		fprintf(
		                stderr,
		                "daemon_connect(%s): No permission to access socket\n",
		                path);
		return (-1);
	}
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "daemon_connect(): socket = %s\n", strerror(
		                errno));
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
		fprintf(stderr, "daemon_connect(): connect = %s\n", strerror(
		                errno));
		close(sock);
		return (-1);
	}
	fd_daemon = sock;
	return (0);
}

int daemon_client_execute(char *line, FILE *fp, char *buf_daemon, int show)
{
	int i, ret, nbytes;
	char buf[1024];

	if (fd_daemon < 0)
		return 0;
	ret = write(fd_daemon, line, strlen(line) + 1);
	if (ret <= 0) {
		fprintf(stderr, "daemon_client_execute(%s): write error!\n",
		                line);
		fd_daemon_close();
		return 0;
	}
	if (buf_daemon)
		strcpy(buf_daemon, buf); /* clean buf_daemon before writing to it */
	while (1) {
		nbytes = read(fd_daemon, buf, sizeof(buf) - 1);
		if (nbytes <= 0 && errno != EINTR) {
			fprintf(
			                stderr,
			                "daemon_client_execute(%s): read error!\n",
			                line);
			fd_daemon_close();
			return 0;
		}
		if (nbytes > 0) {
			buf[nbytes] = '\0';
			if (show) {
				fprintf(fp, "%s", buf);
				fflush(fp);
			}
			if (buf_daemon)
				strcat(buf_daemon, buf);/* Uses strcat to not overwrite*/
			if (nbytes >= 4) {
				i = nbytes - 4;
				if (buf[i] == '\0' && buf[i + 1] == '\0'
				                && buf[i + 2] == '\0') {
					ret = buf[i + 3];
					break;
				}
			}
		}
	}
	return ret;
}

void fd_daemon_close(void)
{
	if (fd_daemon > 0)
		close(fd_daemon);
	fd_daemon = -1;
}

#define ZEBRA_OUTPUT_FILE "/var/run/.zebra_out"
FILE *zebra_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (daemon_connect(ZEBRA_PATH) < 0)
		return NULL;

	f = fopen(ZEBRA_OUTPUT_FILE, "wt");
	if (!f) {
		fd_daemon_close();
		return NULL;
	}
	new_cmdline = libconfig_device_to_linux_cmdline((char*) cmdline);
	daemon_client_execute("enable", stdout, NULL, 0);
	daemon_client_execute(new_cmdline, f, NULL, 1);
	fclose(f);
	fd_daemon_close();

	return fopen(ZEBRA_OUTPUT_FILE, "rt");
}

#define OSPF_OUTPUT_FILE "/var/run/.ospf_out"
FILE *ospf_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (daemon_connect(OSPF_PATH) < 0)
		return NULL;

	f = fopen(OSPF_OUTPUT_FILE, "wt");
	if (!f) {
		fd_daemon_close();
		return NULL;
	}
	new_cmdline = libconfig_device_to_linux_cmdline((char*) cmdline);
	daemon_client_execute("enable", stdout, NULL, 0);
	daemon_client_execute(new_cmdline, f, NULL, 1);
	fclose(f);
	fd_daemon_close();

	return fopen(OSPF_OUTPUT_FILE, "rt");
}

#define RIP_OUTPUT_FILE "/var/run/.rip_out"
FILE *rip_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (daemon_connect(RIP_PATH) < 0)
		return NULL;

	f = fopen(RIP_OUTPUT_FILE, "wt");
	if (!f) {
		fd_daemon_close();
		return NULL;
	}
	new_cmdline = libconfig_device_to_linux_cmdline((char*) cmdline);
	daemon_client_execute("enable", stdout, NULL, 0);
	daemon_client_execute(new_cmdline, f, NULL, 1);
	fclose(f);
	fd_daemon_close();

	return fopen(RIP_OUTPUT_FILE, "rt");
}

#define BGP_OUTPUT_FILE "/var/run/.bgp_out"
FILE *bgp_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (daemon_connect(BGP_PATH) < 0)
		return NULL;

	f = fopen(BGP_OUTPUT_FILE, "wt");
	if (!f) {
		fd_daemon_close();
		return NULL;
	}
	new_cmdline = libconfig_device_to_linux_cmdline((char*) cmdline);
	daemon_client_execute("enable", stdout, NULL, 0);
	daemon_client_execute(new_cmdline, f, NULL, 1);
	fclose(f);
	fd_daemon_close();

	return fopen(BGP_OUTPUT_FILE, "rt");
}

//-------------------------------------------------------------------------------
// Functions to handle different netmask notations (classic and CIDR)  
//-------------------------------------------------------------------------------

/* Takes an cidr address (10.5.0.1/8) and creates in  */
/* buf a classic address(10.5.0.1 255.0.0.0)          */
int cidr_to_classic(char *cidr_addr, char *buf)
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

	p++; //to skip the "/".

	for (i = 0; i < 2; i++) {
		cidr[i] = *p;
		p++;
	}
	cidr[2] = '\0';

	cidr_to_netmask(atoi(cidr), mask);
	sprintf(buf, "%s %s", ip, mask);

	return 0;
}

/* Takes a network address using classic notation (10.5.0.1 255.0.0.0) */
/* and creates in buf a networ address using cidr notation(10.5.0.1/8) */
int classic_to_cidr(char *addr, char *mask, char *buf)
{
	int bits;

	bits = netmask_to_cidr(mask);
	if (bits < 0)
		return (-1);
	sprintf(buf, "%s/%d", addr, bits);

	return 0;
}

/* Takes an octet string mask (e.g. "255.255.255.192") and creates    */
/* an CIDR notation. The argument buf should be the string and myVal  */
/* is a pointer to an unsigned int. It's based on Whatmask util       */
/* source code.                                                       */
int netmask_to_cidr(char *buf)
{
	unsigned int tmp;
	if (octets_to_u_int(&tmp, buf) < 0)
		return (-1);
	if (validatemask(tmp) == 0) {
		return (bitcount(tmp));
	}
	return (-1);
}

/* Takes the number of 1s bits (CIDR notation) and creates a   */
/* netmask string (e.g. "255.255.255.192"). The argument buf   */
/* should have at least 16 bytes allocated to it. It's based   */
/* on Whatmask util source code.                               */
int cidr_to_netmask(unsigned int cidr, char *buf)
{
	if ((0 <= cidr) && (cidr <= 32)) {
		bitfill_from_left(cidr);
		u_int_to_octets(bitfill_from_left(cidr), buf);
		return 0;
	}

	return (-1); //CIDR not valid!
}

/* Creates a netmask string (e.g. "255.255.255.192")  */
/* from an unsgined int. The argument buf should have */
/* at least 16 bytes allocated to it.                 */
char* u_int_to_octets(unsigned int myVal, char* buf)
{
	unsigned int val1, val2, val3, val4;

	val1 = myVal & 0xff000000;
	val1 = val1 >> 24;
	val2 = myVal & 0x00ff0000;
	val2 = val2 >> 16;
	val3 = myVal & 0x0000ff00;
	val3 = val3 >> 8;
	val4 = myVal & 0x000000ff;

	snprintf(buf, 16, "%u" "." "%u" "." "%u" "." "%u", val1, val2, val3,
	                val4);

	return (buf);

}

/* Takes an octet string (e.g. "255.255.255.192")            */
/* and creates an unsgined int. The argument buf should be   */
/* the string and myVal is a pointer to an unsigned int      */
int octets_to_u_int(unsigned int *myVal, char *buf)
{
	unsigned int val1, val2, val3, val4;
	*myVal = 0;

	if (sscanf(buf, "%u" "." "%u" "." "%u" "." "%u", &val1, &val2, &val3,
	                &val4) != 4) {
		return (-1); /* failure */
	}

	if (val1 > 255 || val2 > 255 || val3 > 255 || val4 > 255) { /* bad input */
		return (-1);
	}

	*myVal |= val4;
	val3 <<= 8;
	*myVal |= val3;
	val2 <<= 16;
	*myVal |= val2;
	val1 <<= 24;
	*myVal |= val1;

	/* myVal was passed by reference so it is already setup for the caller */
	return 0; /* success */

}

/* This will fill in numBits bits with ones into an unsigned int.    */
/* The fill starts on the left and moves toward the right like this: */
/* 11111111000000000000000000000000                                  */
/* The caller must be sure numBits is between 0 and 32 inclusive     */
unsigned int bitfill_from_left(unsigned int numBits)
{
	unsigned int myVal = 0;
	int i;

	/* fill up the bits by adding ones so we have numBits ones */
	for (i = 0; i < numBits; i++) {
		myVal = myVal >> 1;
		myVal |= 0x80000000;
	}

	return myVal;
}

int bitcount(unsigned int myInt)
{
	int count = 0;

	while (myInt != 0) {
		count += myInt & 1;
		myInt >>= 1;
	}

	return count;
}

/* validatemask checks to see that an unsigned long    */
/* has all 1s in a row starting on the left like this */
/* Good: 11111111111111111111111110000000              */
int validatemask(unsigned int myInt)
{
	int foundZero = 0;

	while (myInt != 0) { /* while more bits to count */

		if ((myInt & 0x80000000) != 0) /* test leftmost bit */
		{
			if (foundZero)
				return (-1); /* bad */
		} else {
			foundZero = 1;
		}

		myInt <<= 1; /* shift off the left - zero fill */
	}

	return 0; /* good */
}

int validateip(char *ip)
{
	unsigned int tmp;
	return (octets_to_u_int(&tmp, ip));
}

#define PROG_RIPD "ripd"

int get_ripd(void)
{
	return libconfig_exec_check_daemon(PROG_RIPD);
}

int set_ripd(int on_noff)
{
	int ret;

	ret = libconfig_exec_init_program(on_noff, PROG_RIPD);
	if (on_noff) {
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i = 0; i < 5; i++) {
			if (stat(RIP_PATH, &s_stat) < 0) {
				if (errno != ENOENT) {
					fprintf(
					                stderr,
					                "set_ripd: stat = %s\n",
					                strerror(errno));
					return (-1);
				}
			} else
				break;
			sleep(1); /* Wait for daemon with 3s timeout! */
		}
		sleep(1); /* to be nice! */
	}
	return ret;
}

#define PROG_BGPD "bgpd"

int get_bgpd(void)
{
	return libconfig_exec_check_daemon(PROG_BGPD);
}

int set_bgpd(int on_noff)
{
	int ret;

	ret = libconfig_exec_init_program(on_noff, PROG_BGPD);
	if (on_noff) {
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i = 0; i < 5; i++) {
			if (stat(BGP_PATH, &s_stat) < 0) {
				if (errno != ENOENT) {
					fprintf(
					                stderr,
					                "set_bgpd: stat = %s\n",
					                strerror(errno));
					return (-1);
				}
			} else
				break;
			sleep(1); /* Wait for daemon with 3s timeout! */
		}
		sleep(1); /* to be nice! */
	}
	return ret;
}

#define PROG_OSPFD "ospfd"

int get_ospfd(void)
{
	return libconfig_exec_check_daemon(PROG_OSPFD);
}

int set_ospfd(int on_noff)
{
	int ret;

	ret = libconfig_exec_init_program(on_noff, PROG_OSPFD);
	if (on_noff) {
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i = 0; i < 5; i++) {
			if (stat(OSPF_PATH, &s_stat) < 0) {
				if (errno != ENOENT) {
					fprintf(
					                stderr,
					                "set_ospfd: stat = %s\n",
					                strerror(errno));
					return (-1);
				}
			} else
				break;
			sleep(1); /* Wait for daemon with 3s timeout! */
		}
		sleep(1); /* to be nice! */
	}
	return ret;
}

int zebra_hup(void)
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
int rota_flutuante(void)
{
	FILE *f;
	char buf[128];
	int ret = 0;

	if (!get_ripd() && !get_ospfd())
		return -1;

	if (daemon_connect(ZEBRA_PATH) < 0)
		return -1;
	f = fopen(ZEBRA_SYSTTY_OUTPUT_FILE, "wt");
	if (!f) {
		fd_daemon_close();
		return -1;
	}
	daemon_client_execute("enable", stdout, NULL, 0);
	daemon_client_execute("show ip route", f, NULL, 1);
	fclose(f);
	fd_daemon_close();
	f = fopen(ZEBRA_SYSTTY_OUTPUT_FILE, "rt");
	if (!f)
		return -1;
	while (!feof(f) && !ret) {
		if (fgets(buf, 128, f)) {
			if (strlen(buf) > 13) {
				if (strncmp(buf, "R>*", 3) == 0 || strncmp(buf,
				                "O>*", 3) == 0) {
					if (strncmp(buf + 4, "0.0.0.0/0", 9)
					                == 0) {
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
FILE * lconfig_quagga_get_conf(char *filename, char *key)
{
	FILE *f;
	int len, found = 0;
	char buf[1024];

	f = fopen(filename, "rt");
	if (!f)
		return f;
	while (!feof(f)) {
		fgets(buf, 1024, f);
		len = strlen(buf);
		libconfig_str_striplf(buf);
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
FILE * lconfig_bgp_get_conf(int main_nip)
{
	char key[64];

	if (main_nip)
		strcpy(key, "router bgp");
	else
		sprintf(key, "ip as-path");

	return lconfig_quagga_get_conf(BGPD_CONF, key);
}


/* Search the ASN in bgpd configuration file */
int lconfig_bgp_get_asn(void)
{
	FILE *bgp_conf;
	const char router_bgp[] = "router bgp ";
	char *buf, *asn_add;
	int bgp_asn = 0;

	if (!get_bgpd()) return 0;

	bgp_conf = lconfig_bgp_get_conf(1);
	if (bgp_conf)
	{
		buf=malloc(1024);
		asn_add=buf;
		while(!feof(bgp_conf))
		{
			fgets(buf, 1024, bgp_conf);
			if (!strncmp(buf,router_bgp,strlen(router_bgp)))
			{
				asn_add+=strlen(router_bgp); //move pointer to the AS number
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
 *  main_ninterf = 1 -> posiciona no inicio da configuracao geral (comandos
 			'ip route'); nesse caso o argumento intf eh ignorado
 *  main_ninterf = 0 -> posiciona no inicio da configuracao da interface 'intf',
 *			sendo que 'intf' deve estar no formato linux (ex.: 'eth0')
 */
FILE *zebra_get_conf(int main_ninterf, char *intf)
{
	char key[64];

	if (main_ninterf)
		strcpy(key, "ip route");
	else
		sprintf(key, "interface %s", intf);

	return lconfig_quagga_get_conf(ZEBRA_CONF, key);
}

void zebra_dump_static_routes_conf(FILE *out)
{
	FILE *f;
	char buf[1024];

	f = zebra_get_conf(1, NULL);

	if (!f)
		return;

	while (!feof(f)) {
		fgets(buf, 1024, f);
		if (buf[0] == '!')
			break;
		libconfig_str_striplf(buf);
		fprintf(out, "%s\n", libconfig_device_from_linux_cmdline(
		                libconfig_zebra_to_linux_cmdline(buf)));
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
FILE *rip_get_conf(int main_ninterf, char *intf)
{
	char key[64];

	if (main_ninterf)
		strcpy(key, "router rip");
	else
		sprintf(key, "interface %s", intf);

	return lconfig_quagga_get_conf(RIPD_CONF, key);
}


/*  Abre o arquivo de configuracao do OSPF e posiciona o file descriptor de
 *  acordo com o argumento:
 *  main_ninterf = 1 -> posiciona no inicio da configuracao geral ('router ospf');
 			nesse caso o argumento intf eh ignorado
 *  main_ninterf = 0 -> posiciona no inicio da configuracao da interface 'intf',
 *			sendo que 'intf' deve estar no formato linux (ex.: 'eth0')
 */
FILE *ospf_get_conf(int main_ninterf, char *intf)
{
	char key[64];

	if (main_ninterf)
		strcpy(key, "router ospf");
	else
		sprintf(key, "interface %s", intf);

	return lconfig_quagga_get_conf(OSPFD_CONF, key);
}


/* Higher level of abstraction functions - 18/06/2010 */

/**
 * lconfig_get_routes
 *
 * Get routes from system and return a linked
 * list of struct routes_t
 *
 * @return
 */
struct routes_t * lconfig_get_routes(void)
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

	lconfig_dump_routing(f);
	fclose(f);

	f = fopen(CGI_TMP_FILE, "r");
	if (f == NULL)
		return NULL;

	while (fgets(buf, sizeof(buf), f)) {
		arglist *args = libconfig_make_args(buf);

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
			cur->interface = malloc(32);
			sprintf(cur->interface, "%s %s", args->argv[4], args->argv[5]);
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
			sha1_update(&hashctx, (unsigned char *) cur->interface, strlen(
			                cur->interface));
		sha1_final((unsigned char *) sha1, &hashctx);

		cur->hash = strdup(sha1_to_hex((unsigned char *) sha1));

	}

	fclose(f);

	return route;
}

/**
 * lconfig_free_routes
 *
 * Free a linked list of struct routes_t
 *
 * @param route
 */
void lconfig_free_routes(struct routes_t *route)
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
 * lconfig_add_route
 *
 * Apply route to system from received structure
 *
 * @param route
 * @return 0 if success, -1 otherwise
 */
int lconfig_add_route(struct routes_t *route)
{
	char buf_daemon[256];
	char zebra_cmd[256];

	if (daemon_connect(ZEBRA_PATH) < 0)
		return -1;

	sprintf(zebra_cmd, "ip route %s %s %s", route->network, route->mask, route->gateway);

	daemon_client_execute("enable", stdout, buf_daemon, 0);
	daemon_client_execute("configure terminal", stdout, buf_daemon, 0);
	daemon_client_execute(zebra_cmd, stdout, buf_daemon, 0);
	daemon_client_execute("write file", stdout, buf_daemon, 0);

	fd_daemon_close();

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

	if (daemon_connect(ZEBRA_PATH) < 0)
		return;

	sprintf(zebra_cmd, "no ip route %s %s %s", route->network, route->mask, route->gateway);

	daemon_client_execute("enable", stdout, buf_daemon, 0);
	daemon_client_execute("configure terminal", stdout, buf_daemon, 0);
	daemon_client_execute(zebra_cmd, stdout, buf_daemon, 0);
	daemon_client_execute("write file", stdout, buf_daemon, 0);

	fd_daemon_close();
}

/**
 * lconfig_del_route
 *
 * Exported function to deleted a route from system.
 * Receives the hash as a function, used by the web
 * configurator for now.
 *
 * @param hash
 * @return
 */
int lconfig_del_route(char *hash)
{
	struct routes_t *route, *next;

	next = route = lconfig_get_routes();
	if (route == NULL)
		return 0;

	while(next) {
		if (!strcmp(hash, next->hash)) {
			__del_route(next);
			break;
		}
		next = next->next;
	}

	lconfig_free_routes(route);
	return 0;
}
