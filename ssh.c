#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libconfig/defines.h>
#include <libconfig/args.h>
#include <libconfig/error.h>
#include <libconfig/nv.h>
#include <libconfig/ssh.h>

int ssh_create_rsakey(int keysize)
{
	int ret;
	char line[128];

	unlink(SSH_KEY_FILE);
#ifdef OPTION_OPENSSH
	sprintf(line, "/bin/ssh-keygen -q -t rsa -N \"\" -f %s -b %d", SSH_KEY_FILE, keysize);
#else
	sprintf(line, "/bin/dropbearkey -t rsa -f %s -s %d >/dev/null 2>/dev/null", SSH_KEY_FILE, keysize);
#endif
	system(line);

	if (save_ssh_secret(SSH_KEY_FILE) < 0) {
		ret = -1;
		pr_error(1, "unable to save key");
	}
	else
		ret = 0;
	return ret;
}

