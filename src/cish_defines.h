
#define MAX_SERVERS 3
#define MAX_SERVER_AUTH_KEY 256

typedef struct
{
	char ip_addr[16];
	char authkey[MAX_SERVER_AUTH_KEY];
	int timeout;
} server_config;

typedef struct
{
	char login_secret[15];
	char enable_secret[15];
	int terminal_lines;
	int terminal_timeout;

	int debug[20]; /* debug persistent !!! Check debug.c */

	server_config radius[MAX_SERVERS]; /* !!! find new location! */
	server_config tacacs[MAX_SERVERS]; /* !!! find new location! */

	/* 11111,22222,33333,44444,55555,66666,77777,88888 */
	char nat_helper_ftp_ports[48]; /* !!! find new location! */
	char nat_helper_irc_ports[48]; /* !!! find new location! */
	char nat_helper_tftp_ports[48]; /* !!! find new location! */
} cish_config;

