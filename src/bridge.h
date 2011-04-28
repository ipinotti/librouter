#ifndef LIBROUTER_BRIDGE_H
#define LIBROUTER_BRIDGE_H

#define FILE_BRIDGE_IP	"/var/run/bridge.ip"

//#define BR_DEBUG
#ifdef BR_DEBUG
#define br_dbg(x,...) \
		syslog(LOG_INFO, "%s : %d => "x , __FUNCTION__, __LINE__, ##__VA_ARGS__);
#else
#define br_dbg(x,...)
#endif

struct ipa_t {
	char addr[16];
	char mask[16];
};

int librouter_br_initbr(void);
int librouter_br_exists(char *brname);
int librouter_br_addbr(char *brname);
int librouter_br_delbr(char *brname);
int librouter_br_addif(char *brname, char *ifname);
int librouter_br_delif(char *brname, char *ifname);
int librouter_br_checkif(char *brname, char *ifname);
int librouter_br_hasifs(char *brname);
int librouter_br_setageing(char *brname, int sec);
int librouter_br_getageing(char *brname);
int librouter_br_setbridgeprio(char *brname, int prio);
int librouter_br_getbridgeprio(char *brname);
int librouter_br_setfd(char *brname, int sec);
int librouter_br_getfd(char *brname);
int librouter_br_setgcint(char *brname, int sec);
int librouter_br_sethello(char *brname, int sec);
int librouter_br_gethello(char *brname);
int librouter_br_setmaxage(char *brname, int sec);
int librouter_br_getmaxage(char *brname);
int librouter_br_setpathcost(char *brname, char *portname, int cost);
int librouter_br_setportprio(char *brname, char *portname, int cost);
int librouter_br_set_stp(char *brname, int on_off);
int librouter_br_get_stp(char *brname);
int librouter_br_dump_info(char *brname, FILE *out);
void librouter_br_dump_bridge(FILE *out);
int librouter_br_update_ipaddr(char *ifname);
int librouter_br_get_ipaddr(char *brname, struct ipa_t *ip);

#endif /* LIBROUTER_BRIDGE_H */
