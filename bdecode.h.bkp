typedef enum {key, value} dict_t;

void parse_dict(struct state *state, char *benc, int *pos, int depth);
void parse_list(struct state *state, char *benc, int *pos, char *curr_key, int depth);
int  parse_int(char *benc, int *pos);
int  parse_string(char *benc, int *pos, char *str);
