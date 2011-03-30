/*
 * bcm53115s.h
 *
 *  Created on: Mar 1, 2011
 *      Author: Igor Kramer Pinotti (ipinotti@pd3.com.br)
 */

#ifndef BCM53115S_H_
#define BCM53115S_H_

#include <stdint.h>

/* DEBUG Functions */
//#define BCM53115S_DEBUG_PRINTF
#define BCM53115S_DEBUG_SYSLOG

#ifdef BCM53115S_DEBUG_PRINTF
#define bcm53115s_dbg_printf(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define bcm53115s_dbg_printf(x,...)
#endif

#ifdef BCM53115S_DEBUG_SYSLOG
#define bcm53115s_dbg_syslog(x,...) \
		syslog(LOG_INFO,  "%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define bcm53115s_dbg_syslog(x,...)
#endif
/* End DEBUG Functions */


//BCM53115S DEFINES

#define CMD_SPI_BYTE_RD 	0x60
#define CMD_SPI_BYTE_WR 	0x61
#define ROBO_SPIF_BIT 		7
#define BCM53115_SPI_CHANNEL 	1
#define ROBO_RACK_BIT 		5

#define VLAN_START_BIT 		7
#define VLAN_WRITE_CMD 		0
#define VLAN_READ_CMD		1
#define VLAN_CLR_TABLE_CMD 	2
#define VLAN_RD_WR_BIT_0 	0
#define VLAN_RD_WR_BIT_1 	1



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


#define BCM53115S_NUM_VLAN_TABLES		4094
#define BCM53115S_SPI_DEV	"/dev/spidev28672.0"
#define BCM53115S_ID	0x53115

/* Storm protect */
#define BCM53115S_STORM_PROTECT_PAGE			0x41

#define BCM53115S_STORM_PROTECT_INGRESS_RATE_CTRL_REG	0x00
#define BCM53115S_STORM_PROTECT_RESERVED_MASK		0xFFF90080
#define BCM53115S_STORM_PROTECT_MC_SUPP_EN		0x00000002
#define BCM53115S_STORM_PROTECT_BC_SUPP_EN		0x00000008
#define BCM53115S_STORM_PROTECT_BC_SUPP_NORMALIZED	0x00000100

#define BCM53115S_STORM_PROTECT_PORT_CTRL_REG		0x10

/* Storm suppressing, bc storm suppresion and bucket 0 */
#define BCM53115S_STORM_PROTECT_PORT_CTRL_BC_SUPR_EN	0x14400000
#define BCM53115S_STORM_PROTECT_PORT_CTRL_MC_SUPR_EN	0x02000000
#define BCM53115S_STORM_PROTECT_PORT_CTRL_RATE_MSK	0x000000FF

/* Global IEEE 802.1Q Register Page 43h: Address 00h */
#define ROBO_VLAN_PAGE				0x34 /* VLAN Registers */
#define BCM53115S_VLAN_PAGE			ROBO_VLAN_PAGE
#define BCM53115S_ENABLE_8021Q_MSK		0x80


#define BCM53115S_VLAN_DROPUNTAG_REG		0x03

#define BCM53115S_VLAN_TAG_REG			0x10
#define BCM53115S_VLAN_DEFAULT_COS_MSK		0xE000
#define BCM53115S_VLAN_DEFAULT_VID_MSK		0x0FFF


/* QoS Registers Page 30h */
#define ROBO_QOS_PAGE				0x30 /* QoS Registers */
#define BCM53115S_QOS_PAGE			ROBO_QOS_PAGE
#define BCM53115S_QOS_GLOBAL_CTRL_REG		0x00
#define BCM53115S_QOS_8021P_ENABLE_REG		0x04
#define BCM53115S_QOS_DIFFSERV_ENABLE_REG	0x06

#define BCM53115S_QOS_COS_PRIOMAP_REG		0x10
#define BCM53115S_QOS_DIFFSERV_PRIOMAP_REG	0x30
#define BCM53115S_TC_TO_COS_MAP_REG		0x62

#define BCM53115S_QOS_TXQ_CONTROL_REG		0x80
#define BCM53115S_QOS_TXQ_CONTROL_WRR_MSK	0x03




/* DELETE!!! DELETE!!! DELETE!!! */
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


/* Indirect Access Control */
#define BCM53115SREG_READ_OPERATION		0x10
#define BCM53115SREG_WRITE_OPERATION		0x00
#define BCM53115SREG_VLAN_TABLE_SELECT		0x04
#define BCM53115SREG_PAGE_VLAN_TABLE_INDEX	0x05
#define BCM53115SREG_OFFSET_VLAN_TABLE_INDEX	0x81
#define BCM53115SREG_PAGE_VLAN_TABLE_RD_WR_CL	0x05
#define BCM53115SREG_OFFSET_VLAN_TABLE_RD_WR_CL	0x80
#define BCM53115SREG_OFFSET_VLAN_TABLE_ENTRY	0x83


#define BCM53115SREG_VLAN_MEMBERSHIP_PORT1_MSK	0x01
#define BCM53115SREG_VLAN_MEMBERSHIP_PORT2_MSK	0x02
#define BCM53115SREG_VLAN_MEMBERSHIP_PORT3_MSK	0x04
#define BCM53115SREG_VLAN_MEMBERSHIP_PORT4_MSK  0x08

struct vlan_bcm_config_t {
	int vid; /*vlan id*/
	int membership;
};

struct vlan_bcm_table_t {
	unsigned int reserved:10;
	unsigned int fwd_mode:1;
	unsigned int mspt_index:3;
	unsigned int untag_map_cpu_port:1;
	unsigned int untag_map_reserved:3;
	unsigned int untag_map_ports:5;
	unsigned int fwd_map_cpu_port:1;
	unsigned int fwd_map_reserved:3;
	unsigned int fwd_map_ports:5;
};

/* Page 41h: Broadcast Storm Suppresion Register */
int librouter_bcm53115s_get_broadcast_storm_protect(int port);
int librouter_bcm53115s_set_broadcast_storm_protect(int enable, int port);

int librouter_bcm53115s_get_storm_protect_rate(unsigned char *rate, int port);
int librouter_bcm53115s_set_storm_protect_rate(unsigned char rate, int port);

int librouter_bcm53115s_set_multicast_storm_protect(int enable, int port);
int librouter_bcm53115s_get_multicast_storm_protect(int port);
//
///* Global Control 3 */
int librouter_bcm53115s_get_8021q(void);
int librouter_bcm53115s_set_8021q(int enable);

int librouter_bcm53115s_get_wrr(void);
int librouter_bcm53115s_set_wrr(int enable);
//
///* Global Control 4 & 5 */
//int librouter_bcm53115s_get_replace_null_vid(void);
//int librouter_bcm53115s_set_replace_null_vid(int enable);
//

//
///* Port n Control 1*/


int librouter_bcm53115s_get_8021p(int port);
int librouter_bcm53115s_set_8021p(int enable, int port);

int librouter_bcm53115s_get_diffserv(int port);
int librouter_bcm53115s_set_diffserv(int enable, int port);
//
int librouter_bcm53115s_get_default_vid(int port);
int librouter_bcm53115s_set_default_vid(int port, int vid);
//
//
//int librouter_bcm53115s_get_taginsert(int port);
//int librouter_bcm53115s_set_taginsert(int enable, int port);

int librouter_bcm53115s_set_drop_untagged(int enable, int port);
int librouter_bcm53115s_get_drop_untagged(int port);
//
int librouter_bcm53115s_get_default_vid(int port);
int librouter_bcm53115s_set_default_vid(int port, int vid);

int librouter_bcm53115s_get_default_cos(int port);
int librouter_bcm53115s_set_default_cos(int port, int cos);

int librouter_bcm53115s_get_cos_prio(int cos);
int librouter_bcm53115s_set_cos_prio(int cos, int prio);

int librouter_bcm53115s_get_dscp_prio(int dscp);
int librouter_bcm53115s_set_dscp_prio(int dscp, int prio);
//
///* VLAN */
//int librouter_bcm53115s_get_table(int entry, struct vlan_config_t *vconfig);
//int librouter_bcm53115s_del_table(struct vlan_config_t *vconfig);
//int librouter_bcm53115s_add_table(struct vlan_config_t *vconfig);



///* Initialization */
int librouter_bcm53115s_probe(void);
int librouter_bcm53115s_set_default_config(void);


#endif
