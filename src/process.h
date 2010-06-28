/*
 * process.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef PROCESS_H_
#define PROCESS_H_

#define MAX_PROC_CMDLINE   256

pid_t libconfig_process_get_info(char *progname, char *arg, char *proc_cmdline);
int libconfig_process_pid_exists(pid_t pid);
int libconfig_process_wait_for(pid_t pid, int timeout);
int libconfig_process_get_runlevel(void);

#define libconfig_process_get_pid(a) libconfig_process_get_info(a, NULL, NULL)
#define libconfig_process_get_pid_with_arg(a, b) libconfig_process_get_info(a, b, NULL)

#endif /* PROCESS_H_ */
