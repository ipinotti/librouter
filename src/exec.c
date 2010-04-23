#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <dirent.h>
#include <time.h>
#include <syslog.h>

#include "options.h"
#include "defines.h"
#include "exec.h"
#include "error.h"
#include "initreq.h"
#include "args.h"
#include "process.h"
#include "str.h"

int telinit(char c, int sleeptime)
{
	struct init_request request;
	int fd, bytes;

	memset(&request, 0, sizeof(request));
	request.magic = INIT_MAGIC;
	request.cmd = INIT_CMD_RUNLVL;
	request.runlevel = c;
	request.sleeptime = sleeptime;

	fd = open(INIT_FIFO, O_WRONLY);
	if (fd<0) 
	{
		pr_error(1, "telinit");
		return (-1);
	}

	bytes = write(fd, &request, sizeof(request));
	close(fd);

	return ((bytes==sizeof(request)) ? 0 : (-1));
}

#define FILE_INITTAB "/etc/inittab"

// Adiciona/deleta um programa da lista de programas iniciados pelo init.
int init_program(int add_del, char *prog)
{
	FILE *f;
	char buf[101];
	int found = 0;

	if ((f=fopen(FILE_INITTAB, "r+")) == NULL)
	{
		pr_error(1, "could not open %s", FILE_INITTAB);
		return (-1);
	}
	while ((!feof(f))&&(!found))
	{
		fgets(buf, 100, f); buf[100]=0;
		if (strstr(buf, prog))
		{
			fseek(f, -strlen(buf), SEEK_CUR);
			fputc(add_del ? 'u' : '#', f);
			found = 1;
		}
	}
	fclose(f);
	if (!found)
	{
		pr_error(0, "%s not found in %s", prog, FILE_INITTAB);
		return (-1);
	}
	return telinit('q', 5);
}

int is_daemon_running(char *prog)
{
	FILE *f;
	char buf[101];
	int found = 0;
	int c=0;

	f=fopen(FILE_INITTAB, "r+");
	if (!f)
	{
		pr_error(1, "could not open %s", FILE_INITTAB);
		return (0);
	}
	while ((!feof(f))&&(!found))
	{
		fgets(buf, 100, f); buf[100] = 0;
		if (strstr(buf, prog))
		{
			fseek(f, -strlen(buf), SEEK_CUR);
			c = fgetc(f);
			found = 1;
		}
	}
	fclose(f);
	if (!found)
	{
		pr_error(0, "%s not found in %s", prog, FILE_INITTAB);
		return (0);
	}
	return (c=='u');
}

#define MAX_ARGS 10

// Executa o programa "path" com seus argumentos "...", fechando
// stdout e stderr (para o programa nao apresentar mensagens em caso
// de erro) caso no_out seja 1 e retornando:
//  0 em caso de sucesso ou
// -1 em caso de erro na funcao ou caso o programa executado
//   retorne com exit_status menor que zero.
int exec_prog(int no_out, char *path, ...)
{
	int ret, i;
	pid_t pid;
	va_list ap;
	char *argv[MAX_ARGS];

	argv[0]=path;
	va_start(ap, path);
	for (i=1; i < MAX_ARGS; i++)
	{
		argv[i]=va_arg(ap, char *);
		if (argv[i] == NULL) break;
	}
	va_end(ap);
	argv[i]=NULL;

	pid=fork();
	switch (pid)
	{
		case 0: // child - execute program
			if (no_out)
			{
				close(1); // close stdout
				close(2); // close stderr
			}
			ret=execv(path, argv);
			pr_error(1, "unable to execute %s", path);
			return (-1);
		break;

		case -1:
			pr_error(1, "could not fork");
			return (-1);
		break;

		default: // parent - wait until child finishes
		{
			int status;
			if (waitpid(pid, &status, 0)<0)
			{
				pr_error(1, "waitpid");
				return (-1);
			}
			if (!WIFEXITED(status))
			{
				pr_error(0, "child did not exited normally");
				return (-1);
			}
			if (WEXITSTATUS(status)<0)
			{
				pr_error(0, "child exit status = %d %s", WEXITSTATUS(status), path);
				return (-1);
			}
			return 0;
		}
		break;
	}
}

#define FILE_INETD "/etc/inetd.conf"

// Adiciona/deleta um programa da lista de programas iniciados pelo inetd.
int set_inetd_program(int add_del, char *prog)
{
	FILE *f;
	char buf[101];
	int found = 0;
	pid_t pid;
	
	f = fopen(FILE_INETD, "r+");
	if (!f)
	{
		pr_error(1, "could not open %s", FILE_INETD);
		return (-1);
	}
	
	while ((!feof(f))&&(!found))
	{
		fgets(buf, 100, f); buf[100] = 0;
		if (strstr(buf, prog))
		{
			fseek(f, -strlen(buf), SEEK_CUR);
			fputc(add_del ? ' ' : '#', f);
			found = 1;
		}
	}
	
	fclose(f);
	
	if (!found)
	{
		pr_error(0, "%s not found in %s", prog, FILE_INETD);
		return (-1);
	}
	
	pid = get_pid("inetd");
	if (!pid)
	{
		pr_error(0, "inetd is not running");
		return (-1);
	}
	kill(pid, SIGHUP);
	
	return 0;
}

// Verfica se um programa esta ativo na lista de aplicativos iniciados pelo inetd
int get_inetd_program(char *prog)
{
	FILE *f;
	char buf[101];
	char *p;
	int found = 0;
  
	f = fopen(FILE_INETD, "r+");
	if (!f)
	{
		pr_error(1, "could not open %s", FILE_INETD);
		return (-1);
	}
  
	while ((!feof(f))&&(!found))
	{
		fgets(buf, 100, f); buf[100] = 0;
		p = buf;
		if ((*p != '#')&&(strstr(buf, prog)))
			found = 1;
	}

	fclose(f);

	return (found);
}

// Executa o programa "path" com seus argumentos "..." e armazena
// a saida do programa no buffer "buffer", de tamanho maximo "len".
int exec_prog_capture(char *buffer, int len, char *path, ...)
{
	int ret, i;
	pid_t pid;
	va_list ap;
	char *argv[MAX_ARGS];
	int fd[2];
	int nbytes;
	char *buf;
	
	len--;
	argv[0] = path;
	va_start(ap, path);
	for (i=1; i<MAX_ARGS; i++)
	{
		argv[i] = va_arg(ap, char *);
		if(argv[i]==NULL)	break;
	}
	va_end(ap);
	argv[i] = NULL;
	pipe(fd);
	pid = fork();
	switch (pid)
	{
		case 0: // child - execute program
			close(fd[0]); // fecha "input side of pipe".
			close(1); // fecha "standard output" ...
			close(2); // e "standard error"
			dup(fd[1]); // ... e substitui por "output side of pipe".
			dup(fd[1]);
			//invalidate_plist(); // para forcar a releitura da lista de processos
			ret = execv(path, argv);
			pr_error(1, "could not exec %s %s", path, argv);
			return -1;
			break;
		case -1:
			pr_error(1, "fork");
			return -1;
			break;
		default: // parent
			close(fd[1]); // fecha "output side of pipe"
			buf = buffer; 
			while (len>0)
			{
				nbytes = read(fd[0], buf, len);
				if(nbytes>0)
				{
					len -= nbytes;
					buf += nbytes;
				}
				else	if(nbytes<0)
					{
						perror("");
						exit(-1);
					}
					else break;
			}
			*buf = 0;
			waitpid(pid, 0, 0); // wait until child finishes
			return 1;
		break;
	}
}

/*
 *  Procura dentro do arquivo filename a primeira linha que contenha a string key;
 *  substitui os caracteres que precedem o inicio de key ate o inicio da linha, pela string com_chr fornecida
 */
int replace_str_bef_key(char *filename, char *key, char *com_chr)
{
	int fd=0, size_f, size_r, i, ret=-1;
	char *buf=NULL, *p=NULL, *pk=NULL, *r_tp=NULL;
	struct stat st;

	if ((fd = open(filename, O_RDONLY))<0)
	{
		ret = -1;
		goto error5;
	}

	if (fstat(fd, &st)<0)
	{
		ret = -1;
		goto error5;
	}
	//So continua a execucao da funcao se st.st_size for maior que zero.
	if(st.st_size)
	{
		if(!(buf = malloc(st.st_size + MAX_CMD_LINE + 3)))
		{
			ret = -1;
			goto error5;
		}
	}
	else goto error5;
	
	read(fd, buf, st.st_size);
	close(fd);
	*(buf+st.st_size) = '\0';

	p=pk= strstr(buf, key);			/* pk e p apontam para o inicio da palavra chave */
	if (pk == NULL) goto end5;

	for(i=0; (*pk != '\n')&&(pk > buf)&&(i<MAX_CMD_LINE); i++, pk--);
	if(i>=MAX_CMD_LINE)	goto error5;
	if(*pk == '\n')		pk++;		/* Agora pk aponta para o inicio da linha que contem key */
									/* Se nenhuma das duas opcoes acima for verdadeira, eh porque 
									   estamos no inicio do arquivo */
	
	size_r = (st.st_size - (p-buf));	/* sizer contem o tamanho do restante do arquivo */
	if(!(r_tp = malloc(size_r+1)))
	{
		ret = -1;
		goto error5;
	}
	memcpy(r_tp, p, size_r);		/* copia o resto do arquivo para um buffer temporario */
	
	memcpy(pk, com_chr, strlen(com_chr));	/* insere caracter(es) de comentario */
	pk += strlen(com_chr);			
	
	size_f = pk - buf;
	memcpy(pk, r_tp, size_r);		/* anexa-se novamente o resto do arquivo apos a insercao do simbolo de comentario */
	
	remove(filename);
	if ((fd=open(filename, O_WRONLY|O_CREAT, 0600))<0)
	{
		ret = -1;
		goto error5;
	}
	write(fd, buf, size_f + size_r);
	close(fd);
	ret = 1;

end5:
	if (buf) free(buf);
	if (r_tp) free(r_tp);
	return ret;
error5:
	if (fd) close(fd);
	goto end5;	
}

/*
 * Replace a exact string in a file
 */
int replace_string_file(char *filename, char *key, char *new_string)
{
	struct stat st;
	char *p, *buf = NULL;
	int len, fd = 0, ret = -1;

	len = strlen(key);
	if( (fd = open(filename, O_RDONLY)) < 0 ) {
		syslog(LOG_ERR, "not possible to open file '%s'", filename);
		return -1;
	}
	if( fstat(fd, &st) < 0 ) {
		syslog(LOG_ERR, "fstat");
		goto error;
	}
	if( (buf = malloc(st.st_size + 1)) == NULL ) {
		syslog(LOG_ERR, "not possible to alloc memory");
		goto error;
	}
	read(fd, buf, st.st_size);
	close(fd);
	fd = 0;

	*(buf + st.st_size) = 0;
	if( (p = strstr(buf, key)) == NULL )
		goto error;
	remove(filename);
	if( (fd = open(filename, O_WRONLY|O_CREAT, st.st_mode)) < 0 ) {
		syslog(LOG_ERR, "could not create file '%s'", filename);
		fd = 0;
		goto error;
	}
	write(fd, buf, p - buf);
	write(fd, new_string, strlen(new_string));
	write(fd, p + strlen(key), st.st_size - (p + strlen(key) - buf));
	ret = 0;

error:
	if( fd != 0 )
		close(fd);
	if( buf != NULL )
		free(buf);
	return ret;
}

int copy_file_to_file(char *filename_src, char *filename_dst)
{
	int fd;
	FILE *f;
	char *buf;
	struct stat st;

	if( (filename_src == NULL) || (filename_dst == NULL) )
		return -1;

	if( (fd = open(filename_src, O_RDONLY)) < 0 ) {
		syslog(LOG_ERR, "not possible to open file '%s'", filename_src);
		return -1;
	}
	if( fstat(fd, &st) < 0 ) {
		syslog(LOG_ERR, "fstat");
		close(fd);
		return -1;
	}
	if( (buf = malloc(st.st_size + 1)) == NULL ) {
		syslog(LOG_ERR, "not possible to alloc memory");
		close(fd);
		return -1;
	}
	read(fd, buf, st.st_size);
	close(fd);
	*(buf + st.st_size) = 0;

	if( (f = fopen(filename_dst, "w")) == NULL ) {
		free(buf);
		return -1;
	}
	if( fwrite(buf, 1, st.st_size, f) != st.st_size ) {
		fclose(f);
		free(buf);
		return -1;
	}
	fclose(f);
	free(buf);

	return 0;
}

/*
 *  Configura as opcoes das linhas dentro do inittab
 *
 *	Exemplo:
 *    wn:34:once:/bin/wnsd -a 0 -A 0 -p 80
 *    uma chamada do tipo 'control_inittab_lineoptions("/bin/wnsd", "-p", "82")' muda '80' para '82'
 */
int control_inittab_lineoptions(char *program, char *option, char *value)
{
	int fd;
	struct stat st;
	char *p, *p2, *buf, *p_f, tp[256];

	if( (program == NULL) || (option == NULL) || (value == NULL) )
		return -1;
	if( (fd = open(FILE_INITTAB, O_RDONLY)) < 0 ) {
		syslog(LOG_ERR, "could not open %s", FILE_INITTAB);
		return -1;
	}
	if( fstat(fd, &st) < 0 ) {
		syslog(LOG_ERR, "fstat error");
		close(fd);
		return -1;
	}
	if( (buf = malloc(st.st_size + strlen(value) + 6)) == NULL ) {
		syslog(LOG_ERR, "not possible to alloc memory");
		close(fd);
		return -1;
	}
	read(fd, buf, st.st_size);
	*(buf + st.st_size) = 0;
	close(fd);

	if( (p = strstr(buf, program)) == NULL ) {
		free(buf);
		return -1;
	}

	sprintf(tp, " %s ", option);
	if( (p2 = strstr(p, tp)) == NULL ) {
		free(buf);
		return -1;
	}

	p2 += strlen(tp);
	for( ; *p2 == ' '; p2++ );
	for( p=p2; (*p != ' ') && (*p != '\n'); p++ );
	*p2 = 0;
	if( (p_f = malloc(st.st_size + strlen(value) + 6)) == NULL ) {
		free(buf);
		return -1;
	}
	strcpy(p_f, p);
	strcat(buf, value);
	strcat(buf, p_f);
	free(p_f);

	remove(FILE_INITTAB);
	if( (fd = open(FILE_INITTAB, O_WRONLY|O_CREAT, st.st_mode)) < 0 ) {
		syslog(LOG_ERR, "could not create file '%s'", FILE_INITTAB);
		free(buf);
		return -1;
	}
	write(fd, buf, strlen(buf));
	free(buf);
	close(fd);

	return 0;
}

/*  Busca uma opcao das linhas dentro do inittab */
int get_inittab_lineoptions(char *program, char *option, char *store, unsigned int maxlen)
{
	int fd, len;
	struct stat st;
	char *p, *p2, *buf, tp[256];

	if( (program == NULL) || (option == NULL) || (store == NULL) || (maxlen == 0) )
		return -1;
	if( (fd = open(FILE_INITTAB, O_RDONLY)) < 0 ) {
		syslog(LOG_ERR, "could not open %s", FILE_INITTAB);
		return -1;
	}
	if( fstat(fd, &st) < 0 ) {
		syslog(LOG_ERR, "fstat error");
		close(fd);
		return -1;
	}
	if( (buf = malloc(st.st_size + 1)) == NULL ) {
		syslog(LOG_ERR, "not possible to alloc memory");
		close(fd);
		return -1;
	}
	read(fd, buf, st.st_size);
	*(buf + st.st_size) = 0;
	close(fd);

	if( (p = strstr(buf, program)) == NULL ) {
		free(buf);
		return -1;
	}

	sprintf(tp, " %s ", option);
	if( (p2 = strstr(p, tp)) == NULL ) {
		free(buf);
		return -1;
	}

	p2 += strlen(tp);
	for( ; *p2 == ' '; p2++ );
	for( p=p2; (*p != ' ') && (*p != '\n'); p++ );
	*p = 0;
	len = strlen(p2);
	if( (len > 0) && (len < maxlen) )
		strcpy(store, p2);
	else
		len = 0;
	free(buf);
	return len;
}

/* Rotina que verifica se eh possivel escrever 'len' bytes no final do arquivo
 * 'filename' de forma que o tamanho deste nao ultrapasse 'maxsize'.
 * Se a escrita superar o tamanhpo maximo permitido para o arquivo, a rotina
 * comecara a excluir as primeiras linhas do arquivo de forma a liberar espaco
 * para que a escrita desejada seja possivel.
 */
int test_file_write_size(char *filename, unsigned int maxsize, char *buf, unsigned int len)
{
	FILE *f;
	int fd, max;
	struct stat st;
	char *p, *local, *start;

	if( (filename == NULL) || (maxsize == 0) || (buf == NULL) || (len == 0) || (len > maxsize) )
		return -1;

	if( (fd = open(filename, O_RDONLY | O_CREAT)) < 0 )
		return -1;
	if( fstat(fd, &st) < 0 ) {
		close(fd);
		return -1;
	}
	close(fd);

	if( st.st_size > 0 ) {
		max = maxsize - 1;
		if( (st.st_size + len) > max ) {
			/* Necessita ajustes */
			if( (fd = open(filename, O_RDONLY)) < 0 )
				return -1;
			if( (local = malloc(st.st_size + 1)) == NULL ) {
				syslog(LOG_ERR, "not possible to alloc memory");
				close(fd);
				return -1;
			}
			read(fd, local, st.st_size);
			*(local + st.st_size) = 0;
			close(fd);
			for( start=local; *start != 0; ) {
				if( (p = strchr(start, '\n')) != NULL ) {
					*p = 0;
					start = ++p;
				}
				if( (strlen(start) + len) <= max )
					break;
			}
			if( (f = fopen(filename, "w")) == NULL ) {
				free(local);
				return -1;
			}
			if( strlen(start) > 0 ) {
				if( fwrite(start, 1, strlen(start), f) != strlen(start) ) {
					fclose(f);
					free(local);
					return -1;
				}
			}
			fclose(f);
			free(local);
		}
	}

	/* Adiciona os novos bytes no final do arquivo */
	if( (f = fopen(filename, "a")) == NULL )
		return -1;
	if( fwrite(buf, 1, len, f) != len ) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

/* Busca o valor de uma opcao de um programa iniciado pelo init.
 * Linha no inittab:
 *   u1:3:respawn:/sbin/syslogd -n -b 10 -L -R 192.168.1.1
 *
 * A chamada:
 *   init_program_get_option_value("syslogd", "-R", buf, 32)
 * vai copiar para a regiao de memoria apontada por buf nao mais do que
 * 32 bytes. O conteudo copiado serah "182.168.1.1".
 */
int init_program_get_option_value(char *prog, char *option, char *store, unsigned int max_len)
{
	FILE *f;
	char buf[128];
	unsigned int i, ok;
	int n_args, ret = -1;
	arg_list argl = NULL, argl2 = NULL;

	if( (prog == NULL) || (option == NULL) || (store == NULL) || (max_len == 0) )
		return -1;
	store[0] = 0;
	if( (f = fopen(FILE_INITTAB, "r+")) == NULL ) {
		pr_error(1, "could not open %s", FILE_INITTAB);
		return -1;
	}
	while( !feof(f) ) {
		if( fgets(buf, 127, f) == 0 )
			break;
		buf[127] = 0;
		if( strstr(buf, prog) != NULL ) {
			ok = 1;
			if( ((n_args = parse_args_din(buf, &argl)) > 0) && (parse_args_din(prog, &argl2) > 0) ) {
				if( strstr(argl[0], argl2[0]) == NULL )
					ok = 0;
			}
			free_args_din(&argl2);
			if( ok == 1 ) {
				if( (n_args < 3) || (parse_args_din(option, &argl2) != 1) )
					goto error;
				for( i=0; i < n_args; i++ ) {
					if( strcmp(argl[i], argl2[0]) == 0 )
						break;
				}
				if( i < (n_args - 1) ) {
					strncpy(store, argl[i+1], max_len-1);
					store[max_len-1] = 0;
					ret = 0;
				}
error:
				free_args_din(&argl);
				free_args_din(&argl2);
				fclose(f);
				return ret;
			}
			free_args_din(&argl);
		}
	}
	fclose(f);
	return -1;
}

/* Adiciona/remove uma opcao de um programa iniciado pelo init. */
int init_program_change_option(int add_del, char *prog, char *option)
{
	FILE *f;
	int n_args, opt_args;
	char buf[128], new_line[128];
	unsigned int i, j, ok, found = 0;
	arg_list argl = NULL, argl2 = NULL;

	if( (prog == NULL) || (option == NULL) )
		return -1;
	if( (f = fopen(FILE_INITTAB, "r+")) == NULL ) {
		pr_error(1, "could not open %s", FILE_INITTAB);
		return -1;
	}
	while( !feof(f) ) {
		if( fgets(buf, 127, f) == 0 )
			break;
		buf[127] = 0;
		if( strstr(buf, prog) != NULL ) {
			ok = 1;
			if( (parse_args_din(buf, &argl) > 0) && (parse_args_din(prog, &argl2) > 0) ) {
				if( strstr(argl[0], argl2[0]) == NULL )
					ok = 0;
			}
			free_args_din(&argl);
			free_args_din(&argl2);
			if( ok == 1 ) {
				found = 1;
				break;
			}
		}
	}
	fclose(f);
	if( found == 0 )
		return -1;

	if( (n_args = parse_args_din(buf, &argl)) == 0 ) {
		free_args_din(&argl);
		return -1;
	}
	if( (opt_args = parse_args_din(option, &argl2)) == 0 ) {
		free_args_din(&argl);
		free_args_din(&argl2);
		return -1;
	}
	for( i=0, found=0; i <= (n_args - opt_args); i++ ) {
		for( j=0, ok=1; j < opt_args; j++ ) {
			if( strcmp(argl[i+j], argl2[j]) ) {
				ok = 0;
				break;
			}
		}
		if( ok == 1 ) {
			found = 1;
			break;
		}
	}
	if( add_del ) {
		if( found == 1 ) {
			free_args_din(&argl);
			free_args_din(&argl2);
			return 0;
		}
		new_line[0] = 0;
		for( i=0; i < n_args; i++ ) {
			strcat(new_line, argl[i]);
			strcat(new_line, " ");
		}
		for( i=0; i < opt_args; i++ ) {
			strcat(new_line, argl2[i]);
			strcat(new_line, " ");
		}
	}
	else {
		if( found == 0 ) {
			free_args_din(&argl);
			free_args_din(&argl2);
			return 0;
		}
		new_line[0] = 0;
		for( i=0; i < n_args; i++ ) {
			for( j=0, found=1; j < opt_args; j++ ) {
				if( ((i+j) >= n_args) || strcmp(argl[i+j], argl2[j]) ) {
					found = 0;
					break;
				}
			}
			if( found == 0 ) {
				strcat(new_line, argl[i]);
				strcat(new_line, " ");
			}
			else
				i += opt_args;
		}
	}
	free_args_din(&argl);
	free_args_din(&argl2);
	new_line[strlen(new_line) - 1] = (strchr(buf, '\n') != NULL) ? '\n' : 0;
	return replace_exact_string(FILE_INITTAB, buf, new_line);
}

