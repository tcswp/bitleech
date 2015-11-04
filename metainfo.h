#define MAX_FILE	0x100000
#define MAX_STR   	0x10000
#define MAX_PIECES	0x10000
#define MAX_MSG		5000
#define MAX_PEERS	60

#define piece_len(index)		(index==metainfo.num_pieces-1)?metainfo.last_piece_length:metainfo.piece_length
#define check_hash(piece,index) !memcmp(SHA1(piece,piece_len(index),NULL),metainfo.pieces+20*index,20)

struct file
{
  unsigned long length;
  char 			*path;
};

struct metainfo
{
  char 			**announce_list;
  int			announce_num;
  int 			piece_length;
  int			last_piece_length;
  int			last_block_length;
  unsigned char *pieces;
  int			num_pieces;
  unsigned char info_hash[20];
  
  // bencoding offsets for calculating info hash
  int 			info_begin;
  int 			info_end;
  
  char 			*name;
  int			length;
  bool			multi_file_mode;
  struct file   *file;
  int			file_num;
};