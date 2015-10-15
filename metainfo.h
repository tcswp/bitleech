#define MAX_FILE	0x100000
#define MAX_STR   	0x100000
#define MAX_PIECES	0x10000
#define MAX_MSG		5000
#define MAX_PEERS	60
#define QUEUE_LEN	10

struct file
{
  unsigned long length;
  char 			path[FILENAME_MAX];
};

struct metainfo
{
  char 			announce_list[256][256];
  int			announce_num;
  int 			piece_length;
  int			last_piece_length;
  unsigned char pieces[MAX_PIECES*20];
  int			num_pieces;
  unsigned char info_hash[20];
  int 			info_begin;
  int 			info_end;
  bool			multi_file_mode;
  char 			name[FILENAME_MAX];
  int			length;
  struct file   file[256];
  int			file_num;
};

void set_metainfo(void *strct, const char *key, void *value, int len);
void set_ares(void *strct, const char *key, void *value, int len);
int check_hash(unsigned char *piece, int index, struct metainfo *metainfo);