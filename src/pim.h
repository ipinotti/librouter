#define PIMD_PID "/var/run/pimdd.pid"
#define PIMS_PID "/var/run/pimsd.pid"
#define PIMD_CFG_FILE "/etc/pimdd.conf"
#define PIMS_CFG_FILE "/etc/pimsd.conf"

#define MAX_LINES 50

int pimdd_phyint(int, char *);
int pimsd_phyint(int, char *);
void pimsd_bsr_candidate(int, char *, char *, char *);
void pimsd_rp_address(int, char *);
void pimsd_rp_candidate(int, char *, char *, char *, char *);

void lconfig_pim_dump(FILE *);
void dump_pim_interface(FILE *, char *);

