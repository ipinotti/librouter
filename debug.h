#ifndef _LIBDEBUG_H
#define _LIBDEBUG_H 1

typedef struct
{
	const char *name;
	const char *token;
	const char *description;
	int enabled;
} debuginfo;

char *find_debug_token(char *logline, char *name, int enabled);
int set_debug_token(int on_off, const char *token);
void set_debug_all(int on_off);
void dump_debug(void);
int get_debug_state(const char *token);
void recover_debug_all(void);

unsigned int get_debug_console(void);

#endif

