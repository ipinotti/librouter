#define MAX_PROC_CMDLINE   256

pid_t get_process_info(char *progname, char *arg, char *proc_cmdline);
int pid_exists(pid_t pid);
int wait_for_process(pid_t pid, int timeout);
int get_runlevel(void);

#define get_pid(a) get_process_info(a, NULL, NULL)
#define get_pid_with_arg(a, b) get_process_info(a, b, NULL)


