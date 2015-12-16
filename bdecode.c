#include "torrc.h"

int info_begin, info_end;
int announce_num = 0, file_num = 0;
int multi_file_mode;

#ifdef DBG
int depth = 0;
int d;
#define indent(depth) for (d = 0; d < depth; d++) putchar('\t')
#endif

/* replace manual parsing with sscanfs */

int parse_int(char *benc, int *n)
{
	char intstr[16];
	int i;

	for (i=1; benc[i] != 'e'; i++)
		intstr[i-1] = benc[i];
	intstr[i-1]='\0';

	*n = strtoul(intstr, NULL, 10);
	return i+1;
}

int parse_string(char *benc, char **str, int *len)
{
	int i;
	int length;
	char templen[16]; /* longest string length of num */

	for (i=0; benc[i] != ':'; i++)
		templen[i] = benc[i];
	templen[i] = '\0';

	length = atoi(templen);
	
	*str = malloc(length+1);
	memcpy(*str, benc+i+1, length);
	(*str)[length] = '\0';
	
	*len = length;
	return length+i+1;
}

int parse_list(void (*strct_fill_callback)(void *strct, const char *key, void *value, int len), void *strct, char *benc, char *key)
{
	int offset = 1;
	char c = benc[0];

	char *str;
	char *path, *tmp_path;
	
	int path_len;
	
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
			printf(" => %u\n", n);
#endif
			strct_fill_callback(strct, key, (int *) n, 0);
		}
		else if (isdigit(c))
		{
			if (!strcmp(key, "path"))
			{
				path_len = 0;
				path = NULL;
				while (c != 'e')
				{
					offset+=parse_string(benc+offset, &str, &len);
					if (path) strcat(path, "/");
					path_len+=(len+1);
					tmp_path = realloc(path, path_len);
					if (tmp_path == -1)
					{
						errexit("baaad realloc\n");
					}
					if (path == NULL && *tmp_path != '\0')
						*tmp_path = '\0';
					path = tmp_path;
					strcat(path, str);
					free(str);
					c = benc[offset];
				}
#ifdef DBG
				printf("%s", path);
#endif
				strct_fill_callback(strct, key, path, path_len);
			}
			else
			{
				offset+=parse_string(benc+offset, &str, &len);
#ifdef DBG
				printf("%s", str);
#endif
				strct_fill_callback(strct, key, str, len);
			}
		}
	}
#ifdef DBG
	printf("]");
#endif

	return offset+1;
}

int parse_dict(void (*strct_fill_callback)(void *strct, const char *key, void *value, int len), void *strct, char *benc)
{
	int offset = 1;
	dict_t keyval = key;
	char c = benc[0];

	char *str;
	char dict_key[64];
	int n, len;
#ifdef DBG
	indent(depth);
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
				info_end = offset;
		}
		else if (c == 'l')
		{
			offset+=parse_list(strct_fill_callback, strct, benc+offset, dict_key);
		}
		else if (c == 'i')
		{
			offset+=parse_int(benc+offset, &n);
#ifdef DBG
			printf("%u", n);
#endif
			strct_fill_callback(strct, dict_key, (int *) n, 0);
		}
		else if (isdigit(c))
		{
			offset+=parse_string(benc+offset, &str, &len);
			if (keyval == key)
			{
#ifdef DBG
				putchar('\n');
				indent(depth);
				printf("%s => ", str);
#endif				
				if (!strcmp("info",str))
					info_begin = offset;
				
				if (!strcmp("files",str))
					multi_file_mode = 1;
				
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
	
	return offset+1;
}

void set_metainfo(void *strct, const char *key, void *value, int len)
{	
	struct metainfo *metainfo = (struct metainfo *) strct;

	if (!strcmp(key,"announce") || !strcmp(key,"announce-list"))
	{
		announce_num++;
		if ((metainfo->announce_list = realloc(metainfo->announce_list, (announce_num + 1) * sizeof (char *))) == NULL)
		{
			errexit("ran out of memory realloc");
		}
		
		metainfo->announce_list[announce_num-1] = (char *)value;
		metainfo->announce_list[announce_num] = NULL;
	}
	else if (!strcmp(key,"length"))
	{		
		if (multi_file_mode)
		{
			if ((metainfo->file = realloc(metainfo->file, (file_num + 1) * sizeof (struct file))) == NULL)
			{
				errexit("ran out of memory realloc");
			}
			(metainfo->file[file_num]).length = (int) value;
		}
		else
			metainfo->length = (int) value;
	}
	else if (!strcmp(key,"path"))
		(metainfo->file[file_num++]).path = (char *)value;
	else if (!strcmp(key,"piece length"))
		metainfo->piece_length = (int) value;
	else if (!strcmp(key,"pieces"))
		metainfo->pieces = (unsigned char *)value;
	else if (!strcmp(key,"name"))
		metainfo->name = (char *)value;
}

void set_ares(void *strct, const char *key, void *value, int len)
{
	struct announce_res *ares = (struct announce_res *) strct;
	
	if (!strcmp(key,"interval"))
		ares->interval = (int) value;
	else if (!strcmp(key,"tracker_id"))
		ares->tracker_id = strndup((char *)value, len);
	else if (!strcmp(key,"complete"))
		ares->complete = (int) value;
	else if (!strcmp(key,"incomplete"))
		ares->incomplete = (int) value;
	else if (!strcmp(key,"peers"))
		ares->peer_num = decode_peers(&ares->peer, (unsigned char*)value, len);
}