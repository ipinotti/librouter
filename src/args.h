/*
 * args.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef ARGS_H_
#define ARGS_H_

typedef char **arg_list;

typedef struct {
	int argc;
	char **argv;
} arglist;

char *stripwhite(char *line);
int librouter_arg_count(const char *);
arglist *librouter_make_args(const char *);
void librouter_destroy_args(arglist *);
int librouter_parse_args_din(char *cmd_line, arg_list *rcv_p);
void librouter_destroy_args_din(arg_list *rcv_buf);

#endif /* ARGS_H_ */

