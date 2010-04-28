#include <stdio.h>
#include <string.h>
#include "md5.h"

unsigned char *hash_sn_str(unsigned char *serial)
{
	int i, serial_len;
	static unsigned char password[20];
	unsigned char buffer2[8], string[13], md5buffer[16];

	if( (serial == NULL) || ((serial_len = strlen((char *)serial)) < 6) ) /* O tamanho minimo para o numero de serie eh 6 */
		return NULL;
#if 0
	string[ 0] = 0xf3;
	string[ 2] = 0x43;
	string[ 4] = 0xa4;
	string[ 6] = 0x6b;
	string[ 8] = 0x41;
	string[10] = 0x56;
	string[ 1] = serial[5];
	string[ 3] = serial[3];
	string[ 5] = serial[4];
	string[ 7] = serial[1];
	string[ 9] = serial[2];
	string[11] = serial[0];
	string[12] = 0;
#else
	switch( serial_len ) {
		case 6:
			string[ 0] = 0xf3;
			string[ 2] = 0x43;
			string[ 4] = 0xa4;
			string[ 6] = 0x6b;
			string[ 8] = 0x41;
			string[10] = 0x56;
			string[ 1] = serial[5];
			string[ 3] = serial[3];
			string[ 5] = serial[4];
			string[ 7] = serial[1];
			string[ 9] = serial[2];
			string[11] = serial[0];
			string[12] = 0;
			break;
		case 10:
			string[ 0] = 0xf3;
			string[ 2] = 0x43;
			string[ 4] = serial[9];
			string[ 6] = serial[7];
			string[ 8] = serial[8];
			string[10] = serial[5];
			string[ 1] = serial[6];
			string[ 3] = serial[3];
			string[ 5] = serial[4];
			string[ 7] = serial[1];
			string[ 9] = serial[2];
			string[11] = serial[0];
			string[12] = 0;
			break;
		default:
			return NULL;
	}
#endif
	md5_buffer((char *)string, 12, md5buffer);
	buffer2[4] = md5buffer[ 4]^md5buffer[15];
	buffer2[2] = md5buffer[ 8]^md5buffer[ 2];
	buffer2[1] = md5buffer[ 6]^md5buffer[ 5];
	buffer2[6] = md5buffer[ 1]^md5buffer[ 9];
	buffer2[5] = md5buffer[12]^md5buffer[13];
	buffer2[3] = md5buffer[14]^md5buffer[10];
	buffer2[0] = md5buffer[ 7]^md5buffer[ 0];
	buffer2[7] = md5buffer[ 3]^md5buffer[11];
	buffer2[7] += 0x11;
	buffer2[3] += 0x9f;
	buffer2[2] += 0x79;
	buffer2[1] += 0x04;
	buffer2[0] += 0x2e;
	buffer2[6] += 0xbd;
	buffer2[5] += 0x59;
	buffer2[4] += 0x06;

	for( i=0; i < 8; i++ )
		sprintf((char *)password + (i<<1), "%02x", buffer2[i]);
	password[16] = 0;
	return password;
}

