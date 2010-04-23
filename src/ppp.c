#include <linux/config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include "options.h"
#include "cish_defines.h"
#include "defines.h"
#include "nv.h"
#include "ppp.h"
#include "process.h"
#include "ipsec.h"
#include "ip.h"
#include "pam.h"

int notify_systtyd(void)
{
	FILE *F;
	char buf[16];

	F=fopen("/var/run/systty.pid", "r");
	if (!F) return 0;
	fgets(buf, 15, F);
	fclose(F);
	if (kill((pid_t)atoi(buf), SIGHUP))
	{
		fprintf(stderr, "%% systtyd[%i] seems to be down\n", atoi(buf));
		return (-1);
	}
	return 0;
}

int notify_mgetty(int serial_no)
{
	FILE *F;
	char buf[32];

	if (serial_no < MAX_WAN_INTF) sprintf(buf, MGETTY_PID_WAN, serial_no);
		else sprintf(buf, MGETTY_PID_AUX, serial_no-MAX_WAN_INTF);
	if ((F=fopen(buf, "r")))
	{
		fgets(buf, 31, F);
		fclose(F);
		if (kill((pid_t)atoi(buf), SIGHUP))
		{
			fprintf (stderr, "%% mgetty[%i] seems to be down\n", atoi(buf));
			return (-1);
		}
		return 0;
	}
	/* Try to signal tty lock owner */
	if (serial_no < MAX_WAN_INTF) sprintf(buf, LOCK_PID_WAN, serial_no);
		else sprintf(buf, LOCK_PID_AUX, serial_no-MAX_WAN_INTF);
	if ((F=fopen(buf, "r")))
	{
		fgets(buf, 31, F);
		fclose(F);
		if (kill((pid_t)atoi(buf), SIGHUP))
		{
			fprintf (stderr, "%% %i seems to be down\n", atoi(buf));
			return (-1);
		}
		return 0;
	}
	return (-1);
}

int ppp_get_config(int serial_no, ppp_config *cfg)
{
	FILE *f;
	char file[32];

	snprintf(file, 32, PPP_CFG_FILE, serial_no);
	f=fopen(file, "rb");
	if (!f)
	{
		ppp_set_defaults(serial_no, cfg);
	}
	else
	{
		fread(cfg, sizeof(ppp_config), 1, f);
		fclose(f);
	}
	return 0;
}

int ppp_has_config(int serial_no)
{
	FILE *f;
	char file[32];

	snprintf(file, 32, PPP_CFG_FILE, serial_no);
	f=fopen(file, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	return 0;
}

int ppp_set_config(int serial_no, ppp_config *cfg)
{
	FILE *f;
	char file[32];

	snprintf(file, 32, PPP_CFG_FILE, serial_no);
	f=fopen(file, "wb");
	if (!f) return -1;

	snprintf(cfg->osdevice, 16, "ttyS%d", serial_no);

	fwrite(cfg, sizeof(ppp_config), 1, f);
	fclose(f);
	return notify_systtyd();
}

void ppp_set_defaults(int serial_no, ppp_config *cfg)
{
	/* Clean memory! */
	memset(cfg, 0, sizeof(ppp_config));

	snprintf(cfg->osdevice, 16, "ttyS%d", serial_no);

	cfg->unit=serial_no;
	cfg->speed=9600;
	cfg->echo_failure=3;
	cfg->echo_interval=5;
	cfg->novj=1;
	cfg->ip_unnumbered=-1;
	cfg->backup=-1;
}

/*
ABORT "NO CARRIER"
ABORT "NO DIALTONE"
ABORT "ERROR"
ABORT "NO ANSWER"
ABORT "BUSY"
ABORT "Username/Password Incorrect"
""    ATZ
*/
int ppp_add_chat(char *chat_name, char *chat_str)
{
	FILE *f;
	char file[50];

	sprintf(file, PPP_CHAT_FILE, chat_name);
	f=fopen(file, "wt");
	if (!f) return -1;
	fputs(chat_str, f);
	fclose(f);
	return 0;	
}

int ppp_del_chat(char *chat_name)
{
	char file[50];

	sprintf(file, PPP_CHAT_FILE, chat_name);
	return unlink(file); /* -1 on error */
}

char *ppp_get_chat(char *chat_name)
{
	FILE *f;
	char file[50];
	long len;
	char *chat_str;

	sprintf(file, PPP_CHAT_FILE, chat_name);
	f=fopen(file, "rt");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	len=ftell(f)+1;
	fseek(f, 0, SEEK_SET);
	if (len == 0)
	{
		fclose(f);
		return NULL;
	}
	chat_str=malloc(len+1);
	if (chat_str == 0)
	{
		fclose(f);
		return NULL;
	}
	fgets(chat_str, len, f);
	fclose(f);
	return chat_str;
}

int ppp_chat_exists(char *chat_name)
{
	FILE *f;
	char file[50];
	
	sprintf(file, PPP_CHAT_FILE, chat_name);
	
	f = fopen(file, "rt");
	if (!f) return 0;
	fclose(f);
	return 1;
}

int ppp_get_state(int serial_no)
{
	FILE *f;
	char file[50];

	if (serial_no < MAX_WAN_INTF) sprintf(file, PPP_UP_FILE_SERIAL, serial_no);
		else sprintf(file, PPP_UP_FILE_AUX, serial_no-MAX_WAN_INTF);
	f = fopen(file, "rt");
	if (!f) return 0;
	fclose(f);
	return 1;
}

int ppp_is_pppd_running(int serial_no)
{
	FILE *f;
	char file[50];
	char buf[32];
	pid_t pid;

	if (serial_no < MAX_WAN_INTF) sprintf(file, PPP_PPPD_UP_FILE_SERIAL, serial_no);
		else sprintf(file, PPP_PPPD_UP_FILE_AUX, serial_no-MAX_WAN_INTF);

	f=fopen(file, "rt");
	if (!f) return 0;
	fread(buf, 32, 1, f);
	fclose(f);
	buf[31]=0;
	pid=atoi(buf);
	if ((pid > 0) && (pid_exists(pid))) return pid;
	return 0;
}

/* Retorna o device serialX correspondente a serial.
   Ex.: serial_no = 0 -> "serial0" 
 */
char *ppp_get_pppdevice(int serial_no)
{
	FILE *f;
	char file[50];
	static char pppdevice[32];
	int len;
	
	pppdevice[0] = 0;
	if (serial_no < MAX_WAN_INTF) sprintf(file, PPP_UP_FILE_SERIAL, serial_no);
		else sprintf(file, PPP_UP_FILE_AUX, serial_no-MAX_WAN_INTF);
	
	f = fopen(file, "rt");
	if (!f) return NULL;
	fgets(pppdevice, 32, f); pppdevice[31]=0;
	len = strlen(pppdevice);
	if (pppdevice[len-1]=='\n') pppdevice[len-1]=0;
	fclose(f);
	return pppdevice;
}

#define MKARG(s)  { arglist[n] = (char *)malloc(strlen(s)+1); strcpy(arglist[n], s); n++; }
#define MKARGI(i) { arglist[n] = (char *)malloc(16); sprintf(arglist[n], "%d", i); n++; }
void ppp_pppd_arglist(char **arglist, ppp_config *cfg, int server)
{
	int n=0;
	char filename[100];
	char addr[32], mask[32]; /* strings utilizadas para receber o ip da ethernet quando IP UNNUMBERED ativo */

#if 0
	MKARG("/bin/nice");
	MKARG("-n");
	MKARG("-20");
#endif

	MKARG("/bin/pppd");
	//snprintf(filename, 99, "/etc/ppp/peers/%s", tty->ppp.osdevice); filename[99]=0;

	MKARG(cfg->osdevice); /* tts/aux[0|1] tts/wan[0|1] */

	if (cfg->sync_nasync)
	{
		MKARG("sync");
	}
	else
	{
		if (cfg->speed) MKARGI(cfg->speed);

		if (cfg->flow_control == FLOW_CONTROL_RTSCTS) {
			MKARG("crtscts");
		} else {
			MKARG("nocrtscts");
			if (cfg->flow_control == FLOW_CONTROL_XONXOFF)
				MKARG("xonxoff");
		}

		if (server)
		{
#if 1
			MKARG("proxyarp"); /* !!! Cannot determine ethernet address for proxy ARP */
#endif
		}
		else
		{
			if (cfg->chat_script[0])
			{
				char buf[100];

				snprintf(filename, 99, PPP_CHAT_FILE, cfg->chat_script); filename[99]=0;
				MKARG("connect");
#if 1
				snprintf(buf, 99, "chat -v -f %s", filename); buf[99]=0; /* -t 90 */
#else
				snprintf(buf, 99, "chat -f %s", filename); buf[99]=0; /* -t 90 */
#endif
				MKARG(buf);
				if (cfg->dial_on_demand)
				{
					MKARG("demand");
					MKARG("ktune");
					MKARG("ipcp-accept-remote");
					MKARG("ipcp-accept-local");
				}
				if (cfg->idle)
				{
					MKARG("idle");
					MKARGI(cfg->idle);
				}
				if (cfg->holdoff)
				{
					MKARG("holdoff");
					MKARGI(cfg->holdoff);
				}
			}
		}
	}

	MKARG("lock");

	MKARG("asyncmap");
	MKARG("0");

	MKARG("unit");
	MKARGI(cfg->unit); /* Undocumented! "serial" index */

	MKARG("nodetach");

	if (!server) MKARG("persist");

	if (cfg->mtu) { 
		MKARG("mtu"); MKARGI(cfg->mtu);
		MKARG("mru"); MKARGI(cfg->mtu); /* !!! same as mtu */
	}
	
	if (cfg->ip_unnumbered != -1) { /* Teste para verificar se IP UNNUMBERED esta ativo */
		char ethernetdev[16];
		sprintf(ethernetdev, "ethernet%d", cfg->ip_unnumbered);
		get_ethernet_ip_addr(ethernetdev, addr, mask); /* Captura o endere�o e mascara da interface Ethernet */
		strncpy(cfg->ip_addr, addr, 16); /* Atualiza cfg com os dados da ethernet */
		cfg->ip_addr[15]=0;
		strncpy(cfg->ip_mask, mask, 16);
		cfg->ip_mask[15]=0;
	}

	if (server) {
		if (cfg->server_ip_addr[0] || cfg->server_ip_peer_addr[0]) 
		{
			char buf[40];
			snprintf(buf, 39, "%s:%s", cfg->server_ip_addr, cfg->server_ip_peer_addr); buf[39]=0;
			MKARG(buf);
		}
	/* Configures local and/or remote IP address */
	} else if (cfg->ip_addr[0] || cfg->ip_peer_addr[0]) {
		char buf[40];
		snprintf(buf, 39, "%s:%s", cfg->ip_addr, cfg->ip_peer_addr); buf[39]=0;
		MKARG(buf);
	} /* !!! teste para usar �noip� quando os parametros ip nao estiverem configurados �noipdefault� */

	if (cfg->server_ip_mask[0]) { MKARG("netmask"); MKARG(cfg->server_ip_mask); }

	if (cfg->server_auth_user[0]) { MKARG("user"); MKARG(cfg->server_auth_user); }
	
	if (cfg->default_route) { MKARG("defaultroute"); } /* only work if system has not a default route allready */
	else { MKARG("nodefaultroute"); }
	
	if( cfg->auth_user[0] && ((cfg->server_flags & SERVER_FLAGS_PAP) || (cfg->server_flags & SERVER_FLAGS_CHAP)) ) {
		MKARG("auth");
		if (cfg->server_flags & SERVER_FLAGS_PAP) {
			MKARG("require-pap");
		}
		else if (cfg->server_flags & SERVER_FLAGS_CHAP) {
			MKARG("require-chap");
		}
	} else MKARG("noauth"); /* Will not ask for the peer to authenticate itself */

	
	/* pppd will use lines in the secrets files which have name as 
	* the second field when looking for a secret to use in authenticating the peer */
	if (cfg->server_auth_user[0]) { MKARG("name"); MKARG(cfg->server_auth_user); }
	else { MKARG("name"); MKARG(cfg->cishdevice); }

	if (cfg->usepeerdns) 
		MKARG("usepeerdns"); /* recebe servidores DNS do peer (Problemas com Juniper!) */

	if (cfg->novj) MKARG("novj");
	if (cfg->echo_interval)	{
		MKARG("lcp-echo-interval");
		MKARGI(cfg->echo_interval);
	}
	if (cfg->echo_failure) {
		MKARG("lcp-echo-failure");
		MKARGI(cfg->echo_failure);
	}

	MKARG("nodeflate");
	MKARG("nobsdcomp");

	if (cfg->multilink)
		MKARG("multilink");

#ifdef CONFIG_HDLC_SPPP_LFI
	{
		char conv[(CONFIG_MAX_LFI_PRIORITY_MARKS * 11)];

		/* Fragmentation */
		if( cfg->fragment_size != 0 ) {
			sprintf(conv, "%d", cfg->fragment_size);
			MKARG("lfifragsize");
			MKARG(conv);
		}
		/* Interleaving */
		if( cfg->priomarks[0] != 0 ) {
			int k;
			char tmp[16];

			conv[0] = 0;
			for(k=0; (k < CONFIG_MAX_LFI_PRIORITY_MARKS) && (cfg->priomarks[k] != 0); k++) {
				sprintf(tmp, "%d:", cfg->priomarks[k]);
				strcat(conv, tmp);
			}
			conv[strlen(conv)-1] = 0;
			MKARG("lfimarks");
			MKARG(conv);
		}
	}
#endif

	if (cfg->ipx_enabled)
	{
		char ipx_network[10], ipx_node[14];
		snprintf(ipx_network, 10, "%08lX", (unsigned long int) cfg->ipx_network);
		ipx_network[8]=0;
		sprintf(ipx_node, "%02x%02x%02x%02x%02x%02x", 
			cfg->ipx_node[0], cfg->ipx_node[1], cfg->ipx_node[2], 
			cfg->ipx_node[3], cfg->ipx_node[4], cfg->ipx_node[5]);
		MKARG("ipx");
		MKARG("ipx-network");
		MKARG(ipx_network);
		MKARG("ipx-node");
		MKARG(ipx_node);
		MKARG("ipxcp-accept-remote");
		/* When ip address is not configured and ipx is configured, 
		* disable ipcp negotiation, otherwire pppd complains that there is no ip address */
		if (!cfg->ip_addr[0]) MKARG("noip");
		#if 0
		MKARG(ipx-routing);
		MKARGI(2); /* RIP/SAP */
		#endif
	}

	#if 1 // pppd faz a selecao do que apresentar em log!
	if (cfg->debug)
		MKARG("debug");
	#endif

	#if 0 // debug
	{
		int i;

		syslog(LOG_ERR, "pppd line");
		for (i=0; i < n; i++)
			syslog(LOG_INFO, ">> %s", arglist[i]);
	}
	#endif
	arglist[n]=NULL;
}

#ifdef OPTION_IPSEC
int l2tp_ppp_get_config(char *name, ppp_config *cfg)
{
	FILE *f;
	char file[50];

	sprintf(file, L2TP_PPP_CFG_FILE, name);
	f=fopen(file, "rb");
	if (!f)
	{
		l2tp_ppp_set_defaults(name, cfg);
	}
	else
	{
		fread(cfg, sizeof(ppp_config), 1, f);
		fclose(f);
	}
	return 0;
}

int l2tp_ppp_has_config(char *name)
{
	FILE *f;
	char file[50];

	sprintf(file, L2TP_PPP_CFG_FILE, name);
	f=fopen(file, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	return 0;
}

int l2tp_ppp_set_config(char *name, ppp_config *cfg)
{
	FILE *f;
	char file[50];

	sprintf(file, L2TP_PPP_CFG_FILE, name);
	f=fopen(file, "wb");
	if (!f) return -1;
	fwrite(cfg, sizeof(ppp_config), 1, f);
	fclose(f);
	set_l2tp(RESTART);
	return 0;
}

void l2tp_ppp_set_defaults(char *name, ppp_config *cfg)
{
	memset(cfg, 0, sizeof(ppp_config));
	cfg->echo_failure=5;
	cfg->echo_interval=30;
	cfg->ip_unnumbered=0; /* default to ethernet0 */
}
#endif

