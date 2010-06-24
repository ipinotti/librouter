/*
 * exec.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef EXEC_H_
#define EXEC_H_

/* prototipos de funcoes */
int libconfig_exec_telinit(char c, int sleeptime);
int libconfig_exec_init_program(int add_del, char *prog);
int libconfig_exec_change_init_option(int add_del, char *prog, char *option);
int libconfig_exec_get_init_option_value(char *prog,
                                  char *option,
                                  char *store,
                                  unsigned int max_len);
int libconfig_exec_check_daemon(char *prog);
int libconfig_exec_prog(int no_out, char *path, ...);

#define libconfig_exec_daemon(s) libconfig_exec_init_program(1, s)
#define libconfig_kill_daemon(s) libconfig_exec_init_program(0, s)

int libconfig_exec_set_inetd_program(int add_del, char *prog);
int libconfig_exec_get_inetd_program(char *prog);
int libconfig_exec_prog_capture(char *buffer, int len, char *path, ...);
int libconfig_exec_replace_str_before_key(char *filename, char *key, char *com_chr);
int libconfig_exec_replace_string_file(char *filename, char *key, char *new_string);
int libconfig_exec_copy_file(char *filename_src, char *filename_dst);
int libconfig_exec_control_inittab_lineoptions(char *program, char *option, char *value);
int libconfig_exec_get_inittab_lineoptions(char *program,
                            char *option,
                            char *store,
                            unsigned int maxlen);
int libconfig_exec_test_write_len(char *filename,
                         unsigned int maxsize,
                         char *buf,
                         unsigned int len);

#endif /* EXEC_H_ */

