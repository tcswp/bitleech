#include "torrc.h"

extern struct metainfo *mi;

inline void indent(int i) {int j; for (j = 0; j < i; j++) putchar('\t');};

void parse_dict(struct state *state, char *benc, int *pos, int depth)
{
	dict_t dict_pos;
	char str[MAX_STR];
	char dict_key[64];
	int i, len;

	dict_pos = key;
	(*pos)++;
	
	printf("{");

	while (benc[*pos] != 'e')
	{
		if (benc[*pos] == 'd')
		{
			parse_dict(state, benc, pos, depth+1);
			if (!strcmp("info",dict_key))
				mi->info_end = *pos;
		}
		else if (isdigit(benc[*pos]))
		{
			len = parse_string(benc, pos, str);
			if (dict_pos == key)
			{
				putchar('\n');
				indent(depth);
				printf("%s => ", str);
				
				if (!strcmp("info",str))
					mi->info_begin = *pos;

				strcpy(dict_key, str);
				dict_pos = value;
				continue;
			}
			else
			{
				printf("%s", str);
				fill_struct(state, dict_key, (char *) str, len);
			}
		}
		else if (benc[*pos] == 'i')
		{
			i = parse_int(benc, pos);
			printf("%d", i);
			fill_struct(state, dict_key, (int *) i,0);
		}
		else if (benc[*pos] == 'l')
		{
			parse_list(state, benc, pos, dict_key, depth);
			(*pos)++;
		}

		dict_pos = key;
	}
	
	putchar('\n');
	indent(depth-1);
	printf("}\n");
	(*pos)++;
}

void parse_list(struct state *state, char *benc, int *pos, char *curr_key, int depth)
{
	int len, i;
	char str[MAX_STR];

	(*pos)++;

	printf("[");
 
	while (benc[*pos] != 'e')
	{
		if (isdigit(benc[*pos]))
		{
			len = parse_string(benc, pos, str);
			printf("%s", str);
			fill_struct(state, curr_key, str, len);
		}
		else if (benc[*pos] == 'l')
		{
			parse_list(state, benc, pos, curr_key, depth);
			(*pos)++;
		}
		else if (benc[*pos] == 'i')
		{
			i = parse_int(benc, pos);
			printf(" => %d\n", i);
			fill_struct(state, curr_key, (int *) i,0);
		}
		else if (benc[*pos] == 'd')
		{
			parse_dict(state, benc, pos, depth+1);
		}
	}
	printf("]");
}

int parse_int(char *benc, int *pos)
{
	char intstr[16];
	int i;

	(*pos)++;

	for (i=0; benc[*pos] != 'e'; i++, (*pos)++)
		intstr[i] = benc[*pos];
	intstr[i]='\0';

	(*pos)++;

	return strtoul(intstr, NULL, 10);
}

int parse_string(char *benc, int *pos, char *str)
{
	int i;
	int length;
	char templen[16]; /* longest string length of num */

	for (i=0; benc[*pos] != ':' && isdigit(benc[*pos]); i++, (*pos)++)
		templen[i] = benc[*pos];
	templen[i] = '\0';

	length = atoi(templen);
	str[length] = '\0';

	/* incr pos to move i past ':' */
	for (i=0, (*pos)++; i<length; (*pos)++, i++)
		str[i] = benc[*pos];

	return length;
}