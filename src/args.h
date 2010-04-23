#ifndef _ARGS_H
#define _ARGS_H 1

#include "typedefs.h"

typedef struct
{
	int argc;
	char **argv;
} arglist;

char *stripwhite(char *line);
int argcount(const char *);
arglist *make_args(const char *);
void destroy_args(arglist *);
int parse_args_din(char *cmd_line, arg_list *rcv_p);
void free_args_din(arg_list *rcv_buf);

#endif

