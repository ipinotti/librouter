
typedef enum 
{
	mode,
	source,
	source_interface,
	destination,
	checksum,
	sequence,
	pmtu,
	ttl,
	key
} tunnel_param_type;

int add_tunnel(char *);
int del_tunnel(char *);
int change_tunnel(char *, tunnel_param_type, void *);
int mode_tunnel(char *, int);
void dump_tunnel_interface(FILE *, int, char *);

