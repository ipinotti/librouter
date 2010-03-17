
void start_default_snmp(void);
unsigned int get_microcom_lock(void);
unsigned int release_microcom_lock(void);
int get_microcom_mode(void);
int set_microcom_mode(unsigned int mode);
unsigned int is_microcom_lock_from_remote(void);
unsigned int get_vcli_pid(void);
int transfer_microcom_lock_to_us(void);
int get_process_fd_device(pid_t pid, int f_d, char *store, unsigned int max_len);
unsigned int get_led_state(char *led_name);
void generate_init_prompt(char *product_ident, char *store, unsigned int max_len);

