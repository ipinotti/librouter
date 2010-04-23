#define MAX_NUM_PROCESSES 100
#define MAX_PROC_BASENAME  20

/* Info about a process. */
typedef struct
{
  char basename[MAX_PROC_BASENAME]; /* Only the part after the last / */
  char cmdline[MAX_PROC_CMDLINE];
  pid_t pid;		       /* Process ID.			  */
} PROC;

void invalidate_plist(void);
int read_proc(void);
int get_pid(char *name);

