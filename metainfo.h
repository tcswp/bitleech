#define MAX_FILE	0x100000
#define MAX_PEERS	60

#define piece_len(idx)		(((idx)==num_pieces-1)?last_piece_length:metainfo.piece_length)

struct file
{
  unsigned long length;
  char *path;
};

struct metainfo
{
	unsigned char info_hash[20];
	
	char **announce_list;
	char *creation_date;
	char *comment;
	char *created_by;
	
	int piece_length;
	unsigned char *pieces;
	int private;
	char *name;
	union
	{
		struct file *file;
		int length;
	};
};
