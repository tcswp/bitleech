#include "torrc.h"

int check_hash(unsigned char *piece, int index, struct metainfo *metainfo)
{
	return !memcmp(SHA1(piece,piece_length(metainfo, index),NULL),metainfo->pieces+20*index,20);
}

void set_metainfo(void *strct, const char *key, void *value, int len)
{	
	struct metainfo *metainfo = (struct metainfo *) strct;

	if (!strcmp(key,"announce") || !strcmp(key,"announce-list"))
		strcpy(metainfo->announce_list[metainfo->announce_num++], (char *) value);
	else if (!strcmp(key,"length")) 		 	 
	{
		if (metainfo->multi_file_mode)
			(metainfo->file[metainfo->file_num]).length = (int) value;
		else
			metainfo->length = (int) value;
	}
	else if (!strcmp(key,"path"))
		strcpy((metainfo->file[metainfo->file_num++]).path,(char *) value);
	else if (!strcmp(key,"piece length"))
		metainfo->piece_length = (int) value;
	else if (!strcmp(key,"pieces"))
	{
		memcpy(metainfo->pieces, (unsigned char *)value, len);
		metainfo->num_pieces = len/20;
		metainfo->last_piece_length = metainfo->length-((metainfo->num_pieces-1)*metainfo->piece_length);
		metainfo->last_block_length = metainfo->last_piece_length % BLOCK_LEN;
	}
	else if (!strcmp(key,"name"))
		strcpy(metainfo->name, (char *) value);
	else if (!strcmp(key,"info_hash"))
		memcpy(metainfo->info_hash, (unsigned char *) value, 20);
}

void set_ares(void *strct, const char *key, void *value, int len)
{
	struct announce_res *ares = (struct announce_res *) strct;

	if (!strcmp(key,"interval"))
		ares->interval = (int) value;
	else if (!strcmp(key,"tracker_id"))
		memcpy(ares->tracker_id,(char *)value, len);
	else if (!strcmp(key,"complete"))
		ares->complete = (int) value;
	else if (!strcmp(key,"incomplete"))
		ares->incomplete = (int) value;
	else if (!strcmp(key,"peers"))
		ares->peer_num = decode_peers(ares, (unsigned char*)value, len);
}