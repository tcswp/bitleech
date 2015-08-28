#define MAX_FILE	0x100000
#define MAX_STR   	0x100000
#define MAX_PIECES	0x10000
#define MAX_MSG		5000
#define MAX_PEERS	60

typedef enum {false, true} bool;

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
  unsigned char pieces[MAX_PIECES*20];
  int			num_pieces;
  unsigned char info_hash[20];
  int 			info_begin;
  int 			info_end;
  char 			name[FILENAME_MAX];
  struct file   file[256];
  int			file_num;
};

struct peer
{
	unsigned int 		ip;
	unsigned short 		port;
	bool 				connected;
	char 				*recv_buffer;
	int 				recv_len;
	int					sock;
	bool				sent_hs;
	time_t				send_time;
	
	unsigned char  		*bitfield;
	bool				recvd_choke;
	bool				sent_choke;
	bool				recvd_interested;
	bool				sent_interested;
	
	int			piece_offset;
	unsigned char *curr_piece;
	int			curr_index;
	int			block_len;
};

struct state
{
  int 			*piece_freq;
  unsigned char *have;

  int			uploaded;
  int			downloaded;
  unsigned long	left;
  char 			event[9];
  struct peer 	peer[MAX_PEERS];
  int			peer_num;
  
  int			qd_requests;
  unsigned char *pending_reqs;
  
  int			interval;
  char			tracker_id[256];
  int			complete;
  int			incomplete;
};

void fill_struct(struct state *state, const char *key, void *value, int len);
int check_hash(unsigned char *piece, int index);
void ntohb(unsigned char *hostorder, unsigned char *netorder, int bytes);
