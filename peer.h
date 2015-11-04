#define PSTR		"BitTorrent protocol"
#define BLOCK_LEN	0x4000
#define PEER_TIMEOUT 30
#define MAX_TIMEOUT 5

#define RECV_MAX	3000

typedef enum
{
	CHOKE, UNCHOKE,
	INTERESTED, NOT_INTERESTED,
	HAVE, BITFIELD,
	REQUEST, PIECE,
	CANCEL, PORT
} t_msg_id;

typedef enum
{
	AM_CHOKING 		= 1,
	PEER_CHOKING 	= 2,
	PEER_INTERESTED = 4,
	AM_INTERESTED	= 8,
	CONNECTED 		= 16,
	SENT_HS 		= 32,
	RECVD_BITFIELD 	= 64,
	SENT_REQ 		= 128,
	ENDGAME_MODE	= 256
} t_peer_flags;



struct queue;

struct peer
{
	unsigned int 	ip;
	unsigned short 	port;
	char 			peer_id[20];
	time_t			send_time;
	int				pieces_downloaded;
	int				blocks_downloaded;
	int				timeouts;
	struct queue	queue;
	unsigned char  	*bitfield;
	
	t_peer_flags	flags;
};

struct state
{
	unsigned char *piece_freq;
	unsigned char *have;
	unsigned char *pending_reqs;
	unsigned char *requests;
	int	requested;
	int	got;
	int	num_choked;
	int	num_connected;
	
	int	uploaded;
	int	downloaded;
	int	left;
	char event[9];
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
	unsigned char *block;
};

struct msg
{
	int length;
	char id;

	union
	{
		int have;
		struct msg_request request;
		struct msg_piece piece;
		unsigned char *bitfield;
	};
	
} __attribute__((packed));

void start_pwp(struct peer *peer, int peer_num, struct state *state);