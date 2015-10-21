#define PSTR		"BitTorrent protocol"
#define BLOCK_LEN	0x4000
#define PEER_TIMEOUT 10
#define MAX_TIMEOUT 5

typedef enum
{
	CHOKE, UNCHOKE, INTERESTED, NOT_INTERESTED,
	HAVE, BITFIELD, REQUEST, PIECE, CANCEL
} type;


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
	bool endmode;
	
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