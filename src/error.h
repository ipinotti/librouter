/*
 * error.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef ERROR_H_
#define ERROR_H_

void librouter_pr_error(int output_strerror, char *fmt, ...);
void librouter_logerr(char *fmt, ...);

#endif /* ERROR_H_ */
