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
} type;



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
	
	bool			am_choking;
	bool			peer_choking;
	bool			peer_interested;
	bool			am_interested;
	bool 			connected;
	bool			sent_hs;
	bool			recvd_bitfield;
	bool			sent_req;
};

struct state
{
	int *piece_freq;
	unsigned char *have;
	unsigned char *pending_reqs;
	unsigned char *requests;
	int	requested;
	int	got;
	int	num_choked;
	int	num_connected;
	bool endgame_mode;
	
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

struct metainfo;

int piece_length(struct metainfo *metainfo, int index);
void start_pwp(struct peer *peer, int peer_num, struct state *state, struct metainfo *metainfo);