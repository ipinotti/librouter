/*
 * str.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef STR_H_
#define STR_H_

char *librouter_str_find_substr(char *buf, char *key);
int librouter_str_replace_string_in_file(char *filename,
                                         char *key,
                                         char *value);
int librouter_str_find_string_in_file(char *filename,
                                      char *key,
                                      char *buffer,
                                      int len);
void librouter_str_strip_slash (char *string);
void librouter_str_striplf(char *string);
int librouter_str_is_empty(char *string);
int librouter_str_replace_exact_string(char *filename, char *key, char *value);
unsigned int librouter_str_read_password(int echo_on,
                                         char *store,
                                         unsigned int max_len);

int librouter_str_add_line_to_file(char *filename, char *line);
int librouter_str_del_line_in_file(char *filename, char *key);
int librouter_str_find_string_in_file_return_stat(char *filename, char *key);


#endif /* STR_H_ */
