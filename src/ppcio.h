/*
 * ppcio.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef PPCIO_H_
#define PPCIO_H_

typedef struct {
	unsigned char led_sys;
} ppcio_data;

int librouter_ppcio_read(ppcio_data *pd);
int librouter_ppcio_write(ppcio_data *pd);

#endif /* PPCIO_H_ */
