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
	/* Accounting */
	AAA_ACCT_NONE,
	AAA_ACCT_TACACS,
	/* Command Accounting */
	AAA_ACCT_TACACS_CMD_NONE,
	AAA_ACCT_TACACS_NO_CMD_1,
	AAA_ACCT_TACACS_CMD_1,
	AAA_ACCT_TACACS_NO_CMD_15,
	AAA_ACCT_TACACS_CMD_15,
	AAA_ACCT_TACACS_CMD_ALL
};

enum aaa_families {
	AAA_AUTH = 1, AAA_AUTHOR, AAA_ACCT, AAA_ACCT_CMD
};

typedef struct {
	enum aaa_families family;
	enum aaa_modes mode;
} aaa_config_t;

/* Prototypes */
int libconfig_pam_web_authenticate(char *user, char *pass);

int libconfig_pam_get_current_mode(char *file_name);
int libconfig_pam_get_current_author_mode(char *file_name);
int libconfig_pam_get_current_acct_mode(char *file_name);
int libconfig_pam_get_current_acct_cmd_mode(char *file_name);

int libconfig_pam_config_mode(cish_config *cish_cfg,
                  int mode,
                  int change_active_mode,
                  char *pam_file);

int libconfig_pam_get_auth_type(char *device);

#endif /* PAM_H_ */
