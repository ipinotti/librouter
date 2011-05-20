/*
 * pam.c
 *
 *  Created on: Jun 24, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include "args.h"
#include "defines.h"
#include "ssh.h"
#include "typedefs.h"
#include "pam.h"
#include "ppp.h"

/* New modes must be added to this global variable */
#define MAX_AAA_TYPES 32
static const aaa_config_t
                aaa_config[MAX_AAA_TYPES] = {
                /* Authentication */
                { AAA_AUTH, AAA_AUTH_NONE }, { AAA_AUTH, AAA_AUTH_NONE }, { AAA_AUTH,
                                AAA_AUTH_LOCAL }, { AAA_AUTH, AAA_AUTH_RADIUS }, { AAA_AUTH,
                                AAA_AUTH_RADIUS_LOCAL }, { AAA_AUTH, AAA_AUTH_TACACS }, { AAA_AUTH,
                                AAA_AUTH_TACACS_LOCAL },
                /* Authorization */
                { AAA_AUTHOR, AAA_AUTHOR_NONE }, { AAA_AUTHOR, AAA_AUTHOR_TACACS }, { AAA_AUTHOR,
                                AAA_AUTHOR_TACACS_LOCAL },
                /* Accounting */
                { AAA_ACCT, AAA_ACCT_NONE }, { AAA_ACCT, AAA_ACCT_TACACS },
                /* Command Accounting */
                { AAA_ACCT_CMD, AAA_ACCT_TACACS_CMD_NONE }, { AAA_ACCT_CMD,
                                AAA_ACCT_TACACS_NO_CMD_1 },
                                { AAA_ACCT_CMD, AAA_ACCT_TACACS_CMD_1 }, { AAA_ACCT_CMD,
                                                AAA_ACCT_TACACS_NO_CMD_15 }, { AAA_ACCT_CMD,
                                                AAA_ACCT_TACACS_CMD_15 }, { AAA_ACCT_CMD,
                                                AAA_ACCT_TACACS_CMD_ALL } };

struct web_auth_data {
	char *user;
	char *pass;
};

static int _pam_null_conv(int n,
                          const struct pam_message **msg,
                          struct pam_response **resp,
                          void *data)
{
	return (PAM_CONV_ERR);
}

/**
 * web_conv Conversation function for WEB access
 *
 * This function is called by the PAM module responsible
 * for the authentication.
 *
 * @param num_msg
 * @param msgm
 * @param response
 * @param appdata_ptr
 * @return
 */
static int librouter_pam_web_conv(int num_msg,
                                  const struct pam_message **msgm,
                                  struct pam_response **response,
                                  void *appdata_ptr)
{
	int count = 0;
	struct pam_response *reply;
	struct web_auth_data *web_data = (struct web_auth_data *) appdata_ptr;

	if (num_msg <= 0)
		return PAM_CONV_ERR;

	reply = (struct pam_response *) calloc(num_msg, sizeof(struct pam_response));
	if (reply == NULL)
		return PAM_CONV_ERR;

	/* Supply module with username and password */
	for (count = 0; count < num_msg; ++count) {
		char *string = NULL;
		int nc;

		switch (msgm[count]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
			/* Password */
			string = web_data->pass;
			break;
		case PAM_PROMPT_ECHO_ON:
			/* User */
			string = web_data->user;
			break;

		default:
			syslog(LOG_ERR, "erroneous conversation (%d)\n", msgm[count]->msg_style);
		}

		if (string) {
			/* must add to reply array */
			/* add string to list of responses */

			reply[count].resp_retcode = 0;
			reply[count].resp = string;
			string = NULL;
		}
	}

	*response = reply;
	reply = NULL;

	return PAM_SUCCESS;
}

/**
 * librouter_pam_web_authenticate
 *
 * Authenticate a user via PAM methods
 *
 * @param user String with user's name
 * @param pass String with the user's password
 * @return AUTH_OK if success, AUTH_NOK if fail to authenticate
 */
int librouter_pam_web_authenticate(char *user, char *pass)
{
	int pam_err;
	int ret = AUTH_NOK;
	struct pam_conv fpam_conv;
	static pam_handle_t *pam_handle = NULL;
	static struct pam_conv null_conv = { _pam_null_conv, NULL };
	struct web_auth_data web_data;

	dbgS_aaa("Starting librouter_pam_web_authenticate\n\n");

	if (user != NULL)
		web_data.user = strdup(user);
	else
		web_data.user = NULL;

	if (pass != NULL)
		web_data.pass = strdup(pass);
	else
		web_data.pass = NULL;

	fpam_conv.conv = librouter_pam_web_conv;
	fpam_conv.appdata_ptr = &web_data;

	if ((pam_err = pam_start("web", NULL, &null_conv, &pam_handle)) != PAM_SUCCESS)
		goto web_auth_err;

	if ((pam_err = pam_set_item(pam_handle, PAM_CONV, (const void *) &fpam_conv)) != PAM_SUCCESS)
		goto web_auth_err;

	if ((pam_err = pam_authenticate(pam_handle, 0)) != PAM_SUCCESS)
		goto web_auth_err;

	/* Now check if the authenticated user is allowed to login. */
	if (pam_acct_mgmt(pam_handle, 0) == PAM_AUTHTOK_EXPIRED) {
		if (pam_chauthtok(pam_handle, 0) != PAM_SUCCESS)
			goto web_auth_err;
	}


	/*
	 *  Call 'pam_open_session' to open the authenticated session;
	 *  'pam_close_session' gets called by the process that cleans up the utmp entry (i.e., init);
	 */
	if (pam_open_session(pam_handle, 0) != PAM_SUCCESS)
		goto web_auth_err;


	/*
	 * Initialize the supplementary group access list.
	 * This should be done before pam_setcred because the PAM modules might add groups during the pam_setcred call.
	 */
	if (pam_setcred(pam_handle, PAM_ESTABLISH_CRED) != PAM_SUCCESS)
		goto web_auth_err;

	ret = AUTH_OK;

	web_auth_err: if (pam_handle != NULL)
		pam_end(pam_handle, pam_err);
	pam_handle = NULL;

	return ret;
}

/**
 * librouter_pam_web_authenticate
 *
 * Authenticate a user via PAM methods
 *
 * @param user String with user's name
 * @param pass String with the user's password
 * @return AUTH_OK if success, AUTH_NOK if fail to authenticate
 */
int librouter_pam_enable_authenticate(void)
{
	int pam_err;
	int ret = AUTH_NOK;
	struct pam_conv fpam_conv;
	static pam_handle_t *pam_handle = NULL;
	static struct pam_conv null_conv = { _pam_null_conv, NULL };

	dbgS_aaa("Starting librouter_pam_enable_authenticate\n\n");

	fpam_conv.conv = misc_conv;
	fpam_conv.appdata_ptr = NULL;

	if ((pam_err = pam_start("enable", "$enable$", &null_conv, &pam_handle)) != PAM_SUCCESS)
		goto web_auth_err;

	if ((pam_err = pam_set_item(pam_handle, PAM_CONV, (const void *) &fpam_conv)) != PAM_SUCCESS)
		goto web_auth_err;

	if ((pam_err = pam_authenticate(pam_handle, 0)) != PAM_SUCCESS)
		goto web_auth_err;

	/* Now check if the authenticated user is allowed to login. */
	if (pam_acct_mgmt(pam_handle, 0) == PAM_AUTHTOK_EXPIRED) {
		if (pam_chauthtok(pam_handle, 0) != PAM_SUCCESS)
			goto web_auth_err;
	}

	/*
	 *  Call 'pam_open_session' to open the authenticated session;
	 *  'pam_close_session' gets called by the process that cleans up the utmp entry (i.e., init);
	 */
	if (pam_open_session(pam_handle, 0) != PAM_SUCCESS)
		goto web_auth_err;

	/* 
	 * Initialize the supplementary group access list. 
	 * This should be done before pam_setcred because the PAM modules might add groups during the pam_setcred call.
	 */
	if (pam_setcred(pam_handle, PAM_ESTABLISH_CRED) != PAM_SUCCESS)
		goto web_auth_err;

	ret = AUTH_OK;

	web_auth_err: if (pam_handle != NULL)
		pam_end(pam_handle, pam_err);
	pam_handle = NULL;

	return ret;
}

#ifdef OPTION_AAA_AUTHORIZATION
/**
 *
 *
 * @param user
 * @param cmd
 * @return
 */
int librouter_pam_authorize_command(char *cmd)
{
	int pam_err;
	struct pam_conv fpam_conv;
	static pam_handle_t *pam_handle = NULL;
	static struct pam_conv null_conv = { _pam_null_conv, NULL };
	struct passwd *pw;

	pw = getpwuid(getuid());

	if (pw == NULL)
		return -1;

	if ((pam_err = pam_start("login", pw->pw_name, &null_conv, &pam_handle)) != PAM_SUCCESS)
		return -1;

	pam_set_item(pam_handle, PAM_USER_PROMPT, (const void *) cmd);

	if (pam_acct_mgmt(pam_handle, 0) != PAM_SUCCESS) {
		fprintf(stderr, "User %s not authorized to run this command\n");
		return -1;
	}

	return 0;
}
#endif /* OPTION_AAA_AUTHORIZATION */

#ifdef OPTION_AAA_ACCOUNTING
/**
 *
 *
 * @param user
 * @param cmd
 * @return
 */
int librouter_pam_account_command(char *cmd)
{
	int pam_err;
	struct pam_conv fpam_conv;
	static pam_handle_t *pam_handle = NULL;
	static struct pam_conv null_conv = { _pam_null_conv, NULL };
	struct passwd *pw = getpwuid(getuid());

	if (pw == NULL)
		return -1;

	if ((pam_err = pam_start("login", pw->pw_name, &null_conv, &pam_handle)) != PAM_SUCCESS)
		return -1;

	pam_set_item(pam_handle, PAM_USER_PROMPT, (const void *) cmd);

	if (pam_open_session(pam_handle, 0) != PAM_SUCCESS)
		return -1;

	return 0;
}
#endif /* OPTION_AAA_ACCOUNTING */


/**
 * Discover mode in file
 *
 * Read file and discover which Authentication mode is currently active
 *
 * @param file_name File to be checked
 * @return authentication mode currently configured
 */
int librouter_pam_get_current_mode(char *file_name)
{
	/* authentication */
	FILE *f;
	char buf[256];
	int mode = 0;

	buf[255] = 0;

	/* Open pam configuration file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		printf("%%Error : Could not open %s\n", file_name);
		return 0;
	}

	/* Discover which authentication method is on */
	while (fgets(buf, sizeof(buf) - 1, f)) {
		if (strstr(buf, "auth_current_mode=")) {

			if (strstr(buf, "AAA_AUTH_NONE"))
				mode = AAA_AUTH_NONE;

			if (strstr(buf, "AAA_AUTH_LOCAL"))
				mode = AAA_AUTH_LOCAL;

			if (strstr(buf, "AAA_AUTH_RADIUS"))
				mode = AAA_AUTH_RADIUS;

			if (strstr(buf, "AAA_AUTH_RADIUS_LOCAL"))
				mode = AAA_AUTH_RADIUS_LOCAL;

			if (strstr(buf, "AAA_AUTH_TACACS"))
				mode = AAA_AUTH_TACACS;

			if (strstr(buf, "AAA_AUTH_TACACS_LOCAL"))
				mode = AAA_AUTH_TACACS_LOCAL;

			if (mode != 0)
				break;
		}
	}

	fclose(f);
	return mode;
}

/**
 * Discover mode in file
 * Read file and discover which authorization mode is currently active
 *
 * @param file_name File to be checked
 * @return authorization mode currently configured
 */
int librouter_pam_get_current_author_mode(char *file_name)
{
	/* authorization */
	FILE *f;
	char buf[256];
	int mode = 0;

	buf[255] = 0;

	/* Open pam configuration file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		printf("%%Error : Could not open %s\n", file_name);
		return 0;
	}

	/* Discover which authorization method is on */
	while (fgets(buf, sizeof(buf) - 1, f)) {
		if (strstr(buf, "author_current_mode=")) {

			if (strstr(buf, "AAA_AUTHOR_NONE"))
				mode = AAA_AUTHOR_NONE;

			if (strstr(buf, "AAA_AUTHOR_TACACS"))
				mode = AAA_AUTHOR_TACACS;

			if (strstr(buf, "AAA_AUTHOR_TACACS_LOCAL"))
				mode = AAA_AUTHOR_TACACS_LOCAL;

			if (mode != 0)
				break;
		}
	}

	fclose(f);
	return mode;
}

/**
 * Discover mode in file
 * Read file and discover which accouting mode is currently active
 *
 * @param file_name File to be checked
 * @return accouting mode currently configured
 */
int librouter_pam_get_current_acct_mode(char *file_name)
{
	/* exec accounting */
	FILE *f;
	char buf[256];
	int mode = 0;

	buf[255] = 0;

	/* Open pam configuration file */
	f = fopen(file_name, "r");
	if (f == NULL) {
		printf("%%Error : Could not open %s\n", file_name);
		return 0;
	}

	/* Discover which accouting method is on */
	while (fgets(buf, sizeof(buf) - 1, f)) {
		if (strstr(buf, "acct_current_mode=")) {

			if (strstr(buf, "AAA_ACCT_NONE"))
				mode = AAA_ACCT_NONE;

			if (strstr(buf, "AAA_ACCT_TACACS"))
				mode = AAA_ACCT_TACACS;

			if (mode != 0)
				break;
		}
	}

	fclose(f);
	return mode;
}

/**
 * Discover mode in file
 * Read file and discover which command accounting mode is currently active
 *
 * @param file_name : File to be checked
 * @return : command accounting mode currently configured
 */
int librouter_pam_get_current_acct_cmd_mode(char *file_name)
{
	/* command accounting */
	FILE *f;
	char buf[256];
	int mode = 0;

	buf[255] = 0;

	if (!(f = fopen(file_name, "r")))
		return 0;

	while (fgets(buf, sizeof(buf) - 1, f)) {

		if (strstr(buf, "acct_current_command_mode=")) {

			if (strstr(buf, "AAA_ACCT_TACACS_CMD_NONE"))
				mode = AAA_ACCT_TACACS_CMD_NONE;

			if (strstr(buf, "AAA_ACCT_TACACS_CMD_1"))
				mode = AAA_ACCT_TACACS_CMD_1;

			if (strstr(buf, "AAA_ACCT_TACACS_CMD_15"))
				mode = AAA_ACCT_TACACS_CMD_15;

			if (strstr(buf, "AAA_ACCT_TACACS_CMD_ALL"))
				mode = AAA_ACCT_TACACS_CMD_ALL;

			if (mode != 0)
				break;
		}
	}

	fclose(f);
	return mode;
}

/**
 * Decode an AAA mode
 * Decodes several possible modes to the
 * 3 basic families : Authentication, Authorization or Accounting.
 *
 * @param mode configured mode
 * @return family
 */
static int _librouter_pam_get_aaa_family(int mode)
{
	int i;

	for (i = 0; i < (MAX_AAA_TYPES - 1); i++) {
		if (mode == aaa_config[i].mode)
			return aaa_config[i].family;
	}

	return 0;
}

/**
 * Comment a whole section in a PAM configuration file
 *
 * @param src Source buffer to be read from
 * @param dest Destination buffer where modifications are meant to be put
 * @param f File descripton of the file
 * @return 0
 */
static int _librouter_pam_comment_section(char *src, char *dest, FILE *f)
{
	while (fgets(src, 511, f)) {

		if (!strcmp(src, "\n")) {
			strcat(dest, src);
			/* leaves when SECTION is over */
			break;
		}

		if (src[0] != '#')
			strcat(dest, "#"); /* disable line! */

		strcat(dest, src);
	}
	return 0;
}

/**
 * Check if line is to be uncommented
 *
 * @param buf A line of a PAM configuration file
 * @param mode Current PAM mode
 * @return 1 if line is to be commented, 0 otherwise.
 */
static int _librouter_pam_uncomment_line(char *buf, int mode)
{
	if (mode == AAA_AUTH_NONE && strstr(buf, "auth_none"))
		return 1;

	if (mode == AAA_AUTH_LOCAL && strstr(buf, "auth_local"))
		return 1;
	else if (mode == AAA_AUTH_TACACS && strstr(buf, "auth_tacacs_only"))
		return 1;
	else if (mode == AAA_AUTH_TACACS_LOCAL && strstr(buf, "auth_tacacs_local"))
		return 1;
	else if (mode == AAA_AUTH_RADIUS && strstr(buf, "auth_radius_only"))
		return 1;
	else if (mode == AAA_AUTH_RADIUS_LOCAL && strstr(buf, "auth_radius_local"))
		return 1;
	else if (mode == AAA_AUTHOR_NONE && strstr(buf, "author_none"))
		return 1;
	else if (mode == AAA_AUTHOR_TACACS && strstr(buf, "author_tacacs_only"))
		return 1;
	else if (mode == AAA_AUTHOR_TACACS_LOCAL && strstr(buf, "author_tacacs_local"))
		return 1;
	else if (mode == AAA_ACCT_NONE && strstr(buf, "acct_none"))
		return 1;
	else if (mode == AAA_ACCT_TACACS && strstr(buf, "acct_tacacs"))
		return 1;

	return 0;
}

/**
 * Disable a mode
 * @param mode Mode to be disabled
 * @param pam_file File name to be read/written to
 * @return 1 if success, 0 otherwise
 */
static int _librouter_pam_disable_mode(int mode, char *pam_file)
{
	int fd;
	FILE *f;
	struct stat st;
	char *dest, src[512];

	if ((fd = open(pam_file, O_RDONLY)) < 0)
		return 0;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return 0;
	}
	close(fd);

	if (!st.st_size)
		return 0;

	if (!(dest = malloc(st.st_size + (st.st_size / 20))))
		return 0;

	dest[0] = '\0';
	if (!(f = fopen(pam_file, "r"))) {
		free(dest);
		return 0;
	}

	while (fgets(src, sizeof(src) - 1, f)) {
		int family = _librouter_pam_get_aaa_family(mode);

		strcat(dest, src);

		if (family == AAA_AUTH) {

			if (strstr(src, "_SECTION AUTHENTICATION"))
				_librouter_pam_comment_section(src, dest, f);

		} else if (family == AAA_AUTHOR) {

			if (strstr(src, "_SECTION AUTHORIZATION"))
				_librouter_pam_comment_section(src, dest, f);

		} else if (family == AAA_ACCT) {

			if (strstr(src, "_SECTION ACCOUNTING"))
				_librouter_pam_comment_section(src, dest, f);

		}
	}
	fclose(f);

	if ((f = fopen(pam_file, "w"))) {
		fwrite(dest, 1, strlen(dest), f);
		fclose(f);
		free(dest);
		return 1;
	}

	free(dest);

	return 0;
}

/**
 * Configure mode in PAM config file
 * Looks for the strings identifying the AAA modes and sets the mode accordingly.
 *
 * @param src source buffer
 * @param dest destination buffer
 * @param mode current PAM mode
 * @param pam_file PAM configuration file
 * @return
 */
int pam_set_mode(char *src, char *dest, int mode, char *pam_file)
{
	int family = _librouter_pam_get_aaa_family(mode);

	if (strstr(src, "auth_current_mode") && family == AAA_AUTH) {
		/* Authentication */
		if (mode == AAA_AUTH_LOCAL)
			strcat(dest, "#auth_current_mode=AAA_AUTH_LOCAL\n");
		else if (mode == AAA_AUTH_RADIUS)
			strcat(dest, "#auth_current_mode=AAA_AUTH_RADIUS\n");
		else if (mode == AAA_AUTH_RADIUS_LOCAL)
			strcat(dest, "#auth_current_mode=AAA_AUTH_RADIUS_LOCAL\n");
		else if (mode == AAA_AUTH_TACACS)
			strcat(dest, "#auth_current_mode=AAA_AUTH_TACACS\n");
		else if (mode == AAA_AUTH_TACACS_LOCAL)
			strcat(dest, "#auth_current_mode=AAA_AUTH_TACACS_LOCAL\n");
		else
			strcat(dest, "#auth_current_mode=AAA_AUTH_NONE\n");

		return 1;

	} else if (strstr(src, "author_current_mode") && family == AAA_AUTHOR) {
		/* Authorization */
		if (mode == AAA_AUTHOR_TACACS)
			strcat(dest, "#author_current_mode=AAA_AUTHOR_TACACS\n");
		else if (mode == AAA_AUTHOR_TACACS_LOCAL)
			strcat(dest, "#author_current_mode=AAA_AUTHOR_TACACS_LOCAL\n");
		else
			strcat(dest, "#author_current_mode=AAA_AUTHOR_NONE\n");

		return 1;

	} else if (strstr(src, "acct_current_mode") && family == AAA_ACCT) {
		/* Exec accouting */
		if (mode == AAA_ACCT_TACACS)
			strcat(dest, "#acct_current_mode=AAA_ACCT_TACACS\n");
		else
			strcat(dest, "#acct_current_mode=AAA_ACCT_NONE\n");
		return 1;

	} else if (strstr(src, "acct_current_command_mode") && family == AAA_ACCT_CMD) {
		/* Command accounting */
		int current_acct;

		current_acct = librouter_pam_get_current_acct_cmd_mode(pam_file);
		if (mode == AAA_ACCT_TACACS_CMD_1) {
			if (current_acct == AAA_ACCT_TACACS_CMD_15)
				strcat(dest, "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_ALL\n");
			else
				strcat(dest, "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_1\n");
		} else if (mode == AAA_ACCT_TACACS_CMD_15) {
			if (current_acct == AAA_ACCT_TACACS_CMD_1)
				strcat(dest, "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_ALL\n");
			else
				strcat(dest, "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_15\n");
		} else if (mode == AAA_ACCT_TACACS_NO_CMD_15) {
			if (current_acct == AAA_ACCT_TACACS_CMD_ALL)
				strcat(dest, "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_1\n");
			else
				strcat(dest,
				                "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_NONE\n");
		} else if (mode == AAA_ACCT_TACACS_NO_CMD_1) {
			if (current_acct == AAA_ACCT_TACACS_CMD_ALL)
				strcat(dest, "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_15\n");
			else
				strcat(dest,
				                "#acct_current_command_mode=AAA_ACCT_TACACS_CMD_NONE\n");
		}
		/*
		 * else if (mode == AAA_ACCT_TACACS_CMD_ALL) {
		 * 	strcat(dest,"#acct_current_command_mode=AAA_ACCT_TACACS_CMD_ALL\n");
		 * } else {
		 * 	strcat(dest,"#acct_current_command_mode=AAA_ACCT_TACACS_CMD_NONE\n");
		 * }
		 */
		return 1;
	} else {
		/* Nothing was changed */
		return 0;
	}
}

/**
 * Configure PAM mode in configuration file
 *
 * @param mode mode to be configured
 * @param pam_file
 * @return 1 when success, 0 otherwise
 */
int librouter_pam_config_mode(int mode, char *pam_file)
{
	FILE *f;
	struct stat st;
	int fd;
	char *p, *buffer, buf[512];

	if ((fd = open(pam_file, O_RDONLY)) < 0)
		return 0;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return 0;
	}

	close(fd);

	/* So continua a execucao da funcao se st.st_size for maior que zero. */
	if (!st.st_size)
		return 0;

	if (!(buffer = malloc(2 * st.st_size)))
		return 0;

	/* Primeira varredura pelo arquivo: ligar ou desligar modo */
	_librouter_pam_disable_mode(mode, pam_file);

	buffer[0] = '\0';
	if (!(f = fopen(pam_file, "r"))) {
		free(buffer);
		return 0;
	}

	while (fgets(buf, sizeof(buf) - 1, f)) {
		if (pam_set_mode(buf, buffer, mode, pam_file)) {
			/* When 1 is returned, then line was changed */
			continue;
		} else if (_librouter_pam_uncomment_line(buf, mode)) {
			p = buf;

			/* remove commentary! */
			if (*p == '#')
				p++;

			/* removes eventual spaces */
			for (; *p == ' '; p++)
				;

			strcat(buffer, p);
		} else {
			strcat(buffer, buf);
		}
	}

	fclose(f);

	if ((f = fopen(pam_file, "w"))) {
		fwrite(buffer, 1, strlen(buffer), f);
		fclose(f);
	}

	free(buffer);
	return 1;
}

int librouter_pam_get_auth_type(char *device)
{
	int serial_no = 0;
	ppp_config cfg;

#if defined(CONFIG_DEVFS_FS)
	if (!strcmp("tts/wan0", device)) serial_no=0;
	else if (!strcmp("tts/wan1", device)) serial_no=1;
	else if (!strcmp("tts/aux0", device)) serial_no=2;
	else if (!strcmp("tts/aux1", device)) serial_no=3;
#else
	if (!strcmp("ttyW0", device))
		serial_no = 0;
	else if (!strcmp("ttyW1", device))
		serial_no = 1;
	else if (!strcmp("ttyS0", device))
		serial_no = 2;
	else if (!strcmp("ttyS1", device))
		serial_no = 3;
#endif
	else
		return AUTH_TYPE_NONE;

	if (librouter_ppp_has_config(serial_no)) {
		librouter_ppp_get_config(serial_no, &cfg);

		if (cfg.server_flags & SERVER_FLAGS_CHAP) {
			return AUTH_TYPE_CHAP;
		} else if (cfg.server_flags & SERVER_FLAGS_PAP) {
			return AUTH_TYPE_PAP;
		}
	}

	return AUTH_TYPE_NONE;
}

int librouter_pam_get_users(char *users)
{
	FILE *f;
	/* Dump users */
	if ((f = fopen(FILE_PASSWD, "r"))) {
		struct passwd *pw;

		while ((pw = fgetpwent(f))) {
			if (pw->pw_uid > 500 && !strncmp(pw->pw_gecos, "Local", 5)) {
				strcat(users, pw->pw_name);
				strcat(users, " ");
			}
		}

		fclose(f);
	}
	return 0;
}

static int _validate_username(char *username)
{
	int i;
	const char *users[7] = { "root", "admin", "ppp", "uucp", "upload", "nobody", NULL };

	for (i = 0; users[i]; i++) {
		if (strcmp(users[i], username) == 0) {
			syslog(LOG_ERR, "%% Cannot configure %s user!\n", username);
			printf("%% Cannot configure %s user!\n", username);
			return -1;
		}
	}

	return 0;
}

int librouter_pam_add_user(char *user, char *pw)
{
	if (user == NULL || pw == NULL)
		return -1;

	if (_validate_username(user) < 0)
		return -1;

	librouter_exec_prog(1, "/bin/deluser", user, NULL);
	librouter_exec_prog(1, "/bin/adduser", user, "-p", pw, NULL);

	return 0;
}

int librouter_pam_del_user(char *user)
{
	if (user == NULL)
		return -1;

	if (_validate_username(user) < 0)
		return -1;

	librouter_exec_prog(1, "/bin/deluser", user, NULL);

	return 0;
}

int librouter_pam_add_user_to_group (char *user, char *group)
{
	if (user == NULL || group == NULL)
		return -1;

	if (_validate_username(user) < 0)
		return -1;

	librouter_exec_prog(1, "/bin/addgroup", user, group, NULL);

	return 0;
}

int librouter_pam_del_user_from_group (char *user, char *group)
{
	if (user == NULL || group == NULL)
		return -1;

	if (_validate_username(user) < 0)
		return -1;

	librouter_exec_prog(1, "/bin/delgroup", user, group, NULL);

	return 0;
}

static int test()
{

	   int j, ngroups;
	   gid_t *groups;
	   struct passwd *my_passwd;
	   struct group *gr;
	   int ret=0, num_groups=0;
	   uid_t me;

	   me = getuid ();
	   my_passwd = getpwuid (me);

	   if (!my_passwd)
	   {
		syslog (LOG_INFO, "Couldn't find out about user %d.\n", (int) me);
		return -1;
	   }

	   /* Retrieve number of groups */
	   getgrouplist(my_passwd->pw_name, my_passwd->pw_gid, groups, &ngroups);

	   /* Reserving memory for the N groups */
	   groups = malloc(ngroups * sizeof (gid_t));
	   if (groups == NULL) {
		   syslog (LOG_INFO, "Couldn't reserve memory for find out about user %d.\n", (int) me);
		   return -1;
	   }

	   /* Retrieve group list */
	   if (getgrouplist(my_passwd->pw_name, my_passwd->pw_gid, groups, &ngroups) == -1) {
		   fprintf(stderr, "getgrouplist() returned -1; ngroups = %d\n",
				   ngroups);
		   return -1;
	   }

	   /* Display list of retrieved groups, along with group names */

	   fprintf(stderr, "ngroups = %d\n", ngroups);
	   for (j = 0; j < ngroups; j++) {
		   printf("%d", groups[j]);
		   gr = getgrgid(groups[j]);
		   if (gr != NULL)
			   printf(" (%s)", gr->gr_name);
		   printf("\n");
	   }

	   free(groups);

	   return 0;
}

int librouter_pam_get_privilege (void)
{

	test();
//	int ret=0, num_groups=0;
//	uid_t me;
//	struct passwd *my_passwd;
//	struct group *my_group;
//	char **members;
//
//	/* Get information about the user ID.  */
//	me = getuid ();
//	my_passwd = getpwuid (me);
//	if (!my_passwd)
//	{
//	  syslog (LOG_INFO, "Couldn't find out about user %d.\n", (int) me);
//	  return -1;
//	}
//
//	printf ("I am %s.\n", my_passwd->pw_gecos);
//	printf ("My login name is %s.\n", my_passwd->pw_name);
//	printf ("My uid is %d.\n", (int) (my_passwd->pw_uid));
//	/* Get information about the default group ID.  */
//	my_group = getgrgid (my_passwd->pw_gid);
//	if (!my_group)
//	{
//	  syslog (LOG_INFO, "Couldn't find out about group %d.\n", (int) my_passwd->pw_gid);
//	  return -1;
//	}
//
//	/* Print the information.  */
//	printf ("1My default group is %s (%d).\n", my_group->gr_name, (int) (my_passwd->pw_gid));
//
//	getgrouplist();
//	getgroups();
//
//	my_group = getgrgid (my_passwd->pw_gid);
//	if (!my_group)
//	{
//	  syslog (LOG_INFO, "Couldn't find out about group %d.\n", (int) my_passwd->pw_gid);
//	  return -1;
//	}
//
//	/* Print the information.  */
//	printf ("2My default group is %s (%d).\n", my_group->gr_name, (int) (my_passwd->pw_gid));
//
//	return 0;
}







/**
 * librouter_pam_get_tacacs_servers
 *
 * Read configuration from TACACS+ file and fills in array supplied by caller.
 * Caller must free alloc'ed data.
 *
 * @param server
 * @return 0 if success
 */
int librouter_pam_get_tacacs_servers(struct auth_server *server)
{
	FILE *f;
	arglist *args;
	char line[128];

	f = fopen(FILE_TACDB_SERVER, "r");
	if (f == NULL)
		return -1;

	while (fgets(line, sizeof(line), f) != NULL) {
		librouter_str_striplf(line); /* Remove line feed */
		args = librouter_make_args(line);
		server->ipaddr = strdup(args->argv[0]);
		if (args->argc == 3) {
			server->key = strdup(args->argv[1]);
			server->timeout = atoi(args->argv[2]);
		}
		librouter_destroy_args(args);
		server++;
	}

	fclose(f);
	return 0;
}

/**
 * librouter_pam_get_radius_servers
 *
 * Read configuration from RADIUS file and fills in array supplied by caller.
 * Caller must free alloc'ed data.
 *
 * @param server
 * @return 0 if success
 */
int librouter_pam_get_radius_servers(struct auth_server *server)
{
	FILE *f;
	arglist *args;
	char line[128];

	f = fopen(FILE_RADDB_SERVER, "r");
	if (f == NULL)
		return -1;

	while (fgets(line, sizeof(line), f) != NULL) {
		librouter_str_striplf(line); /* Remove line feed */
		args = librouter_make_args(line);
		server->ipaddr = strdup(args->argv[0]);
		if (args->argc == 3) {
			server->key = strdup(args->argv[1]);
			server->timeout = atoi(args->argv[2]);
		}
		librouter_destroy_args(args);
		server++;
	}

	fclose(f);
	return 0;
}

/**
 * librouter_pam_free_servers
 *
 * Free fields for authentication server in array supplied by caller
 *
 * @param num_servers
 * @param server
 */
void librouter_pam_free_servers(int num_servers, struct auth_server *server)
{
	int i;

	if (server == NULL)
		return;

	for (i=0; i< num_servers; i++) {
		if (server->ipaddr) {
			free(server->ipaddr);
			server->ipaddr = NULL;
		}

		if (server->key) {
			free(server->key);
			server->key = NULL;
		}
		server++;
	}
}

/**
 * _add_servers_to_file		Write server information to file
 *
 * @param filename
 * @param servers
 * @return 0 when success
 */
static int _add_servers_to_file(char *filename, struct auth_server *servers)
{
	FILE *f;
	int i;

	f = fopen(filename, "w");

	if (f == NULL)
		return -1;

	for (i = 0; i < AUTH_MAX_SERVERS; i++) {
		if (servers[i].ipaddr) {
			if (servers[i].key)
				fprintf(f, "%s\t%s\t%d\n", servers[i].ipaddr, servers[i].key,
				                servers[i].timeout);
			else
				fprintf(f, "%s\n", servers[i].ipaddr);
		}
	}

	fclose(f);

	return 0;
}

/**
 * _dup_server	Copies server information from orig to dest
 *
 * @param dest
 * @param orig
 */
static void _dup_server(struct auth_server *dest, struct auth_server *orig)
{
	memset(dest, 0, sizeof(struct auth_server));

	if (orig->ipaddr)
		dest->ipaddr = strdup(orig->ipaddr);

	if (orig->key)
		dest->key = strdup(orig->key);

	dest->timeout = orig->timeout;
}

/**
 * librouter_pam_add_tacacs_server	Add TACACS+ server to the system
 *
 * @param server
 * @return 0 when success, -1 when maximum number of servers reached
 */
int librouter_pam_add_tacacs_server(struct auth_server *server)
{
	FILE *f;
	int i;
	struct auth_server current_servers[AUTH_MAX_SERVERS];

	memset(current_servers, 0, sizeof(current_servers));

	librouter_pam_get_tacacs_servers(current_servers);

	/* Check if server already exists */
	for (i = 0; i < AUTH_MAX_SERVERS; i++) {
		if (current_servers[i].ipaddr == NULL) {  /* Empty field, use it */
			_dup_server(&current_servers[i], server);
			break;
		} else if (!strcmp(server->ipaddr, current_servers[i].ipaddr)) { /* Match! */
			_dup_server(&current_servers[i], server);
			break;
		}
	}

	if (i == AUTH_MAX_SERVERS) {/* No room for another server */
		librouter_pam_free_servers(AUTH_MAX_SERVERS, current_servers);
		return -1;
	}


	_add_servers_to_file(FILE_TACDB_SERVER, current_servers);
	librouter_pam_free_servers(AUTH_MAX_SERVERS, current_servers);

	return 0;
}

/**
 * librouter_pam_del_tacacs_server	Delete server in system
 *
 * If parameter passed is NULL, all servers will be deleted
 *
 * @param server
 * @return 0 when success
 */
int librouter_pam_del_tacacs_server(struct auth_server *server)
{
	int i;
	FILE *f;
	struct auth_server current[AUTH_MAX_SERVERS];

	memset(current, 0, sizeof(current));

	/* delete all servers ? */
	if (server == NULL) {
		_add_servers_to_file(FILE_TACDB_SERVER, current);
		return 0;
	}

	if (librouter_pam_get_tacacs_servers(current) < 0) {
		librouter_logerr("Could not fetch server info\n");
		return -1;
	}

	/* Search if requested server exists*/
	for (i = 0; i < AUTH_MAX_SERVERS; i++) {
		if (!strcmp(server->ipaddr, current[i].ipaddr)) {
			librouter_pam_free_servers(1, &current[i]); /* Delete it! */
			break;
		}
	}

	_add_servers_to_file(FILE_TACDB_SERVER, current);

	librouter_pam_free_servers(AUTH_MAX_SERVERS, current);

	return 0;
}

/**
 * librouter_pam_add_radius_server	Add RADIUS server to the system
 *
 * @param server
 * @return 0 when success
 */
int librouter_pam_add_radius_server(struct auth_server *server)
{
	FILE *f;
	int i;
	struct auth_server current_servers[AUTH_MAX_SERVERS];

	memset(current_servers, 0, sizeof(current_servers));

	librouter_pam_get_radius_servers(current_servers);

	/* Check if server already exists */
	for (i = 0; i < AUTH_MAX_SERVERS; i++) {
		if ((current_servers[i].ipaddr == NULL) || /* Empty field, use it */
		!strcmp(server->ipaddr, current_servers[i].ipaddr)) {
			_dup_server(&current_servers[i], server);
			break;
		}
	}

	if (i == AUTH_MAX_SERVERS) {/* No room for another server */
		librouter_pam_free_servers(AUTH_MAX_SERVERS, current_servers);
		return -1;
	}

	_add_servers_to_file(FILE_RADDB_SERVER, current_servers);
	librouter_pam_free_servers(AUTH_MAX_SERVERS, current_servers);

	return 0;
}

/**
 * librouter_pam_del_radius_server	Delete server in system
 *
 * If parameter passed is NULL, all servers will be deleted
 *
 * @param server
 * @return 0 when success
 */
int librouter_pam_del_radius_server(struct auth_server *server)
{
	int i;
	FILE *f;
	struct auth_server current[AUTH_MAX_SERVERS];

	memset(current, 0, sizeof(current));

	/* delete all servers ? */
	if (server == NULL) {
		_add_servers_to_file(FILE_RADDB_SERVER, current);
		return 0;
	}

	if (librouter_pam_get_tacacs_servers(current) < 0) {
		librouter_logerr("Could not fetch server info\n");
		return -1;
	}

	/* Search if requested server exists*/
	for (i = 0; i < AUTH_MAX_SERVERS; i++) {
		if (!strcmp(server->ipaddr, current[i].ipaddr)) {
			librouter_pam_free_servers(1, &current[i]); /* Delete it! */
			break;
		}
	}

	_add_servers_to_file(FILE_RADDB_SERVER, current);

	librouter_pam_free_servers(AUTH_MAX_SERVERS, current);

	return 0;
}

