/*
 * pam.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef PAM_H_
#define PAM_H_

#define FILE_PASSWD "/etc/passwd"
#define FILE_RADDB_SERVER "/etc/raddb/server"
#define FILE_TACDB_SERVER "/etc/tacdb/server"

#define	AUTH_NOK		0
#define	AUTH_OK			1
#define	AUTH_FACTORY		2

#define AUTH_MAX_SERVERS	3
#define MAX_AUTH_SERVER_KEY	256

//#define DEBUGS_AAA
#ifdef DEBUGS_AAA
#define dbgS_aaa(x,...) \
		syslog(LOG_INFO,  "%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define dbgS_aaa(x,...)
#endif

//#define DEBUGP_AAA
#ifdef DEBUGP_AAA
#define dbgP_aaa(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define dbgP_aaa(x,...)
#endif




enum {
	AUTH_TYPE_NONE = 0, AUTH_TYPE_PAP, AUTH_TYPE_CHAP
};

enum aaa_modes {
	/* Authentication */
	AAA_AUTH_NONE = 2,
	AAA_AUTH_LOCAL,
	AAA_AUTH_RADIUS,
	AAA_AUTH_RADIUS_LOCAL,
	AAA_AUTH_TACACS,
	AAA_AUTH_TACACS_LOCAL,
	/* Authorization */
	AAA_AUTHOR_NONE,
	AAA_AUTHOR_TACACS,
	AAA_AUTHOR_TACACS_LOCAL,
	AAA_AUTHOR_RADIUS,
	AAA_AUTHOR_RADIUS_LOCAL,
	/* Accounting */
	AAA_ACCT_NONE,
	AAA_ACCT_TACACS,
	AAA_ACCT_RADIUS,
};

enum aaa_families {
	AAA_AUTH = 1, AAA_AUTHOR, AAA_ACCT
};

typedef struct {
	enum aaa_families family;
	enum aaa_modes mode;
} aaa_config_t;

struct auth_server {
	char *ipaddr;
	char *key;
	int timeout;
};

/* Prototypes */
int librouter_pam_enable_authenticate(void);
int librouter_pam_web_authenticate(char *user, char *pass);
int librouter_pam_authorize_command(char *cmd);
int librouter_pam_account_command(char *cmd);

int librouter_pam_get_current_mode(char *file_name);
int librouter_pam_get_current_author_mode(char *file_name);
int librouter_pam_get_current_acct_mode(char *file_name);
int librouter_pam_get_current_acct_cmd_mode(char *file_name);
int librouter_pam_config_mode(int mode, char *pam_file);
int librouter_pam_get_auth_type(char *device);
int librouter_pam_get_users(char *users);
int librouter_pam_del_user(char *user);
int librouter_pam_add_user(char *user, char *pw);
int librouter_pam_add_user_with_hash(char *user, char *pw);
int librouter_pam_add_user_to_group (char *user, char *group);
int librouter_pam_del_user_from_group (char *user, char *group);
int librouter_pam_get_privilege_by_name (char * group_priv);
int librouter_pam_get_privilege (void);
int librouter_pam_cmds_add_user_to_group(char * name, char * priv);
int librouter_pam_cmds_del_user_from_group (char * name);



int librouter_pam_add_tacacs_server(struct auth_server *server);
int librouter_pam_del_tacacs_server(struct auth_server *server);
int librouter_pam_get_tacacs_servers(struct auth_server *server);

int librouter_pam_add_radius_server(struct auth_server *server);
int librouter_pam_del_radius_server(struct auth_server *server);
int librouter_pam_get_radius_servers(struct auth_server *server);

void librouter_pam_free_servers(int num_servers, struct auth_server *server);

#endif /* PAM_H_ */
