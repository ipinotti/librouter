#ifndef _LIB_ACL_H
#define _LIB_ACL_H 1

typedef enum {chain_in, chain_out, chain_both} acl_chain;

int acl_exists(char *acl);
int matched_acl_exists(char *acl, char *iface_in, char *iface_out, char *chain);
int get_iface_acls(char *iface, char *in_acl, char *out_acl);
int get_acl_refcount(char *acl);
int clean_iface_acls(char *iface);
int copy_iface_acls(char *src, char *trg);
int interface_ipsec_acl(int add_del, int chain, char *dev, char *listno);
void cleanup_modules(void);

#endif

