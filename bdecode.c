#include "torrc.h"

static inline void indent(int i) {int j; for (j = 0; j < i; j++) putchar('\t');};

int parse_int(char *benc, int *n)
{
	char intstr[16];
	int i;

	for (i=1; benc[i] != 'e'; i++)
		intstr[i-1] = benc[i];
	intstr[i]='\0';

	*n = strtoul(intstr, NULL, 10);
	return i+1;
}

int parse_string(char *benc, char *str, int *len)
{
	int i, j;
	int length;
	char templen[16]; /* longest string length of num */

	for (i=0; benc[i] != ':' && benc[i] >= '0' && benc[i] <= '9'; i++)
		templen[i] = benc[i];
	templen[i] = '\0';

	length = atoi(templen);
	str[length] = '\0';

	for (j=0; j<length; j++)
		str[j] = benc[j+i+1];

	*len = length;
	return length+i+1;
}

int parse_list(void (*strct_fill_callback)(void *strct, const char *key, void *value, int len), void *strct, char *benc, char *key)
{
	int offset = 1;
	char c = benc[0];

	char *str = malloc(MAX_STR);
	int n, len;

	printf("[");

	while (c != 'e')
	{
		c = benc[offset];
		if (c == 'd')
		{
			offset+=parse_dict(strct_fill_callback, strct, benc+offset);
		}
		else if (c == 'l')
		{
			offset+=parse_list(strct_fill_callback, strct, benc+offset, key);
		}
		else if (c == 'i')
		{
			offset+=parse_int(benc+offset, &n);
			printf(" => %lu\n", n);
			strct_fill_callback(strct, key, (int *) n, 0);
		}
		else if (c >= '0' && c <= '9')
		{
			offset+=parse_string(benc+offset, str, &len);
			printf("%s", str);
			strct_fill_callback(strct, key, str, len);
		}
	}

	printf("]");
	
	//free(str);

	return offset+1;
}

int parse_dict(void (*strct_fill_callback)(void *strct, const char *key, void *value, int len), void *strct, char *benc)
{
	int offset = 1;
	static int depth;
	dict_t keyval = key;
	char c = benc[0];

	char *str = malloc(MAX_STR);
	char dict_key[64];
	int n, len;
	
	printf("{");
	depth++;

	while (c != 'e')
	{
		c = benc[offset];
		if (c == 'd')
		{
			offset+=parse_dict(strct_fill_callback, strct, benc+offset);
			if (!strcmp("info",dict_key))
				((struct metainfo *) strct)->info_end = offset;
		}
		else if (c == 'l')
		{
			offset+=parse_list(strct_fill_callback, strct, benc+offset, dict_key);
		}
		else if (c == 'i')
		{
			offset+=parse_int(benc+offset, &n);
			printf("%lu", n);
			strct_fill_callback(strct, dict_key, (int *) n, 0);
		}
		else if (c >= '0' && c <= '9')
		{
			offset+=parse_string(benc+offset, str, &len);
			if (keyval == key)
			{
				putchar('\n');
				indent(depth);
				printf("%s => ", str);
				
				if (!strcmp("info",str))
					((struct metainfo *) strct)->info_begin = offset;
				if (!strcmp("files",str))
				((struct metainfo *) strct)->multi_file_mode = true;
				
				strcpy(dict_key, str);
				keyval = value;
				continue;
			}
			else
			{
				printf("%s", str);
				strct_fill_callback(strct, dict_key, str, len);
			}
		}
	
		keyval = key;
	}

	depth--;
	putchar('\n');
	indent(depth);
	printf("}\n");

	//free(str);
	
	return offset+1;
}