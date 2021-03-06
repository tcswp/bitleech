typedef enum {key, value} dict_t;

void set_metainfo(void *strct, const char *key, void *value, ...);
void set_ares(void *strct, const char *key, void *value, ...);
int parse_dict(void (*strct_fill_callback)(void *strct, const char *key, void *value, ...), void *strct, char *benc);
int parse_list(void (*strct_fill_callback)(void *strct, const char *key, void *value, ...), void *strct, char *benc, char *key);
int parse_int(char *benc, int *n);
int parse_string(char *benc, char **str, int *len);
