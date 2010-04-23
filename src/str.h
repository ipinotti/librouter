char *find_substr(char *buf, char *key);
int replace_string_in_file_nl(char *filename, char *key, char *value);
int find_string_in_file_nl(char *filename, char *key, char *buffer, int len);
void striplf(char *string);
int is_empty(char *string);
int replace_exact_string(char *filename, char *key, char *value);
unsigned int readPassword(int echo_on, char *store, unsigned int max_len);

