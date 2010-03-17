#include <linux/config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <libconfig/args.h>

#define isspace_local(c) ((c==' ')||(c=='\t'))

/*
 *  Strip whitespace from the start and end of STRING.
 *  Return a pointer into STRING.
 */
char *stripwhite(char *line)
{
	char *s, *t;

	for(s=line; *s==' ' || *s=='\t'; s++);
	if(*s == 0)
		return s;
	t = s + strlen(s) - 1;
	while(t>s && (*t==' ' || *t=='\t'))
		t--;
	*(++t) = 0;
	return s;
}

inline char *findspace(char *src)
{
	register char *t1;
	register char *t2;

	t1 = strchr(src, ' ');
	t2 = strchr(src, '\t');
	if (t1 && t2)
	{
		if (t1 < t2) t2=NULL;
			else t1=NULL;
	}
	if (t1) return t1;
	return t2;
}

int argcount(const char *string)
{
	int ln;
	int cnt;
	int i;

	cnt = 1;
	ln = strlen(string);
	i = 0;
	while (i < ln)
	{
		if (isspace_local(string[i]))
		{
			while (isspace_local(string[i])) ++i;
			if (string[i]) ++cnt;
		}
		++i;
	}
	return cnt;
}

arglist *make_args(const char *string)
{
	arglist	*result;
	char *rightbound;
	char *word;
	char *crsr;
	int count;
	int pos;

	result=(arglist *)malloc(sizeof(arglist));
	count=argcount(string);
	result->argc=count;
	result->argv=(char **)malloc((count+1) * sizeof(char *));
	crsr=(char *)string;
	pos=0;
	while ((rightbound=findspace(crsr)))
	{
		word=(char *)malloc((rightbound-crsr+3) * sizeof(char));
		memcpy(word, crsr, rightbound-crsr);
		word[rightbound-crsr]=0;
		result->argv[pos++]=word;
		crsr=rightbound;
		while (isspace_local(*crsr)) ++crsr;
	}
	if (*crsr)
	{
		result->argv[pos++]=strdup(crsr);
	}
	result->argv[pos]=NULL;
	return result;
}

void destroy_args(arglist *lst)
{
	int i;

	for(i=0; i<lst->argc; ++i)
		free(lst->argv[i]);
	free (lst->argv);
	free (lst);
}

#ifdef CONFIG_BERLIN_SATROUTER

enum terminal_sequence
{
	TERMSEQ_START=1,
	TERMSEQ_FDIGIT,
	TERMSEQ_SDIGIT,
	TERMSEQ_STOP
};

void test_seq_and_blank(char *start)
{
	int i, state;
	char *stop=NULL, *p=start;
	char *special[] =
	{
		"[H",		// TERM_HOME
		"[2K",		// TERM_CLEARLINE
		"[J",		// TERM_CLEARDOWN
		"[1J",		// TERM_CLEARUP
		"[2J",		// TERM_CLEAR
		"[2D",
		"[0m",		// TERM_RESETATTR
		"[1m",		// TERM_FONTBRIGHT
		"[2m",		// TERM_FONTDIM
		"[4m",		// TERM_FONTUNDERSCORE
		"[5m",		// TERM_FONTBLINK
		"[7m",		// TERM_FONTREVERSE
		"[30m",		// TERM_FONTBLACK
		"[31m",		// TERM_FONTRED
		"[37m",		// TERM_FONTWHITE
		NULL
	};
	
	/* Testa contra lista de sequencias conhecidas */
	for(i=0; special[i]; i++)
	{
		if(strlen(start) >= strlen(special[i]))
		{
			if(!strncmp(start, special[i], strlen(special[i])))
			{
				memset(start, ' ', strlen(special[i]));
				return;
			}
		}
	}
	
	for(state=TERMSEQ_START; state != TERMSEQ_STOP; p++)
	{
		switch(state)
		{
			case TERMSEQ_START:
				if(*p == '[')	state = TERMSEQ_FDIGIT;
				break;
			case TERMSEQ_FDIGIT:
				if(isdigit(*p) == 0)
				{
					if(*p == ';')	state = TERMSEQ_SDIGIT;
					else		return;
				}
				break;
			case TERMSEQ_SDIGIT:
				if(isdigit(*p) == 0)
				{
					if(*p == 'f')
					{
						stop = p;
						state = TERMSEQ_STOP;
					}
					else		return;
				}
				break;
		}
	}
	if(state == TERMSEQ_STOP)
	{
		for(p=start; p <= stop; p++)	*p = ' ';
		return;
	}
}

#endif

int parse_args_din(char *cmd_line, arg_list *rcv_p)
{
	int i, count = 0, n_args = 0;
	char *p, *s, *init, *buf_local, **list, **list_ini;

	if( (cmd_line == NULL) || (strlen(cmd_line) == 0) || (rcv_p == NULL) )
		return 0;
	*rcv_p = NULL;
	if( (buf_local = malloc(strlen(cmd_line)+1)) == NULL )
		return 0;
	strcpy(buf_local, cmd_line);

	/* Elimina todos os caracteres que nao sao texto dentro da linha recebida */
	for( p=buf_local; *p; p++ ) {
		if( isprint(*p) == 0 )
			*p=' ';
	}

#ifdef CONFIG_BERLIN_SATROUTER
	/* Exclui sequencias de terminal */
	for( p=buf_local; *p; p++ ) {
		if( *p == '[' )
			test_seq_and_blank(p);
	}
#endif

	/* Pula eventuais espacos em branco no inicio da linha recebida */
	for( init=buf_local; (*init == ' ') || (*init == '\t') || (*init == '\n'); init++ );
	if( strlen(init) == 0 ) {
		free(buf_local);
		return 0;
	}

	/* Descarta eventuais espacos em branco no final da linha recebida */
	for( p=(buf_local + strlen(buf_local) - 1); *p==' '; p-- )
		*p = 0;
	for( p=init; *p; ) {
		n_args++;
		if( (s=strchr(p, ' ')) != NULL )
			p = s;
		else
			break;
		while( *p == ' ' )
			p++;
	}
	if( (list_ini = malloc((n_args + 1) * sizeof(char *))) == NULL ) {
		free(buf_local);
		return 0;
	}
	for( i=0, p=init, list=list_ini; i < n_args; i++ ) {
		if( i < (n_args-1) ) {
			for( s=p; ; s++ ) {
				if( *s == ' ' ) {
					*s = '\0';
					break;
				}
			}
			if( (*list = malloc(strlen(p)+1)) != NULL ) {
				strcpy(*list, p);
				list++;
				count++;
			}
			p = s+1;
			while( *p == ' ' )
				p++;
		}
		else {
			if( (*list = malloc(strlen(p)+1)) != NULL ) {
				strcpy(*list, p);
				list++;
				count++;
			}
		}
	}
	*list = NULL;
	free(buf_local);
	if( count > 0 )
		*rcv_p = list_ini;
	else
		free(list_ini);
	return count;
}

void free_args_din(arg_list *rcv_buf)
{
	char **list;

	if( rcv_buf ) {
		if( *rcv_buf ) {
			for( list=*rcv_buf; *list; list++ )
				free(*list);
			free(*rcv_buf);
			*rcv_buf = NULL;
		}
	}
}

