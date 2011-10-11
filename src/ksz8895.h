/*
 * ksz8895.h
 *
 *  Created on: Oct 4, 2011
 *      Author: Thomás Alimena Del Grande (tgrande@pd3.com.br)
 */

#ifndef KSZ8895_H_
#define KSZ8895_H_

//#define KSZ8895_DEBUG
#ifdef KSZ8895_DEBUG
#define ksz8895_dbg(x,...) \
		printf("%s : %d => "x, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define ksz8895_dbg(x,...)
#endif

#define KSZ8895_NUM_VLAN_TABLES		4096
#define KSZ8895_MAX_VLAN_ENTRIES	128
#define KSZ8895_NUM_VLAN_OBJ		"ksz8895_num_valid_vlans"
#define KSZ8895_ETH_IFACE		"eth0"
#define NUMBER_OF_SWITCH_PORTS		4

#define KSZ8895_ID	0x9540

#define KSZ8895REG_GLOBAL_CONTROL0	0x02
#define KSZ8895REG_GLOBAL_CONTROL1	0x03
#define KSZ8895REG_GLOBAL_CONTROL2	0x04
#define KSZ8895REG_GLOBAL_CONTROL3	0x05
#define KSZ8895REG_GLOBAL_CONTROL4	0x06
#define KSZ8895REG_GLOBAL_CONTROL5	0x07
#define KSZ8895REG_GLOBAL_CONTROL10	0x0C
#define KSZ8895REG_GLOBAL_CONTROL11	0x0D
#define KSZ8895REG_GLOBAL_CONTROL12	0x80
#define KSZ8895REG_GLOBAL_CONTROL13	0x81

#define KSZ8895REG_PORT_CONTROL3	0x13
#define KSZ8895REG_PORT_CONTROL4	0x14

#define KSZ8895REG_PORT_CONTROL		0x10
#define KSZ8895REG_INGRESS_RATE_LIMIT	0xB7
#define KSZ8895REG_EGRESS_RATE_LIMIT	0xBB

#define KSZ8895REG_TOS_PRIORITY_CONTROL	0x90

#define KSZ8895REG_INDIRECT_ACCESS_CONTROL0	0x6E
#define KSZ8895REG_INDIRECT_ACCESS_CONTROL1	0x6F

#define KSZ8895REG_INDIRECT_DATA8		0x70
#define KSZ8895REG_INDIRECT_DATA7		0x71
#define KSZ8895REG_INDIRECT_DATA6		0x72
#define KSZ8895REG_INDIRECT_DATA5		0x73
#define KSZ8895REG_INDIRECT_DATA4		0x74
#define KSZ8895REG_INDIRECT_DATA3		0x75
#define KSZ8895REG_INDIRECT_DATA2		0x76
#define KSZ8895REG_INDIRECT_DATA1		0x77
#define KSZ8895REG_INDIRECT_DATA0		0x78

/* Global Control 2 */
#define KSZ8895REG_ENABLE_MC_STORM_PROTECT_MSK	0x40

/* Global Control 3 */
#define KSZ8895_ENABLE_8021Q_MSK	0x80
#define KSZ8895_ENABLE_WFQ_MSK		0x08

/* Global Control 4 */
#define KSZ8895_ENABLE_REPLACE_NULL_VID_MSK	0x08

/* Port n Control 0 */
#define KSZ8895REG_ENABLE_BC_STORM_PROTECT_MSK	0x80
#define KSZ8895REG_ENABLE_DIFFSERV_MSK		0x40
#define KSZ8895REG_ENABLE_8021P_MSK		0x20
#define KSZ8895REG_ENABLE_TAGINSERT_MSK		0x04
#define KSZ8895REG_ENABLE_TAGREMOVE_MSK		0x02
#define KSZ8895REG_ENABLE_TXQSPLIT_MSK		0x01

/* Port n Control 3 */
#define KSZ8895REG_DEFAULT_COS_MSK		0xE0
#define KSZ8895REG_DEFAULT_VID_UPPER_MSK	0x0F

/* Indirect Access Control */
#define KSZ8895REG_READ_OPERATION		0x10
#define KSZ8895REG_WRITE_OPERATION		0x00
#define KSZ8895REG_VLAN_TABLE_SELECT		0x04

#define KSZ8895REG_VLAN_MEMBERSHIP_PORT1_MSK	0x01
#define KSZ8895REG_VLAN_MEMBERSHIP_PORT2_MSK	0x02
#define KSZ8895REG_VLAN_MEMBERSHIP_PORT3_MSK	0x04
#define KSZ8895REG_VLAN_MEMBERSHIP_PORT4_MSK	0x08
#define KSZ8895REG_VLAN_MEMBERSHIP_PORT5_MSK	0x10



typedef enum {
	real_sw, alias_sw, non_sw
} port_switch_type;

typedef struct {
	port_switch_type type;
	const int port[NUMBER_OF_SWITCH_PORTS];
} port_family_switch;

struct vlan_config_t {
	int vid;
	int membership;
};

struct vlan_table_t {
	unsigned int unused:19;
	unsigned int valid:1;
	unsigned int membership:5;
	unsigned int fid:7;
};

/* CLI and WEB show interfaces numbers translation */
int librouter_ksz8895_get_aliasport_by_realport(int switch_port);
int librouter_ksz8895_get_realport_by_aliasport(int switch_port);


/* For tests only */
int librouter_ksz8895_read(__u8 reg);
int librouter_ksz8895_write(__u8 reg, __u8 data);

/* Global Control 2 */
int librouter_ksz8895_set_multicast_storm_protect(int enable);
int librouter_ksz8895_get_multicast_storm_protect(void);

/* Global Control 3 */
int librouter_ksz8895_get_8021q(void);
int librouter_ksz8895_set_8021q(int enable);

int librouter_ksz8895_get_wfq(void);
int librouter_ksz8895_set_wfq(int enable);

/* Global Control 4 & 5 */
int librouter_ksz8895_get_replace_null_vid(void);
int librouter_ksz8895_set_replace_null_vid(int enable);

int librouter_ksz8895_get_storm_protect_rate(void);
int librouter_ksz8895_set_storm_protect_rate(unsigned int percentage);

/* Port n Control 1*/
int librouter_ksz8895_get_broadcast_storm_protect(int port);
int librouter_ksz8895_set_broadcast_storm_protect(int enable, int port);

int librouter_ksz8895_get_8021p(int port);
int librouter_ksz8895_set_8021p(int enable, int port);

int librouter_ksz8895_get_diffserv(int port);
int librouter_ksz8895_set_diffserv(int enable, int port);

int librouter_ksz8895_get_default_vid(int port);
int librouter_ksz8895_set_default_vid(int port, int vid);

int librouter_ksz8895_get_txqsplit(int port);
int librouter_ksz8895_set_txqsplit(int enable, int port);

int librouter_ksz8895_get_taginsert(int port);
int librouter_ksz8895_set_taginsert(int enable, int port);

/* Port n Control 3 */
int librouter_ksz8895_get_default_vid(int port);
int librouter_ksz8895_set_default_vid(int port, int vid);

int librouter_ksz8895_get_default_cos(int port);
int librouter_ksz8895_set_default_cos(int port, int cos);

int librouter_ksz8895_get_cos_prio(int cos);
int librouter_ksz8895_set_cos_prio(int cos, int prio);

/* ------------------ */
int librouter_ksz8895_get_egress_rate_limit(int port, int prio);
int librouter_ksz8895_set_egress_rate_limit(int port, int prio, int rate);

int librouter_ksz8895_get_ingress_rate_limit(int port, int prio);
int librouter_ksz8895_set_ingress_rate_limit(int port, int prio, int rate);

int librouter_ksz8895_get_dscp_prio(int dscp);
int librouter_ksz8895_set_dscp_prio(int dscp, int prio);

/* VLAN */
int librouter_ksz8895_get_table(int entry, struct vlan_table_t *v);
int librouter_ksz8895_del_table(struct vlan_config_t *vconfig);
int librouter_ksz8895_add_table(struct vlan_config_t *vconfig);



/* Initialization */
int librouter_ksz8895_probe(void);
int librouter_ksz8895_set_default_config(void);

/* Dump */
int librouter_ksz8895_dump_config(FILE *out);



#endif /* KSZ8895_H_ */
