/*
 * bcm53115s.h
 *
 *  Created on: Mar 1, 2011
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef BCM53115S_H_
#define BCM53115S_H_

#include <stdint.h>

/* DEBUG Function */
#define BCM53115S_DEBUG
#ifdef BCM53115S_DEBUG
#define bcm53115s_dbg(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define bcm53115s_dbg(x,...)
#endif

//BCM53115S DEFINES

#define CMD_SPI_BYTE_RD 0x60
#define CMD_SPI_BYTE_WR 0x61
#define ROBO_SPIF_BIT 7
#define BCM53115_SPI_CHANNEL 1
#define ROBO_RACK_BIT 5

#define VLAN_START_BIT 7
#define VLAN_WRITE_CMD 0

//#define BCM_PORT_1G 2
typedef enum
{
   BCM_PORT_10M = 0,
   BCM_PORT_100M,
   BCM_PORT_1G,
}BCM_PORT_SPEED;

#define BCM_PORT_0 0
#define BCM_PORT_1 1
#define BCM_PORT_2 2
#define BCM_PORT_3 3
#define BCM_PORT_4 4
#define BCM_PORT_5 5
#define BCM_PORT_IMP 6


#define BCM53115S_NUM_VLAN_TABLES		16
#define BCM53115S_SPI_DEV	"/dev/spidev28672.0"
#define BCM53115S_SPI_ADDR	0x5f

#define BCM53115S_ID	0x8830

#define BCM53115SREG_GLOBAL_CONTROL0	0x02
#define BCM53115SREG_GLOBAL_CONTROL1	0x03
#define BCM53115SREG_GLOBAL_CONTROL2	0x04
#define BCM53115SREG_GLOBAL_CONTROL3	0x05
#define BCM53115SREG_GLOBAL_CONTROL4	0x06
#define BCM53115SREG_GLOBAL_CONTROL5	0x07
#define BCM53115SREG_GLOBAL_CONTROL10	0x0C
#define BCM53115SREG_GLOBAL_CONTROL11	0x0D

#define BCM53115SREG_PORT_CONTROL3	0x13
#define BCM53115SREG_PORT_CONTROL4	0x14

#define BCM53115SREG_PORT_CONTROL		0x10
#define BCM53115SREG_INGRESS_RATE_LIMIT	0x16
#define BCM53115SREG_EGRESS_RATE_LIMIT	0x9A

#define BCM53115SREG_TOS_PRIORITY_CONTROL	0x60

#define BCM53115SREG_INDIRECT_ACCESS_CONTROL0	0x79
#define BCM53115SREG_INDIRECT_ACCESS_CONTROL1	0x7A

#define BCM53115SREG_INDIRECT_DATA8		0x7B
#define BCM53115SREG_INDIRECT_DATA7		0x7C
#define BCM53115SREG_INDIRECT_DATA6		0x7D
#define BCM53115SREG_INDIRECT_DATA5		0x7E
#define BCM53115SREG_INDIRECT_DATA4		0x7F
#define BCM53115SREG_INDIRECT_DATA3		0x80
#define BCM53115SREG_INDIRECT_DATA2		0x81
#define BCM53115SREG_INDIRECT_DATA1		0x82
#define BCM53115SREG_INDIRECT_DATA0		0x83

/* Global Control 2 */
#define BCM53115SREG_ENABLE_MC_STORM_PROTECT_MSK	0x40

/* Global Control 3 */
#define BCM53115S_ENABLE_8021Q_MSK	0x80
#define BCM53115S_ENABLE_WFQ_MSK		0x08

/* Global Control 4 */
#define BCM53115S_ENABLE_REPLACE_NULL_VID_MSK	0x08

/* Port n Control 0 */
#define BCM53115SREG_ENABLE_BC_STORM_PROTECT_MSK	0x80
#define BCM53115SREG_ENABLE_DIFFSERV_MSK		0x40
#define BCM53115SREG_ENABLE_8021P_MSK		0x20
#define BCM53115SREG_ENABLE_TAGINSERT_MSK		0x04
#define BCM53115SREG_ENABLE_TAGREMOVE_MSK		0x02
#define BCM53115SREG_ENABLE_TXQSPLIT_MSK		0x01

/* Port n Control 3 */
#define BCM53115SREG_DEFAULT_COS_MSK		0xE0
#define BCM53115SREG_DEFAULT_VID_UPPER_MSK	0x0F

/* Indirect Access Control */
#define BCM53115SREG_READ_OPERATION		0x10
#define BCM53115SREG_WRITE_OPERATION		0x00
#define BCM53115SREG_VLAN_TABLE_SELECT		0x04

#define BCM53115SREG_VLAN_MEMBERSHIP_PORT1_MSK	0x01
#define BCM53115SREG_VLAN_MEMBERSHIP_PORT2_MSK	0x02
#define BCM53115SREG_VLAN_MEMBERSHIP_PORT3_MSK	0x04

struct vlan_config_t {
	int vid;
	int membership;
};

struct vlan_table_t {
	unsigned int valid:1;
	unsigned int membership:3;
	unsigned int fid:4;
	unsigned int vid:12;
};

int librouter_bcm53115s_read_test(uint8_t page, uint8_t offset, int len);
int librouter_bcm53115s_write_test(uint8_t page, uint8_t offset, uint8_t data, int len);


///* Global Control 2 */
//int librouter_bcm53115s_set_multicast_storm_protect(int enable);
//int librouter_bcm53115s_get_multicast_storm_protect(void);
//
///* Global Control 3 */
//int librouter_bcm53115s_get_8021q(void);
//int librouter_bcm53115s_set_8021q(int enable);
//
//int librouter_bcm53115s_get_wfq(void);
//int librouter_bcm53115s_set_wfq(int enable);
//
///* Global Control 4 & 5 */
//int librouter_bcm53115s_get_replace_null_vid(void);
//int librouter_bcm53115s_set_replace_null_vid(int enable);
//
//int librouter_bcm53115s_get_storm_protect_rate(unsigned int *percentage);
//int librouter_bcm53115s_set_storm_protect_rate(unsigned int percentage);
//
///* Port n Control 1*/
//int librouter_bcm53115s_get_broadcast_storm_protect(int port);
//int librouter_bcm53115s_set_broadcast_storm_protect(int enable, int port);
//
//int librouter_bcm53115s_get_8021p(int port);
//int librouter_bcm53115s_set_8021p(int enable, int port);
//
//int librouter_bcm53115s_get_diffserv(int port);
//int librouter_bcm53115s_set_diffserv(int enable, int port);
//
//int librouter_bcm53115s_get_default_vid(int port);
//int librouter_bcm53115s_set_default_vid(int port, int vid);
//
//int librouter_bcm53115s_get_txqsplit(int port);
//int librouter_bcm53115s_set_txqsplit(int enable, int port);
//
//int librouter_bcm53115s_get_taginsert(int port);
//int librouter_bcm53115s_set_taginsert(int enable, int port);
//
///* Port n Control 3 */
//int librouter_bcm53115s_get_default_vid(int port);
//int librouter_bcm53115s_set_default_vid(int port, int vid);
//
//int librouter_bcm53115s_get_default_cos(int port);
//int librouter_bcm53115s_set_default_cos(int port, int cos);
//
//int librouter_bcm53115s_get_cos_prio(int cos);
//int librouter_bcm53115s_set_cos_prio(int cos, int prio);
//
///* ------------------ */
//int librouter_bcm53115s_get_egress_rate_limit(int port, int prio);
//int librouter_bcm53115s_set_egress_rate_limit(int port, int prio, int rate);
//
//int librouter_bcm53115s_get_ingress_rate_limit(int port, int prio);
//int librouter_bcm53115s_set_ingress_rate_limit(int port, int prio, int rate);
//
//int librouter_bcm53115s_get_dscp_prio(int dscp);
//int librouter_bcm53115s_set_dscp_prio(int dscp, int prio);
//
///* VLAN */
//int librouter_bcm53115s_get_table(int entry, struct vlan_config_t *vconfig);
//int librouter_bcm53115s_del_table(struct vlan_config_t *vconfig);
//int librouter_bcm53115s_add_table(struct vlan_config_t *vconfig);
//
//
//
///* Initialization */
int librouter_bcm53115s_probe(void);
int librouter_bcm53115s_set_default_config(void);


#endif
