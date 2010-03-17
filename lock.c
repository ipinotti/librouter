#include <unistd.h>
#include <sys/sem.h>
#include <syslog.h>

#include <libconfig/defines.h>

union semun
{
	int val;			/* value for SETVAL */
	struct semid_ds *buf;		/* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;	/* array for GETALL, SETALL */
	struct seminfo *__buf;		/* buffer for IPC_INFO */
};

static int semaphore_state(int sem_id, char action)
{
	struct sembuf sem_b;

	sem_b.sem_num = 0;
	switch( action ) {
		case 'p':
			sem_b.sem_op = -1;
			break;

		case 'v':
			sem_b.sem_op = 1;
			break;
	}
	sem_b.sem_flg = SEM_UNDO;
	return ((semop(sem_id, &sem_b, 1) == -1) ? 0 : 1);
}

static int get_sem_id(unsigned int key, int *sem_id)
{
	union semun sem_union;

	if( (key == 0) || (sem_id == NULL) )
		return 0;
	if( (*sem_id = semget((key_t) key, 1, 0666)) == -1 ) {
		if( (*sem_id = semget((key_t) key, 1, 0666 | IPC_CREAT)) == -1 )
			return 0;
		sem_union.val = 1;
		if( semctl(*sem_id, 0, SETVAL, sem_union) == -1 )
			return 0;
	}
	return 1;
}

int lock_rmon_config_access(void)
{
	int count, sem_id;

	/* Semaphore: make the lock */
	if( get_sem_id(RMON_SEM_KEY, &sem_id) == 0 )
		return 0;
	for( count=0; count < 1000; count++ ) {
		if( semaphore_state(sem_id, 'p') == 1 )
			return 1;
		usleep(1000); /* 1ms */
	}
	return 0;
}

int unlock_rmon_config_access(void)
{
	int count, sem_id;

	/* Semaphore: make unlock */
	if( (sem_id = semget((key_t) RMON_SEM_KEY, 1, 0666)) == -1 )
		return 0;
	/* Se a liberacao do semaforo falhar, temos que tentar mais vezes.
	 * Sair desta rotina e nao liberar o semaforo seria muito ruim.
	 */
	for( count=0; count < 5000; count++ ) {
		if( semaphore_state(sem_id, 'v') == 1 )
			return 1;
		usleep(100);
	}
	syslog(LOG_ERR, "Not possible to unlock semaphore: %d", RMON_SEM_KEY);
	return 0;
}

int lock_snmp_tree_access(void)
{
	int count, sem_id;

	/* Semaphore: make the lock */
	if( get_sem_id(SNMP_TREE_SEM_KEY, &sem_id) == 0 )
		return 0;
	for( count=0; count < 1000; count++ ) {
		if( semaphore_state(sem_id, 'p') == 1 )
			return 1;
		usleep(1000); /* 1ms */
	}
	return 0;
}

int unlock_snmp_tree_access(void)
{
	int count, sem_id;

	/* Semaphore: make unlock */
	if( (sem_id = semget((key_t) SNMP_TREE_SEM_KEY, 1, 0666)) == -1 )
		return 0;
	/* Se a liberacao do semaforo falhar, temos que tentar mais vezes.
	 * Sair desta rotina e nao liberar o semaforo seria muito ruim.
	 */
	for( count=0; count < 5000; count++) {
		if( semaphore_state(sem_id, 'v') == 1 )
			return 1;
		usleep(100);
	}
	syslog(LOG_ERR, "Not possible to unlock semaphore: %d", SNMP_TREE_SEM_KEY);
	return 0;
}

/* Rotina que tenta pegar semaforo.
 *  @wait_time: especifica o tempo maximo de espera para conseguir pegar o semaforo
 * Se o argumento for zero e o semaforo estiver com outro processo, retorna imediatamente.
 */
int lock_del_mod_access(unsigned int wait_time)
{
	int count, sem_id;

	/* Semaphore: make the lock */
	if( get_sem_id(DELETE_MOD_SEM_KEY, &sem_id) == 0 )
		return 0;
	if( wait_time > 0 ) {
		wait_time *= 1000; /* converte para o limite do loop */
		for( count=0; count < wait_time; count++ ) {
			if( semaphore_state(sem_id, 'p') == 1 )
				return 1;
			usleep(1000); /* 1ms */
		}
		return 0;
	}
	return semaphore_state(sem_id, 'p');
}

int unlock_del_mod_access(void)
{
	int count, sem_id;

	/* Semaphore: make unlock */
	if( (sem_id = semget((key_t) DELETE_MOD_SEM_KEY, 1, 0666)) == -1 )
		return 0;
	/* Se a liberacao do semaforo falhar, temos que tentar mais vezes.
	 * Sair desta rotina e nao liberar o semaforo seria muito ruim.
	 */
	for( count=0; count < 5000; count++ ) {
		if( semaphore_state(sem_id, 'v') == 1 )
			return 1;
		usleep(100);
	}
	syslog(LOG_ERR, "Not possible to unlock semaphore: %d", DELETE_MOD_SEM_KEY);
	return 0;
}

