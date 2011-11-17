/*
 * defines.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef DEFINES_H_
#define DEFINES_H_

#define M3G_PPP_END 19
#define PPTP_PPP_START 20
#define PPTP_PPP_END 29
#define PPPOE_PPP_START 30
#define PPPOE_PPP_END 39

#define ETHERNETDEV "eth"
#define AUXDEV "aux"
#define AUXDEV_PPP "ax"
#define SERIALDEV "serial"
#define SERIALDEV_PPP "sx"
#define BRIDGE_NAME "bridge"

#define MAX_BRIDGE  16 /* Se alterar este valor alterar tambem as strings "1-1" nos comandos relacionados a bridge */
/* Teste do modelo... */
#define MAX_WAN_INTF 2 /* Numero de interfaces WAN (UCC HDLC Fast Mode)  */
#define MAX_AUX_INTF 2 /* Numero de interfaces AUX (UART) */
#define MAX_LAN_INTF 2 /* Numero de interfaces LAN (UCC Fast Ethernet Mode) */
#define L2TP_INTF 40

#define MAX_NUM_IPS 100
#define MAX_NUM_LINKS 100
#define MAX_HWADDR 6

#define	TTS_AUX0	"/dev/tts/aux0"
#define	TTS_AUX1	"/dev/tts/aux1"

#define TMP_CFG_FILE			"/var/run/tmp-cfg"
#define STARTUP_CFG_FILE		"/var/run/startup-cfg"
#define DEFAULT_CFG_FILE		"/etc/default-config"
#define TFTP_CFG_FILE			"/var/run/tmp-tftp-cfg"
#define TMP_TFTP_OUTPUT_FILE		"/var/run/tmp-tftp-output"
#define FILE_TMP_QOS_DOWN		"/var/run/tmp.qos.down"

/* QoS Files */
#define FILE_QOS_SCRIPT			"/var/run/qos.%s.up"
#define FILE_QOS_CFG			"/var/run/qos.%s.cfg"
#define FILE_FRTS_CFG			"/var/run/frts.%s.cfg"

/* PAM Files */
#define FILE_PAM_GENERIC		"/etc/pam.d/generic_daemon"
#define FILE_PAM_SPPP			"/etc/pam.d/spppd"
#define FILE_PAM_WEB			"/etc/pam.d/web"
#define FILE_PAM_PPP			"/etc/pam.d/ppp"
#define FILE_PAM_CLI			"/etc/pam.d/cli"
#define FILE_PAM_LOGIN			"/etc/pam.d/login"


#define FT_TMP_CFG_FILE			"/var/run/ft-tmp-cfg"
#define FILE_PHY_0_CFG			"/var/run/phy_0_cfg"
#define FILE_PHY_1_CFG			"/var/run/phy_1_cfg"
#define	FILE_DEV_KERNEL_QOS_DUMP 	"/var/run/dev_kernel_qos_dump"
#define	FILE_TMP_IPT_ERRORS		"/var/run/ipterr_XXXXXX"
#define	FILE_TMP_IPT_ERRORS_LEN		32
#define FILE_PAM_ENABLE			"/etc/pam.d/enable"
#define	FILE_ARG_LIST			"/var/run/file_arg_list"
#define FILE_SSH_KNOWN_HOSTS		"/var/run/.ssh/known_hosts"

/* CRYPTO defines */
#define IPSEC_OPTION	/* enable IPSEC */
#define CMDS_BEF_LIST	1 /* !!!!!!! */

#define IPSEC_MAX_CONN			50
#define IPSEC_MAX_CONN_NAME		100
#define FILE_IPSEC_SECRETS		"/etc/ipsec.secrets"
#define FILE_CONN_SECRETS		"/etc/ipsec.%s.secrets"
#define STRING_IPSEC_AUTORELOAD 	"auto_reload="
#define STRING_IPSEC_NAT		"nat_traversal="
#define STRING_IPSEC_OMTU		"overridemtu="
#define FILE_IPSEC_CONF			"/etc/ipsec.conf"
#define MAX_KEY_SIZE			2048
#define MAX_CMD_LINE			500+MAX_KEY_SIZE // tamanho maximo da linha de comando
#define FILE_IKE_CONN_CONF		"/etc/ipsec.%s.conf"
#define FILE_MAN_CONN_CONF		"/etc/manual.%s.conf" /* !!! */
#define FILE_STARTER_PID		"/var/run/starter.pid"
#define FILE_PLUTO_PID			"/var/run/pluto/pluto.pid"
#define FILE_L2TPD_SOCKET		"/var/run/l2tpctrl"
#define FILE_L2TPD_PID			"/var/run/l2tpd.pid"
#define PROG_STARTER			"/lib/ipsec/starter"
#define PROG_L2TPD			"/bin/l2tpd"
#define MAX_LINE			100
#define STRING_ANY			"%any"
#define STRING_DEFAULTROUTE		"%defaultroute"
#define STRING_IPSEC_LINK		"#active="
#define STRING_IPSEC_AUTO		"auto="
#define STRING_IPSEC_IFACE		"interfaces="
#define STRING_IPSEC_AUTHBY		"authby="
#define STRING_IPSEC_AUTHPROTO		"auth="
#define STRING_IPSEC_PFS		"pfs="
#define STRING_IPSEC_L_ID		"leftid="
#define STRING_IPSEC_L_ADDR		"left="
#define STRING_IPSEC_L_SUBNET		"leftsubnet="
#define STRING_IPSEC_L_NEXTHOP		"leftnexthop="
#define STRING_IPSEC_L_RSAKEY		"leftrsasigkey="
#define STRING_IPSEC_L_PROTOPORT	"leftprotoport="
#define STRING_IPSEC_R_ID		"rightid="
#define STRING_IPSEC_R_ADDR		"right="
#define STRING_IPSEC_R_SUBNET		"rightsubnet="
#define STRING_IPSEC_R_NEXTHOP		"rightnexthop="
#define STRING_IPSEC_R_RSAKEY		"rightrsasigkey="
#define STRING_IPSEC_R_PROTOPORT	"rightprotoport="
#define STRING_IPSEC_SPI		"spi="
#define STRING_IPSEC_AH			"ah="
#define STRING_IPSEC_AHKEY		"ahkey="
#define STRING_IPSEC_AH_MD5		"hmac-md5-96"
#define STRING_IPSEC_AH_SHA1		"hmac-sha1-96"
#define STRING_IPSEC_ESP		"esp="
#define STRING_IPSEC_AGGRMODE		"aggrmode="
#define FILE_TMP_RSAKEYS		"/var/run/rsakeys_tmp"
#define FILE_SAVED_RSAKEYS		"/var/run/rsakeys"
#define MAX_ID_LEN			100
#define MAX_ADDR_SIZE			200
#define PROG_PLUTO			"pluto"
#define PROC_MPC180			"/proc/mpc180"

//#define SNMP_THIN_CONF
#ifdef SNMP_THIN_CONF
#define FILE_SNMPD_CONF                 "/etc/snmp/snmpd.conf"
#define FILE_SNMPD_DATA_CONF    	FILE_SNMPD_CONF
#define FILE_SNMPD_STORE_CONF   	FILE_SNMPD_CONF
#else
#define FILE_SNMPD_CONF                 "/etc/snmp/snmpd.conf"
#define FILE_SNMPD_DATA_CONF    	"/etc/snmpdata/snmp/snmpd.conf"
#define FILE_SNMPD_STORE_CONF   	"/etc/snmpstore/snmpd.conf"
#endif

#define SNMP_USERKEY_FILE               "/var/run/snmp.userkeys"
#define PROG_SNMPD			"snmpd"
#define TRAPCONF			"/etc/trap.cfg"
#define RMON_DAEMON			"rmond"
#define MIB_FILES_LOAD_STATS		"/var/run/mibs_loaded_stats"
#define MIB_FILES_PARTLOAD_STATS	"/var/run/mibs_partloaded_stats"

#define PROG_SYSLOGD			"syslogd"
#define FILE_SYSLOGD_PID		"/var/run/syslogd.pid"

#define TRIAL_DAEMON			"triald"
#define SNMP_STATUS_FILE		"/var/run/snmp_status"
#define MOTHERBOARDINFO_TMP		"/var/run/motherinfo_tmp"
#define MOTHERBOARD_INFO_FILE		"/var/run/motherboard_info"
#define VAR_RUN_FILE_TMP		"/var/run/temp_XXXXXX"

/* New QoS */
#define POLICY_CONFIG_FILE "/var/run/%s.policy"
#define POLICY_DESC_FILE "/var/run/%s.poldesc"
#define SERVICE_POLICY_CONFIG_FILE "/var/run/%s.service_policy"

#define	SATR_SN_LEN				10
#define	MODEM_SN_LEN			10

#define PROG_MICROCOM			"microcom"
#define	FILE_MICROCOM_CONF		"/etc/microcom.conf"
enum {
	MICROCOM_MODE_NONE=0,
	MICROCOM_MODE_LOCAL,
	MICROCOM_MODE_MODEM,
	MICROCOM_MODE_LISTEN
};
#define	FILE_VCLI_PID			"/var/run/vcli.pid"

#define SPPPD_DAEMON			"spppd"
#define FILE_SPPPD_PID			"/var/run/spppd.pid"
#define	FILE_SPPP_CHAP_SECRETS		"/etc/sppp/chap-secrets"
#define	FILE_SPPP_CHAP_AUTH_SECRETS	"/etc/sppp/chap-auth-secrets"
#define	FILE_SPPP_PAP_SECRETS		"/etc/sppp/pap-secrets"

#define	FILE_NTP_CONF			"/etc/ntp.conf"
#define	FILE_NTPD_KEYS			"/etc/ntp.keys"

#define	FILE_RMON_LOG_EVENTS_MAX_SIZE	(32 * 1024)
#define	FILE_RMON_LOG_EVENTS	"/var/log/rmon_log_events"
#define	RMON_SEM_KEY		1224
#define	RMON_SHM_KEY		1225
#define	MIBTREE_SHM_KEY		1226
#define	SNMP_TREE_SEM_KEY	1227
#define	IPADDR_SHM_KEY		1228
#define	RMON_DAEMON_SEM_KEY	1229
#define	DELETE_MOD_SEM_KEY	1230
#define	MICROCOM_KEY		1231

#define PROG_MIBLOADER			"mibloader"

#define	BOARD_HW_ID_0	0x00	/* DM991CR com 2 portas - 2 portas ETH (2 PHY Kendin), soh para a placa pequena */
#define	BOARD_HW_ID_1	0x01	/* DM706CR com 1 porta  - 1 porta ETH (1 PHYKendin), vale para a placa grande */
#define	BOARD_HW_ID_2	0x02	/* DM991CR com 2 portas - 1 PHY Kendin + 1 portado switch, vale para a placa grande */
#define	BOARD_HW_ID_3	0x03	/* DM706CS com 5 portas - 1 PHY Kendin + 4 portas do switch, vale para a placa grande */
#define	BOARD_HW_ID_4	0x04	/* DM991CS com 5 portas - 1 PHY Kendin + 4 portas do switch, vale para a placa grande */

enum {
	LOCAL=1,
	REMOTE
};

enum {
	CONN_INCOMPLETE=1,
	CONN_UP,
	CONN_DOWN,
	CONN_SHUTDOWN,
	CONN_WAIT
};

enum {
	AUTO_ADD=1,
	AUTO_START,
	AUTO_IGNORE
};

enum {
	MBINFO_VENDOR=1,
	MBINFO_PRODUCTNAME,
	MBINFO_COMPLETEDESCR,
	MBINFO_PRODUCTCODE,
	MBINFO_FWVERSION,
	MBINFO_SN,
	MBINFO_RELEASEDATE,
	MBINFO_RESETS
};

enum {
	MODEM_MGNT_TELEBRAS=1,
	MODEM_MGNT_TRANSPARENT
};

#endif /* DEFINES_H_ */
