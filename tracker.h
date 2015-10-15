#define PEER_ID		"-nanoBT-ABCDEFGHIJKL"
#define PORT		12345
#define COMPACT		1
#define MAX_RECV	0x100000

struct state;

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
	unsigned int 	ip;
	unsigned short 	port;
	bool 			connected;
	unsigned char 	*recv_buffer;
	int 			recv_len;
	int				sock;
	bool			sent_hs;
	time_t			send_time;
	bool			ready;
	int				downloaded;
	bool			sent_req;
	
	struct queue	queue;
	
	unsigned char  	*bitfield;
	bool			recvd_choke;
	bool			sent_choke;
	bool			recvd_interested;
	bool			sent_interested;
	
	int				piece_offset;
	unsigned char 	*curr_piece;
	int				block_len;
	
	char peer_id[20];
};

struct connect_req
{
	long long 		connection_id;
	long 			action;
	long 			transaction_id;
};

struct connect_res
{
	long 			action;
	long 			transaction_id;
	long long 		connection_id;
};

struct announce_req
{
	long long 		connection_id;
	long 			action;
	long 			transaction_id;
	unsigned char 	info_hash[20];
	unsigned char 	peer_id[20];
	long long 		downloaded;
	long long 		left;
	long long 		uploaded;
	long 			event;
	long 			ip_addr;
	long 			key;
	long 			num_want;
	short 			port;
};

struct announce_res
{
	int			interval;
	char		tracker_id[256];
	int			complete;
	int			incomplete;
	struct peer peer[MAX_PEERS];
	int			peer_num;
};

void err(char *msg);
void encode_url(char *enc_str, char *str, int len);
int http_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char *hostname, char *path, char *port, int sockfd);
// inline long long htonll(long long h);
int udp_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char *hostname, char *path, char *port, struct addrinfo *res, int sockfd);
void announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char (*announce_list)[256]);
int decode_peers(struct announce_res *ares, unsigned char *block, int len);