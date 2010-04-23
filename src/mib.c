#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/shm.h>
#include "defines.h"
#include "args.h"
#include "mib.h"
#include "lock.h"

#undef SNMP_TRANSLATE_DEBUG

struct obj_node *get_snmp_tree_node(char *name, struct obj_node *P_node)
{
	int i;
	struct obj_node *tmp;

	for(i=0; i < P_node->n_sons; i++)
	{
		if(P_node->sons[i]->name)
		{
			if(!strcmp(P_node->sons[i]->name, name))	return P_node->sons[i];
		}
		if(P_node->sons[i]->n_sons)
		{
			if((tmp = get_snmp_tree_node(name, P_node->sons[i])))	return tmp;
		}
	}
	return NULL;
}

static unsigned int recursive_fill(struct obj_node *expand_node, oid *name_buf, int *used_len, int *free_len)
{
	int i, len;
	struct obj_node *P_node;

	if(!expand_node || !name_buf || !used_len || !free_len)	return 0;
	for(P_node=expand_node,len=1; P_node->father; len++)	P_node = P_node->father;
	if(len > *free_len)	return 0;

	for(i=(*used_len+len-1),P_node=expand_node; ; i--)
	{
		name_buf[i] = P_node->sub_oid;
		*used_len += 1;
		*free_len -= 1;
		if(P_node->father)	P_node = P_node->father;
		else			break;
	}
	return 1;
}

static unsigned int do_translation(char *oid_element, oid *name_buf, int *used_len, int *free_len, struct obj_node *top)
{
	char *p;
	int digit;
	struct obj_node *node;

	for(p=oid_element,digit=1; *p; p++)
	{
		if(isdigit(*p) == 0)
		{
			digit = 0;
			break;
		}
	}

	if(digit)
	{
		if(*free_len)
		{
			name_buf[*used_len] = atoi(oid_element);
			*used_len += 1;
			*free_len -= 1;
			return 1;
		}
		else	return 0;
	}
	else
	{
		if((node = get_snmp_tree_node(oid_element, top)))	return recursive_fill(node, name_buf, used_len, free_len);
		else					return 0;
	}
}

static int get_tree_shm_addr(void **shm_init)
{
	int len, shm_confid;
	void *shm_confid_l = (void *) 0;

	if((shm_confid = shmget((key_t) MIBTREE_SHM_KEY, sizeof(unsigned int), 0666)) == -1)
	{
		syslog(LOG_ERR, "Could not access objects tree!");
		return 0;
	}
	if((shm_confid_l = shmat(shm_confid, (void *) 0, 0)) == (void *) -1)
	{
		syslog(LOG_ERR, "Could not link memory for objects tree!");
		return 0;
	}
	len = *((unsigned int *) shm_confid_l);
	shmdt(shm_confid_l);

	if(len)
	{
		if((shm_confid = shmget((key_t) MIBTREE_SHM_KEY, len, 0666)) == -1)
		{
			syslog(LOG_ERR, "Could not access objects tree!");
			return 0;
		}
		if((shm_confid_l = shmat(shm_confid, (void *) 0, 0)) == (void *) -1)
		{
			syslog(LOG_ERR, "Could not link memory for objects tree!");
			return 0;
		}
		*shm_init = shm_confid_l;
		return 1;
	}
	return 0;
}

int snmp_translate_oid(char *oid_str, oid *name, size_t *namelen)
{
	char *p, *local;
	arg_list argl=NULL;
	unsigned int ret=0;
	struct obj_node *top;
	void *shm_init = (void *) 0;
	int i, n, used_len, free_len, load_mib;

	if(*namelen > MAX_OID_LEN)	return 0;
	if((local = strdup(oid_str)))
	{
		while((p=strchr(local, '.')))	*p = ' ';
		if((n = parse_args_din(local, &argl)) > 0)
		{
			for(i=0,load_mib=0; !load_mib && i<n; i++)
			{
				for(p=argl[i]; *p; p++)
				{
					if(isdigit(*p) == 0)
					{
						load_mib++;
						break;
					}
				}
			}
			if(load_mib)
			{
				if(lock_snmp_tree_access())
				{
					if(get_tree_shm_addr(&shm_init))
					{
						adjust_shm_to_static((struct obj_node *) (shm_init + sizeof(unsigned int)), shm_init);
						top = (struct obj_node *) (shm_init + sizeof(unsigned int));
						for(i=0,ret=1,used_len=0,free_len=*namelen; i < n; i++)
						{
							if(!do_translation(argl[i], name, &used_len, &free_len, top))
							{
								for(i=0; i < *namelen; i++)	name[i] = 0;
								ret = 0;
								break;
							}
						}
						if(ret)	*namelen = used_len;
						adjust_shm_to_offset((struct obj_node *) (shm_init + sizeof(unsigned int)), shm_init);
						/* Detach shared memory from this process */
						shmdt(shm_init);
					}
					unlock_snmp_tree_access();
				}
			}
			else
			{
				for(i=0,used_len=0,free_len=*namelen; i < n; i++)
				{
					if(free_len)
					{
						name[used_len] = atoi(argl[i]);
						used_len++;
						free_len--;
					}
				}
				*namelen = used_len;
				ret = 1;
			}
			free_args_din(&argl);
		}
		free(local);
	}
#ifdef SNMP_TRANSLATE_DEBUG
	if(ret)
	{
		char tp[10], result[(MAX_OID_LEN*10) + 6];
		strcpy(result, "oid: ");
		for(i=0; i < *namelen; i++)
		{
			sprintf(tp, "%lu.", name[i]);
			strcat(result, tp);
		}
		*(result + strlen(result) - 1) = '\0';
		printf("%s  oid len: %d\n", result, *namelen);
	}
	else	printf("Error on oid '%s' translation!", oid_str);
#endif
	return ret;
}

void adjust_shm_to_offset(struct obj_node *P_node, void *start_addr)
{
	int i;

	P_node->name = (char *) ((unsigned long) P_node->name - (unsigned long) start_addr);
	if(P_node->father)	P_node->father = (struct obj_node *) ((unsigned long) P_node->father - (unsigned long) start_addr);
	else			P_node->father = NULL;
	if(P_node->n_sons)
	{
		for(i=0; i < P_node->n_sons; i++)
		{
			adjust_shm_to_offset(P_node->sons[i], start_addr);
			P_node->sons[i] = (struct obj_node *) ((unsigned long) P_node->sons[i] - (unsigned long) start_addr);
		}
		P_node->sons = (struct obj_node **) ((unsigned long) P_node->sons - (unsigned long) start_addr);
	}
}

void adjust_shm_to_static(struct obj_node *P_node, void *start_addr)
{
	int i;

	P_node->name = (char *) ((unsigned long) P_node->name + (unsigned long) start_addr);
	if(P_node->father)	P_node->father = (struct obj_node *) ((unsigned long) P_node->father + (unsigned long) start_addr);
	else			P_node->father = NULL;
	if(P_node->n_sons)
	{
		P_node->sons = (struct obj_node **) ((unsigned long) P_node->sons + (unsigned long) start_addr);
		for(i=0; i < P_node->n_sons; i++)
		{
			P_node->sons[i] = (struct obj_node *) ((unsigned long) P_node->sons[i] + (unsigned long) start_addr);
			adjust_shm_to_static(P_node->sons[i], start_addr);
		}
	}
}

int convert_oid_to_string_oid(char *oid_str, char *buf, int max_len)
{
	int i;
	oid name[MAX_OID_LEN];
	size_t namelen=MAX_OID_LEN;
	char tp[10], local[MAX_OID_LEN * 10];

	if(snmp_translate_oid(oid_str, name, &namelen))
	{
		local[0] = '\0';
		for(i=0; i < namelen; i++)
		{
			sprintf(tp, "%lu.", name[i]);
			strcat(local, tp);
		}
		*(local + strlen(local) - 1) = '\0';
		strncpy(buf, local, max_len);
		buf[max_len-1] = '\0';
		return 1;
	}
	return 0;
}

int convert_oid_to_object_name(char *oid_str, char *buf, int max_len)
{
	arg_list argl=NULL;
	struct obj_node *P_node;
	void *shm_init = (void *) 0;
	char *p, *local, *result, tp[10];
	int i, k, n, comp, found, all_decimal, ret=0;

	if((local = strdup(oid_str)))
	{
		while((p=strchr(local, '.')))	*p = ' ';
		if((n = parse_args_din(local, &argl)) > 0)
		{
			for(i=0,all_decimal=1; all_decimal && i<n; i++)
			{
				for(p=argl[i]; *p; p++)
				{
					if(isdigit(*p) == 0)
					{
						all_decimal = 0;
						break;
					}
				}
			}
			if(all_decimal)
			{
				if(lock_snmp_tree_access())
				{
					if(get_tree_shm_addr(&shm_init))
					{
						adjust_shm_to_static((struct obj_node *) (shm_init + sizeof(unsigned int)), shm_init);
						P_node = (struct obj_node *) (shm_init + sizeof(unsigned int));
						if(P_node->sub_oid == atoi(argl[0]))
						{
							for(i=1,ret=1; i < n; i++)
							{
								comp = atoi(argl[i]);
								for(k=0,found=0; k < P_node->n_sons; k++)
								{
									if(P_node->sons[k]->sub_oid == comp)
									{
										P_node = P_node->sons[k];
										found++;
										break;
									}
								}
								if(!found)	break;
							}
							if((result = malloc((MAX_OID_LEN*10) + strlen(P_node->name))))
							{
								strcpy(result, P_node->name);
								for(; i < n; i++)
								{
									sprintf(tp, ".%s", argl[i]);
									strcat(result, tp);
								}
								strncpy(buf, result, max_len);
								buf[max_len-1] = '\0';
								free(result);
							}
							else	ret = 0;
						}
						adjust_shm_to_offset((struct obj_node *) (shm_init + sizeof(unsigned int)), shm_init);
						/* Detach shared memory from this process */
						shmdt(shm_init);
					}
					unlock_snmp_tree_access();
				}
			}
			free_args_din(&argl);
		}
		free(local);
	}
	return ret;
}

void dump_snmp_mibtree_obj(struct obj_node *P_node, int level)
{
	int i;

	if( (P_node == NULL) || (P_node->name == NULL) )
		return;
	if( level == 0 )
		printf(" [%d] %s\n", (unsigned int) P_node->sub_oid, P_node->name);
	else {
		int n = level + 1;
		struct obj_node **list, *node = P_node;

		if( (list = malloc(n * sizeof(struct obj_node *))) != NULL ) {
			for( i=(n-1); i >= 0; i-- ) {
				list[i] = node;
				node = node->father;
			}
			printf("\033[%dC", 2);
			for( i=0; i < (n-2); i++ ) {
				if( (list[i]->n_sons > 1) && (list[i]->sons[list[i]->n_sons - 1] != list[i + 1]) )
					printf("|\033[%dC", 4);
				else
					printf("\033[%dC", 5);
			}
			if( list[n-2]->sons[0] == list[n-1] ) /* Primeiro noh do nivel */
				printf("%s", (list[n-2]->n_sons == 1) ? "'" : "+");
			else if( list[n-2]->sons[list[n-2]->n_sons - 1] == list[n-1] ) /* Ultimo noh do nivel */
				printf("'");
			else /* Noh intermediario dentro do nivel */
				printf("+");
			printf("-- [%d] %s\n", (unsigned int) P_node->sub_oid, P_node->name);
			free(list);
		}
	}
	if( (P_node->n_sons > 0) && (P_node->sons != NULL) ) {
		for( i=0; i < P_node->n_sons; i++ )
			dump_snmp_mibtree_obj(P_node->sons[i], level+1);
	}
}

int dump_snmp_mibtree(void)
{
	int ret = -1;
	struct obj_node *top;
	void *shm_init = (void *) 0;

	if( lock_snmp_tree_access() ) {
		if( get_tree_shm_addr(&shm_init) ) {
			adjust_shm_to_static((struct obj_node *) (shm_init + sizeof(unsigned int)), shm_init);
			top = (struct obj_node *) (shm_init + sizeof(unsigned int));
			/* Loop */
			dump_snmp_mibtree_obj(top, 0);
			adjust_shm_to_offset((struct obj_node *) (shm_init + sizeof(unsigned int)), shm_init);
			/* Detach shared memory from this process */
			shmdt(shm_init);
			ret = 0;
		}
		unlock_snmp_tree_access();
	}
	return ret;
}

