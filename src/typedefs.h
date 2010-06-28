/*
 * typedefs.h
 *
 *  Created on: Jun 23, 2010
 */

#ifndef TYPEDEFS_H_
#define TYPEDEFS_H_

typedef unsigned char BOOL;
typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short int u16;
typedef unsigned char u8;

#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

#define is_ip_zero(ip) ( ((ip).s_addr)==((unsigned int)0) )

#endif /* TYPEDEFS_H_ */

