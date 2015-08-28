#include "torrc.h"

extern struct metainfo *mi;

#define STR_IS(str)	!strcmp(key, str)

void fill_struct(struct state *state, const char *key, void *value, int len)
{	
	//metainfo struct
	if (STR_IS("announce") ||
	    STR_IS("announce-list"))		 strcpy(mi->announce_list[mi->announce_num++], (char *) value);
	else if (STR_IS("length")) 		 	 (mi->file[mi->file_num]).length = (int) value;
	else if (STR_IS("path"))		     strcpy((mi->file[mi->file_num++]).path,(char *) value);
	else if (STR_IS("piece length"))	 mi->piece_length = (int) value;
	else if (STR_IS("pieces"))
	{
		memcpy(mi->pieces, (unsigned char *)value, len);
		mi->num_pieces = len/20;
	}
	else if (STR_IS("name"))	  		 strcpy(mi->name, (char *) value);
	else if (STR_IS("info_hash"))		 memcpy(mi->info_hash, (unsigned char *) value, 20);
	//state struct filled when http tracker responds with bencoding
	else if (STR_IS("interval"))		 state->interval = (int) value;
	else if (STR_IS("tracker_id"))	 	 memcpy(state->tracker_id,(char *)value, len);
	else if (STR_IS("complete"))		 state->complete = (int) value;
	else if (STR_IS("incomplete"))		 state->incomplete = (int) value;
	else if (STR_IS("peers"))			 decode_peers(state, (unsigned char*)value, len);
}

int check_hash(unsigned char *piece, int index)
{
// 	unsigned char *hash, rhash[20];
// 	hash = SHA1(piece,20,NULL);
// 	ntohb(rhash,hash,20);
	return !memcmp(SHA1(piece,mi->piece_length,NULL),mi->pieces+20*index,20);
}