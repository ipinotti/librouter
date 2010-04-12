
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

#include <linux/config.h>

#include <libconfig/device.h>
#include <libconfig/error.h>
#include <libconfig/exec.h>
#include <libconfig/process.h>
#include <libconfig/quagga.h>
#include <libconfig/str.h>

static int fd_daemon;

//-------------------------------------------------------------------------------
// functions created to handle the daemons of zebra package  
//-------------------------------------------------------------------------------

/* Making connection to protocol daemon */
int daemon_connect(char *path)
{
	int i, ret=0, sock, len;
	struct sockaddr_un addr;
	struct stat s_stat;

	fd_daemon = -1;

	/* Stat socket to see if we have permission to access it. */
	for (i=0; i < 3; i++) /* Wait for daemon with 3s timeout! */
	{
		if (stat(path, &s_stat) < 0)
		{
			if (errno != ENOENT)
			{
				fprintf(stderr, "daemon_connect(%s): stat = %s\n", path, strerror(errno));
				return(-1);
			}
		}
			else break;
		sleep(1);
	}
	if (!S_ISSOCK(s_stat.st_mode))
	{
		fprintf(stderr, "daemon_connect(%s): Not a socket\n", path);
		return(-1);
	}
	if (!(s_stat.st_mode & S_IWUSR) || !(s_stat.st_mode & S_IRUSR))
	{
		fprintf(stderr, "daemon_connect(%s): No permission to access socket\n", path);
		return(-1);
	}
	if ((sock=socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		fprintf(stderr, "daemon_connect(): socket = %s\n", strerror(errno));
		return(-1);
	}
	memset(&addr, 0, sizeof (struct sockaddr_un));
	addr.sun_family=AF_UNIX;
	strncpy(addr.sun_path, path, strlen (path));
	len=sizeof(addr.sun_family)+strlen(addr.sun_path);
	for (i=0; i < 3; i++) {
		if ((ret=connect(sock, (struct sockaddr *)&addr, len)) == 0)
			break;
		sleep(1);
	}
	if (ret == -1) {
		fprintf(stderr, "daemon_connect(): connect = %s\n", strerror(errno));
		close(sock);
		return(-1);
	}
	fd_daemon=sock;
	return(0);
}

int daemon_client_execute(char *line, FILE *fp, char *buf_daemon, int show)
{
	int i, ret, nbytes;
	char buf[1024];

	if (fd_daemon < 0) return 0;
	ret = write(fd_daemon, line, strlen(line)+1);
	if (ret <= 0)
	{
		fprintf(stderr, "daemon_client_execute(%s): write error!\n", line);
		fd_daemon_close();
		return 0;
	}
	if (buf_daemon) strcpy(buf_daemon,buf);	/* clean buf_daemon before writing to it */
	while (1)
	{
		nbytes = read(fd_daemon, buf, sizeof(buf)-1);
		if (nbytes <= 0 && errno != EINTR)
		{
			fprintf(stderr, "daemon_client_execute(%s): read error!\n", line);
			fd_daemon_close();
			return 0;
		}
		if (nbytes > 0)
		{
			buf[nbytes] = '\0';
			if (show)
			{
				fprintf(fp, "%s", buf);
				fflush(fp);
			}
			if (buf_daemon) strcat(buf_daemon, buf);/* Uses strcat to not overwrite*/	
			if (nbytes >= 4)
			{
				i = nbytes - 4;
				if (buf[i] == '\0' && buf[i + 1] == '\0' && buf[i + 2] == '\0')
				{
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
	if (fd_daemon > 0) close(fd_daemon);
	fd_daemon = -1;
}

#define ZEBRA_OUTPUT_FILE "/var/run/.zebra_out"
FILE *zebra_show_cmd(const char *cmdline)
{
	FILE *f;
	char *new_cmdline;

	if (daemon_connect(ZEBRA_PATH) < 0) return NULL;

	f=fopen(ZEBRA_OUTPUT_FILE, "wt");
	if (!f)
	{
		fd_daemon_close();
		return NULL;
	}
	new_cmdline = cish_to_linux_dev_cmdline((char*)cmdline);
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

	if (daemon_connect(OSPF_PATH) < 0) return NULL;

	f=fopen(OSPF_OUTPUT_FILE, "wt");
	if (!f)
	{
		fd_daemon_close();
		return NULL;
	}
	new_cmdline=cish_to_linux_dev_cmdline((char*)cmdline);
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

	if (daemon_connect(RIP_PATH) < 0) return NULL;

	f=fopen(RIP_OUTPUT_FILE, "wt");
	if (!f)
	{
		fd_daemon_close();
		return NULL;
	}
	new_cmdline=cish_to_linux_dev_cmdline((char*)cmdline);
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

	if (daemon_connect(BGP_PATH) < 0) return NULL;

	f=fopen(BGP_OUTPUT_FILE, "wt");
	if (!f)
	{
		fd_daemon_close();
		return NULL;
	}
	new_cmdline=cish_to_linux_dev_cmdline((char*)cmdline);
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
	p = strstr(cidr_addr,"/");
	if (p==NULL) return (-1);

	for (i=0; i<20;i++)
	{
		if (*p2 == '/') break;
		ip[i]=*p2;
		p2++;
	}
	ip[i]= '\0';
	
	if (inet_aton(ip, &tmp)==0) return (-1);

	p ++; //to skip the "/".

	for(i=0; i<2;i++)
	{
		cidr[i]=*p;
		p++;
	}
	cidr[2]='\0';

	cidr_to_netmask(atoi(cidr), mask);
	sprintf(buf, "%s %s", ip ,mask);

	return 0;
}
 
/* Takes a network address using classic notation (10.5.0.1 255.0.0.0) */
/* and creates in buf a networ address using cidr notation(10.5.0.1/8) */
int classic_to_cidr(char *addr, char *mask, char *buf)
{
	int bits;

	bits = netmask_to_cidr(mask);
	if (bits < 0) return (-1); 
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
	if (octets_to_u_int(&tmp, buf)<0) return (-1);
	if (validatemask(tmp)==0)
	{
		return (bitcount(tmp));
	}
	return (-1);
}

/* Takes the number of 1s bits (CIDR notation) and creates a   */
/* netmask string (e.g. "255.255.255.192"). The argument buf   */
/* should have at least 16 bytes allocated to it. It's based   */
/* on Whatmask util source code.                               */
int cidr_to_netmask( unsigned int cidr, char *buf)
{
	if ((0 <= cidr)&&(cidr <= 32))
	{
		bitfill_from_left(cidr);
		u_int_to_octets(bitfill_from_left(cidr),buf);
		return 0;
	}

	return (-1); //CIDR not valid!
}

/* Creates a netmask string (e.g. "255.255.255.192")  */
/* from an unsgined int. The argument buf should have */
/* at least 16 bytes allocated to it.                 */
char* u_int_to_octets( unsigned int myVal, char* buf)
{
	unsigned int val1, val2, val3, val4;

	val1 = myVal & 0xff000000;
	val1 = val1  >>24;
	val2 = myVal & 0x00ff0000;
	val2 = val2  >>16;
	val3 = myVal & 0x0000ff00;
	val3 = val3 >>8;
	val4 = myVal & 0x000000ff;

	snprintf(buf, 16, "%u" "." "%u" "." "%u" "." "%u", val1, val2, val3, val4);

	return(buf);

}

/* Takes an octet string (e.g. "255.255.255.192")            */
/* and creates an unsgined int. The argument buf should be   */
/* the string and myVal is a pointer to an unsigned int      */
int octets_to_u_int( unsigned int *myVal, char *buf)
{
	unsigned int val1, val2, val3, val4;
	*myVal = 0;

	if( sscanf(buf, "%u" "." "%u" "." "%u" "." "%u", &val1, &val2, &val3, &val4) != 4 )
	{
		return(-1); /* failure */
	}

	if( val1 > 255 || val2 > 255 || val3 > 255 || val4 > 255   )
	{      /* bad input */
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
unsigned int bitfill_from_left( unsigned int numBits )
{
	unsigned int myVal = 0;
	int i;

	/* fill up the bits by adding ones so we have numBits ones */
	for(i = 0; i< numBits; i++)
	{
		myVal = myVal >> 1;
		myVal |= 0x80000000;
	}

	return myVal;
}

int bitcount(unsigned int myInt)
{
	int count = 0;

	while (myInt != 0)
	{
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

	while (myInt != 0)
	{  /* while more bits to count */

		if( (myInt  & 0x80000000) !=0)	/* test leftmost bit */
		{
			if(foundZero)
				return (-1); /* bad */
		}
		else
		{
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
	return is_daemon_running(PROG_RIPD);
}
 
int set_ripd(int on_noff)
{
	int ret;
 
	ret=init_program(on_noff, PROG_RIPD);
	if (on_noff)
	{
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i=0; i < 5; i++)
		{
			if (stat(RIP_PATH, &s_stat) < 0)
			{
				if (errno != ENOENT) {
					fprintf(stderr, "set_ripd: stat = %s\n", strerror(errno));
					return(-1);
				}
			}
				else break;
			sleep(1); /* Wait for daemon with 3s timeout! */
		}
		sleep(1); /* to be nice! */
	}
	return ret;
}

#define PROG_BGPD "bgpd"

int get_bgpd(void)
{
	return is_daemon_running(PROG_BGPD);
}
 
int set_bgpd(int on_noff)
{
	int ret;
 
	ret=init_program(on_noff, PROG_BGPD);
	if (on_noff)
	{
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i=0; i < 5; i++)
		{
			if (stat(BGP_PATH, &s_stat) < 0)
			{
				if (errno != ENOENT) {
					fprintf(stderr, "set_bgpd: stat = %s\n", strerror(errno));
					return(-1);
				}
			}
				else break;
			sleep(1); /* Wait for daemon with 3s timeout! */
		}
		sleep(1); /* to be nice! */
	}
	return ret;
}

#define PROG_OSPFD "ospfd"

int get_ospfd(void)
{
	return is_daemon_running(PROG_OSPFD);
}
 
int set_ospfd(int on_noff)
{
	int ret;
 
	ret=init_program(on_noff, PROG_OSPFD);
	if (on_noff)
	{
		int i;
		struct stat s_stat;

		/* Stat socket to see if we have permission to access it. */
		for (i=0; i < 5; i++)
		{
			if (stat(OSPF_PATH, &s_stat) < 0)
			{
				if (errno != ENOENT) {
					fprintf(stderr, "set_ospfd: stat = %s\n", strerror(errno));
					return(-1);
				}
			}
				else break;
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

	F=fopen(ZEBRA_PID, "r");
	if (!F) return 0;
	fgets(buf, 16, F);
	fclose(F);
	if (kill((pid_t)atoi(buf), SIGHUP))
	{
		fprintf (stderr, "%% zebra[%i] seems to be down\n", atoi(buf));
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
	int ret=0;

	if (!get_ripd() && !get_ospfd())
		return -1;

	if (daemon_connect(ZEBRA_PATH) < 0) return -1;
	f=fopen(ZEBRA_SYSTTY_OUTPUT_FILE, "wt");
	if (!f)
	{
		fd_daemon_close();
		return -1;
	}
	daemon_client_execute("enable", stdout, NULL, 0);
	daemon_client_execute("show ip route", f, NULL, 1);
	fclose(f);
	fd_daemon_close();
	f=fopen(ZEBRA_SYSTTY_OUTPUT_FILE, "rt");
	if (!f)
		return -1;
	while (!feof(f) && !ret)
	{
		if (fgets(buf, 128, f))
		{
			if (strlen(buf) > 13)
			{
				if (strncmp(buf, "R>*", 3) == 0 || strncmp(buf, "O>*", 3) == 0)
				{
					if (strncmp(buf+4, "0.0.0.0/0", 9) == 0)
					{
						ret=1;
					}
				}
			}
		}
	}
	fclose(f);
	return ret;
}
