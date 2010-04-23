#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <ctype.h>
#include <termios.h>
#include "str.h"
#include "error.h"

int isblank (int c);

// Procura pela string "key" dentro da string "buffer", e retorna
// a string entre o byte imediatamente seguinte a "key" e o proximo
// espaco em branco ou nova linha.
// Ex.: buffer = "xxx abc:def yyy"
//      key = "abc:"
// Retorna -> "def"
char *find_substr(char *buf, char *key)
{
	char *p, *p2;

	p = strstr(buf, key);
	if (p==NULL) return NULL;

	p += strlen(key);
	p2 = p; while ((*p2)&&(*p2!=' ')&&(*p2!='\n')) p2++;
	if (!*p2) return NULL;
	*p2 = 0;

	return (p);
}

// Procura no arquivo 'filename' pela string 'key'. 
// Substitui a string entre o final de 'key' e o final da linha pela 
// string 'value'.
// Exemplo:
// Suponha que temos um arquivo 'snmpd.conf' com a seguinte linha:
//   syslocation Unknown
// A chamada 
//   replace_string_in_file_nl("snmpd.conf", "syslocation", "PD3");
// ira alterar a linha para
//   syslocation PD3
int replace_string_in_file_nl(char *filename, char *key, char *value)
{
	int fd=0, len, i, ret=-1;
	char *buf=NULL, *p, *p2, space=' ', coment='#';
	struct stat st;
	char filename_new[64];

	if ((fd=open(filename, O_RDONLY)) < 0)
	{
		pr_error(1, "could not open %s", filename);
		goto error;
	}
	if (fstat(fd, &st) < 0)
	{
		pr_error(1, "fstat");
		goto error;
	}
	if ((buf=malloc(st.st_size)) == NULL)
	{
		pr_error(1, "could not alloc memory");
		goto error;
	}
	read(fd, buf, st.st_size);
	close(fd);

	p=strstr(buf, key);
	if (p == NULL) goto end;
	p2=strchr(p, '\n');
	if (p2 == NULL) goto end;

	strncpy(filename_new, filename, 63);
	filename_new[63]=0;
	strcat(filename_new, ".new");
	if ((fd=open(filename_new, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0) 
	{
		pr_error(1, "could not create %s", filename_new);
		goto error;
	}
	i=p-buf;
	len=strlen(key);
	if (value != NULL) {
		if (i && buf[i-1] == '#') {
			write(fd, buf, i-1); /* skip # */
			write(fd, key, len);
		}
			else write(fd, buf, p-buf+len);
		write(fd, &space, 1); /* [key] [value] */
		write(fd, value, strlen(value));
	} else {
		if (i && buf[i-1] == '#') {
			write(fd, buf, p-buf+len);
		} else {
			write(fd, buf, i);
			write(fd, &coment, 1); /* # */
			write(fd, key, len);
		}
	}
	write(fd, p2, st.st_size-(p2-buf));
	close(fd);

	unlink(filename);
	rename(filename_new, filename);
	ret=0;
end:
	if (buf) free(buf);
	return ret;
error:
	if (fd) close(fd);
	goto end;
}

// Procura no arquivo 'filename' pela string 'key'. Se encontra-la,
// armazena  a string entre o final de 'key' e o final da linha no
// buffer 'buffer' de tamanho 'len'.
// Exemplo:
// Suponha que temos um arquivo 'snmpd.conf' com a seguinte linha:
//   syslocation Unknown
// A chamada 
//   find_string_in_file_nl("snmpd.conf", "syslocation", buf, 100);
// ira armazenar em 'buf' a string 'Unknown'.
int find_string_in_file_nl(char *filename, char *key, char *buffer, int len)
{
	int fd=0, size;
	char *buf=NULL, *p, *p2;
	int ret=-1; /* error! */
	struct stat st;

	if ((fd=open(filename, O_RDONLY))<0)
	{
		pr_error(1, "could not open %s", filename);
		goto error;
	}
	if (fstat(fd, &st)<0)
	{
		pr_error(1, "fstat");
		goto error;
	}
	if ((buf=malloc(st.st_size)) == NULL)
	{
		pr_error(1, "could not alloc memory");
		goto error;
	}
	read(fd, buf, st.st_size);
	close(fd);

	p=strstr(buf, key);
	if (p == NULL) goto end;
	p2=strchr(p, '\n');
	if (p2 == NULL) goto end;
	p += strlen(key)+1; /* !!! skip space after key! [A]=[B] */
	if (p >= p2) goto end;
	size = (p2-p) > (len-1) ? (len-1) : p2-p;
	memcpy(buffer, p, size);
	buffer[size]=0;
	ret=0;
end:
	if (buf) free(buf);
	return ret;
error:
	if (fd) close(fd);
	goto end;
}

void striplf(char *string)
{
	int ln; /* string length tempvar */

	ln=strlen(string);
	while ((ln > 0) && (string[ln-1] < 32))
		string[--ln]=0;
}

int is_empty(char *string)
{
	char *c=string;
	
	while ((*c)&&(isspace(*c)||isblank(*c))) c++;
	return (*c==0);
}

/*
 * Replace a exact string(key) in a file
 */
int replace_exact_string(char *filename, char *key, char *value)
{
	struct stat st;
	unsigned int len;
	int fd = 0, ret = -1;
	char *p, *buf = NULL;

	if( !filename || !key || !value )
		return ret;
	len = strlen(key);
	if( (fd = open(filename, O_RDONLY)) < 0 ) {
		pr_error(0, "could not open file %s", filename);
		goto error;
	}
	if( fstat(fd, &st) < 0 ) {
		pr_error(0, "fstat");
		goto error;
	}
	if( (buf = malloc(st.st_size + 1)) == NULL ) {
		pr_error(0, "malloc");
		goto error;
	}
	read(fd, buf, st.st_size);
	close(fd);
	*(buf + st.st_size) = 0;

	if( (p = strstr(buf, key)) == NULL )
		goto end;
	if( (fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, st.st_mode)) < 0 ) {
		pr_error(0, "could not create %s", filename);
		goto end;
	}
	write(fd, buf, p-buf);
	write(fd, value, strlen(value));
	write(fd, p + strlen(key), st.st_size - (p + strlen(key) - buf));
	close(fd);
	ret = 0;

end:
	if( buf )
		free(buf);
	return ret;

error:
	if( fd )
		close(fd);
	goto end;
}

#define TERM_CURSORLEFT(a)	"\e["#a"D"	/* moves cursor */

unsigned int readPassword(int echo_on, char *store, unsigned int max_len)
{
	char local[10];
	struct termios zap, original;
	int i, len, recv, fd=fileno(stdin);

	if( (store == NULL) || (max_len == 0) )
		return 0;
	*store = 0;
	fflush(stdout);
	tcgetattr(fd, &original);
	zap = original;
	zap.c_lflag &= ~(ECHO | ICANON);
	/* Desabilita echo */
	tcsetattr(fd, TCSANOW, &zap);
	for( len=0; len < (max_len - 1); ) {
		if( (recv = read(fd, local, sizeof(local))) > 0 ) {
			for( i=0; i < recv; i++ ) {
				if( local[i] == '\n' ) {
					store[len] = 0;
					/* Retorna a configuracao original */
					tcsetattr(fd, TCSANOW, &original);
					return len;
				}
				if( isgraph(local[i]) == 0 ) {
					/* Verificamos a possibilidade de um backspace */
					if( (local[i] == 0x08) && len ) {
						if( echo_on > 0 ) {
							tcsetattr(fd, TCSANOW, &original);
							printf(TERM_CURSORLEFT(1));
							printf(" ");
							printf(TERM_CURSORLEFT(1));
							fflush(stdout);
							tcsetattr(fd, TCSANOW, &zap);
						}
						len--;
					}
				}
				else {
					if( len < (max_len - 1) ) {
						store[len++] = local[i];
						if( echo_on > 0 ) {
							tcsetattr(fd, TCSANOW, &original);
							//printf("%c", local[i]);
							printf("*");
							fflush(stdout);
							tcsetattr(fd, TCSANOW, &zap);
						}
					}
					else
						break;
				}
			}
		}
	}
	store[len] = 0;
	/* Retorna a configuracao original */
	tcsetattr(fd, TCSANOW, &original);
	return len;
}

