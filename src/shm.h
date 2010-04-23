typedef struct
{
    char ifname[IFNAMSIZ];
    struct in_addr local, remote, broadcast;
    int bitlen;
} ip_addr_table_t;

int create_ipaddr_shm(int flags);
int detach_ipaddr_shm(void);
int ipaddr_modify_shm(int add_del, ip_addr_table_t *data);

