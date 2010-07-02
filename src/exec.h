/*
 * exec.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef EXEC_H_
#define EXEC_H_

/* prototipos de funcoes */
int librouter_exec_telinit(char c, int sleeptime);
int librouter_exec_init_program(int add_del, char *prog);
int librouter_exec_change_init_option(int add_del, char *prog, char *option);
int librouter_exec_get_init_option_value(char *prog,
                                  char *option,
                                  char *store,
                                  unsigned int max_len);
int librouter_exec_check_daemon(char *prog);
int librouter_exec_prog(int no_out, char *path, ...);

#define librouter_exec_daemon(s) librouter_exec_init_program(1, s)
#define librouter_kill_daemon(s) librouter_exec_init_program(0, s)

int librouter_exec_set_inetd_program(int add_del, char *prog);
int librouter_exec_get_inetd_program(char *prog);
int librouter_exec_prog_capture(char *buffer, int len, char *path, ...);
int librouter_exec_replace_str_before_key(char *filename, char *key, char *com_chr);
int librouter_exec_replace_string_file(char *filename, char *key, char *new_string);
int librouter_exec_copy_file(char *filename_src, char *filename_dst);
int librouter_exec_control_inittab_lineoptions(char *program, char *option, char *value);
int librouter_exec_get_inittab_lineoptions(char *program,
                            char *option,
                            char *store,
                            unsigned int maxlen);
int librouter_exec_test_write_len(char *filename,
                         unsigned int maxsize,
                         char *buf,
                         unsigned int len);

#endif /* EXEC_H_ */

