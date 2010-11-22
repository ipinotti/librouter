/*
 * ksz8863.h
 *
 *  Created on: Nov 17, 2010
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef KSZ8863_H_
#define KSZ8863_H_

//#define KSZ8863_DEBUG
#ifdef KSZ8863_DEBUG
#define ksz8863_dbg(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define ksz8863_dbg(x,...)
#endif

#define KSZ8863_I2CDEV	"/dev/i2c-0"
#define KSZ8863_I2CADDR	0x5f

#define KSZ8863_ID	0x8830

#define KSZ8863REG_GLOBAL_CONTROL0	0x02
#define KSZ8863REG_GLOBAL_CONTROL1	0x03
#define KSZ8863REG_GLOBAL_CONTROL2	0x04
#define KSZ8863REG_GLOBAL_CONTROL3	0x05

#define KSZ8863REG_PORT_CONTROL3	0x13
#define KSZ8863REG_PORT_CONTROL4	0x14

#define KSZ8863REG_PORT_CONTROL		0x10
#define KSZ8863REG_INGRESS_RATE_LIMIT	0x16
#define KSZ8863REG_EGRESS_RATE_LIMIT	0x9A


/* Global Control 2 */
#define KSZ8863REG_ENABLE_MC_STORM_PROTECT_MSK	0x40

/* Global Control 3 */
#define KSZ8863_ENABLE_8021Q_MSK	0x80
#define KSZ8863_ENABLE_WFQ_MSK		0x08

/* Port n Control 0 */
#define KSZ8863REG_ENABLE_BC_STORM_PROTECT_MSK	0x80

/* Port n Control 3 */
#define KSZ8863REG_DEFAULT_VID_UPPER_MSK	0x0F


int librouter_ksz8863_get_broadcast_storm_protect(int port);
int librouter_ksz8863_set_broadcast_storm_protect(int enable, int port);

int librouter_ksz8863_set_multicast_storm_protect(int enable);
int librouter_ksz8863_get_multicast_storm_protect(void);

int librouter_ksz8863_get_egress_rate_limit(int port, int prio);
int librouter_ksz8863_set_egress_rate_limit(int port, int prio, int rate);

int librouter_ksz8863_get_ingress_rate_limit(int port, int prio);
int librouter_ksz8863_set_ingress_rate_limit(int port, int prio, int rate);

int librouter_ksz8863_get_8021q(void);
int librouter_ksz8863_set_8021q(int enable);

int librouter_ksz8863_get_wfq(void);
int librouter_ksz8863_set_wfq(int enable);

int librouter_ksz8863_get_default_vid(int port);
int librouter_ksz8863_set_default_vid(int port, int vid);

int librouter_ksz8863_probe(void);

#endif /* KSZ8863_H_ */
