/*
 * ssh.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef SSH_H_
#define SSH_H_

#include "options.h"

#ifdef OPTION_OPENSSH
#define SSH_KEY_FILE "/etc/ssh/ssh_host_rsa_key"
#else
#define SSH_KEY_FILE "/etc/dropbear/dropbear_rsa_host_key"
#endif

int librouter_ssh_create_rsakey(int);

#endif /* SSH_H_ */
