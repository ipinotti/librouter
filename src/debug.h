/*
 * debug.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef LIBDEBUG_H_
#define LIBDEBUG_H_

typedef struct {
	const char *name;
	const char *token;
	const char *description;
	int enabled;
} debuginfo;

char *libconfig_debug_find_token(char *logline, char *name, int enabled);
int libconfig_debug_set_token(int on_off, const char *token);
void libconfig_debug_set_all(int on_off);
void libconfig_debug_dump(void);
int libconfig_debug_get_state(const char *token);
void libconfig_debug_recover_all(void);

unsigned int get_debug_console(void);

#endif /* LIBDEBUG_H_ */

