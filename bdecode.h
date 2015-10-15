typedef enum {key, value} dict_t;

int parse_dict(void (*strct_fill_callback)(void *strct, const char *key, void *value, int len), void *strct, char *benc);
int parse_list(void (*strct_fill_callback)(void *strct, const char *key, void *value, int len), void *strct, char *benc, char *key);
int parse_int(char *benc, int *n);
int parse_string(char *benc, char *str, int *len);
