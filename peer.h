#define PSTR				"BitTorrent protocol"
#define BLOCK_LEN			0x4000

#ifndef PEER_H
#define PEER_H
typedef enum
{
	CHOKE, UNCHOKE, INTERESTED, NOT_INTERESTED,
	HAVE, BITFIELD, REQUEST, PIECE, CANCEL
} type;

typedef enum {false, true} bool;

struct request
{
	int piece;
	struct request *next;
	struct request *prev;
};

struct queue
{
	int				size;
	struct request 	*front;
	struct request	*back;
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
	bool				ready;
	int					downloaded;
	bool				sent_req;
	
	struct queue		queue;
	
	unsigned char  		*bitfield;
	bool				recvd_choke;
	bool				sent_choke;
	bool				recvd_interested;
	bool				sent_interested;
	
	int					piece_offset;
	unsigned char 		*curr_piece;
	int					block_len;
	
	char peer_id[20];
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
  
  unsigned char *pending_reqs;
  unsigned char *requests;
  int			requested;
  int			got;
  int			num_choked;
  int			num_connected;
  
  bool			endmode;
  
  int			interval;
  char			tracker_id[256];
  int			complete;
  int			incomplete;
};

struct handshake
{
	unsigned char pstrlen;
	char pstr[sizeof(PSTR)];
	unsigned long reserved;
	unsigned char info_hash[20];
	char peer_id[20];
};

struct msg_request
{
	int index;
	int begin;
	int length;
};

struct msg_piece
{
	int index;
	int begin;
	//unsigned char block[];
};

struct msg
{
	int length;
	char id;

	union
	{
		int have;
		//unsigned char bitfield[];
		struct msg_request request;
		struct msg_piece piece;
	};
	
} __attribute__((packed));

struct request_item
{
	unsigned char 		data[BLOCK_LEN];
	struct request_item	*next;
};

void start_pwp(struct state *state);
#endif