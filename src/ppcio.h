/*
 * ppcio.h
 *
 *  Created on: Jun 24, 2010
 */

#ifndef PPCIO_H_
#define PPCIO_H_

/*
 * For the MRG, we use a kernel module (outside the
 * kernel source tree), and the header file is simply
 * installed at the include directory. Remove the linux
 * include when all boards have ported to the same kernel module.
 */
#ifdef CONFIG_DIGISTAR_MRG
#include <ppc_io.h>
#else
#include <linux/asm/ppc_io.h>
#endif

int librouter_ppcio_read(struct powerpc_gpio *gpio);
int librouter_ppcio_write(struct powerpc_gpio *gpio);

#endif /* PPCIO_H_ */
