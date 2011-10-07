/*
 * data.h
 *
 *  Created on: Oct 6, 2011
 *      Author: tgrande
 */

#ifndef DATA_H_
#define DATA_H_

int librouter_data_save(const char *name, void *data, int len);
int librouter_data_load(const char *name, char *data, int maxlen);

#endif /* DATA_H_ */
