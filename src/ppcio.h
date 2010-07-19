/*
 * ppcio.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef PPCIO_H_
#define PPCIO_H_

#include <linux/asm/ppc_io.h>

int librouter_ppcio_read(struct powerpc_gpio *gpio);
int librouter_ppcio_write(struct powerpc_gpio *gpio);

#endif /* PPCIO_H_ */
