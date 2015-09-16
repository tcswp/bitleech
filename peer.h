#define PSTR				"BitTorrent protocol"
#define BLOCK_LEN			0x4000

typedef enum
{
	CHOKE, UNCHOKE, INTERESTED, NOT_INTERESTED,
	HAVE, BITFIELD, REQUEST, PIECE, CANCEL
} type;

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
