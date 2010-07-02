/*
 * hash.c
 *
 *  Created on: Jun 23, 2010
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hash.h"
#include "md5.h"

#if 0
#define reverse_bits(b) ( (((b>>7)&1)<<0)|(((b>>6)&1)<<1)|(((b>>5)&1)<<2)|(((b>>4)&1)<<3)|(((b>>3)&1)<<4)|(((b>>2)&1)<<5)|(((b>>1)&1)<<6)|(((b>>0)&1)<<7) )
#define reverse_nibble(b) ( ((b&0x0f)<<4) | ((b>>4)&0xf) )
#endif

char *librouter_hash_str(char *buf, unsigned char id)
{
	int i;
	unsigned char md5buffer[16];
	unsigned char buffer2[8];
	static char password[17];

#ifdef I2C_HC08_ID_ADDR
	buf[16]=0xcc;
	buf[17]=0x93;
	buf[18]=0xfd;
	buf[19]=0x57;
	buf[20]=0xf6;
	buf[21]=0x93;
	buf[22]=0x1f;
	buf[23]=0xee;
	buf[24]=0x13;
	buf[25]=0xa4;
	buf[26]=0x41;
	buf[27]=0xfd;
	buf[28]=0x22;
	buf[29]=0x81;
	buf[30]=0x7c;
	buf[31]=0x19;
	for (i=0; i < 32; i++) buf[i] += id;
	md5_buffer(buf, 32, md5buffer);
#else
	unsigned char string[13];

	string[0] = 0x45;
	string[1] = buf[5];
	string[2] = 0x3f;
	string[3] = buf[4];
	string[4] = 0x6b;
	string[5] = buf[3];
	string[6] = 0x4a;
	string[7] = buf[1];
	string[8] = 0x41;
	string[9] = buf[0];
	string[10] = 0x65;
	string[11] = buf[2];
	for (i = 0; i < 12; i++)
		string[i] += id;
	string[12] = 0;
	md5_buffer(string, 12, md5buffer);
#endif
	buffer2[2] = md5buffer[4] ^ md5buffer[15];
	buffer2[4] = md5buffer[8] ^ md5buffer[2];
	buffer2[1] = md5buffer[6] ^ md5buffer[5];
	buffer2[3] = md5buffer[1] ^ md5buffer[9];
	buffer2[5] = md5buffer[12] ^ md5buffer[13];
	buffer2[6] = md5buffer[14] ^ md5buffer[10];
	buffer2[0] = md5buffer[7] ^ md5buffer[0];
	buffer2[7] = md5buffer[3] ^ md5buffer[11];
	buffer2[7] += 0x11;
	buffer2[6] += 0x9f;
	buffer2[2] += 0x79;
	buffer2[1] += 0x04;
	buffer2[0] += 0x2e;
	buffer2[3] += 0xbd;
	buffer2[5] += 0x59;
	buffer2[4] += 0x06;
	for (i = 0; i < 8; i++)
		sprintf(password + (i << 1), "%02x", buffer2[i]);
	password[16] = 0;
	return password;
}
