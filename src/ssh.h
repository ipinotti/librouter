#include "options.h"

#ifdef OPTION_OPENSSH
#define SSH_KEY_FILE "/etc/ssh/ssh_host_rsa_key"
#else
#define SSH_KEY_FILE "/etc/dropbear/dropbear_rsa_host_key"
#endif

int ssh_create_rsakey(int);

