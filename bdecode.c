#include "torrc.h"

#ifdef DBG
int depth, d;
#define indent(d) for (d = 0; d < depth; d++) putchar('\t')
#endif

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

	for (i=0; benc[i] != ':'; i++)
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
#ifdef DBG
	printf("[");
#endif
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
#ifdef DBG
			printf(" => %lu\n", n);
#endif
			strct_fill_callback(strct, key, (int *) n, 0);
		}
		else if (isdigit(c))
		{
			offset+=parse_string(benc+offset, str, &len);
#ifdef DBG
			printf("%s", str);
#endif
			strct_fill_callback(strct, key, str, len);
		}
	}
#ifdef DBG
	printf("]");
#endif
	free(str);

	return offset+1;
}

int parse_dict(void (*strct_fill_callback)(void *strct, const char *key, void *value, int len), void *strct, char *benc)
{
	int offset = 1;
	dict_t keyval = key;
	char c = benc[0];

	char *str = malloc(MAX_STR);
	char dict_key[64];
	int n, len;
#ifdef DBG
	printf("{");
	depth++;
#endif
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
#ifdef DBG
			printf("%lu", n);
#endif
			strct_fill_callback(strct, dict_key, (int *) n, 0);
		}
		else if (isdigit(c))
		{
			offset+=parse_string(benc+offset, str, &len);
			if (keyval == key)
			{
#ifdef DBG
				putchar('\n');
				indent(depth);
				printf("%s => ", str);
#endif				
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
#ifdef DBG
				printf("%s", str);
#endif
				strct_fill_callback(strct, dict_key, str, len);
			}
		}
	
		keyval = key;
	}
#ifdef DBG
	depth--;
	putchar('\n');
	indent(depth);
	printf("}\n");
#endif
	free(str);
	
	return offset+1;
}

void set_metainfo(void *strct, const char *key, void *value, int len)
{	
	struct metainfo *metainfo = (struct metainfo *) strct;
	
	char *str;
	if (len)
	{
		str = malloc(len+1);
		memcpy(str, value, len);
		str[len] = '\0';
	}

	if (!strcmp(key,"announce") || !strcmp(key,"announce-list"))
		metainfo->announce_list[metainfo->announce_num++] = str;
	else if (!strcmp(key,"length")) 		 	 
	{
		if (metainfo->multi_file_mode)
			(metainfo->file[metainfo->file_num]).length = (int) value;
		else
			metainfo->length = (int) value;
	}
	else if (!strcmp(key,"path"))
		(metainfo->file[metainfo->file_num++]).path = str;
	else if (!strcmp(key,"piece length"))
		metainfo->piece_length = (int) value;
	else if (!strcmp(key,"pieces"))
	{
		metainfo->pieces = str;
		metainfo->num_pieces = len/20;
		metainfo->last_piece_length = metainfo->length-((metainfo->num_pieces-1)*metainfo->piece_length);
		metainfo->last_block_length = metainfo->last_piece_length % BLOCK_LEN;
	}
	else if (!strcmp(key,"name"))
		metainfo->name = str;
}

void set_ares(void *strct, const char *key, void *value, int len)
{
	struct announce_res *ares = (struct announce_res *) strct;
	
	if (!strcmp(key,"interval"))
		ares->interval = (int) value;
	else if (!strcmp(key,"tracker_id"))
	{
		ares->tracker_id = malloc(len+1);
		memcpy(ares->tracker_id, value, len);
		ares->tracker_id[len] = '\0';
	}
	else if (!strcmp(key,"complete"))
		ares->complete = (int) value;
	else if (!strcmp(key,"incomplete"))
		ares->incomplete = (int) value;
	else if (!strcmp(key,"peers"))
	{
		ares->peer = decode_peers((unsigned char*)value, len);
		ares->peer = len/6;
	}
}