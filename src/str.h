/*
 * str.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef STR_H_
#define STR_H_

char *libconfig_str_find_substr(char *buf, char *key);
int libconfig_str_replace_string_in_file(char *filename,
                                         char *key,
                                         char *value);
int libconfig_str_find_string_in_file(char *filename,
                                      char *key,
                                      char *buffer,
                                      int len);
void libconfig_str_striplf(char *string);
int libconfig_str_is_empty(char *string);
int libconfig_str_replace_exact_string(char *filename, char *key, char *value);
unsigned int libconfig_str_read_password(int echo_on,
                                         char *store,
                                         unsigned int max_len);

#endif /* STR_H_ */
