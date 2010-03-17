#ifndef _TYPEDEFS
#define _TYPEDEFS

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef unsigned char BOOL;
typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short int u16;
typedef unsigned char u8;
typedef struct in_addr IP;
typedef char **arg_list;

#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

#define is_ip_zero(ip) ( ((ip).s_addr)==((unsigned int)0) )

#endif

