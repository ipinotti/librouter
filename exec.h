#ifndef _LIB_EXEC_H_
#define _LIB_EXEC_H_

#include <sys/types.h>
#include <unistd.h>

// prototipos de funcoes
int telinit(char c, int sleeptime);
int init_program(int add_del, char *prog);
int init_program_change_option(int add_del, char *prog, char *option);
int init_program_get_option_value(char *prog, char *option, char *store, unsigned int max_len);
int is_daemon_running(char *prog);
int exec_prog(int no_out, char *path, ...);

#define exec_daemon(s) init_program(1, s)
#define kill_daemon(s) init_program(0, s)

int set_inetd_program(int add_del, char *prog);
int get_inetd_program(char *prog);
int exec_prog_capture(char *buffer, int len, char *path, ...);
int replace_str_bef_key(char *filename, char *key, char *com_chr);
int replace_string_file(char *filename, char *key, char *new_string);
int copy_file_to_file(char *filename_src, char *filename_dst);
int control_inittab_lineoptions(char *program, char *option, char *value);
int get_inittab_lineoptions(char *program, char *option, char *store, unsigned int maxlen);
int test_file_write_size(char *filename, unsigned int maxsize, char *buf, unsigned int len);

#endif // _LIB_EXEC_H_

