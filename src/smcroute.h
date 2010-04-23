
#define SMC_ROUTE_MAX 20
#define SMC_ROUTE_CONF "/etc/smcroute.conf"

struct smc_route_database
{
	int valid;
	char origin[16], group[16], in[16], out[16];
};

// prototipos de funcoes
void kick_smcroute(void);
int smc_route(int add, char *origin, char *group, char *in, char *out);
void dump_mroute(FILE *out);

