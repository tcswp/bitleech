#define PEER_ID		"-nanoBT-ABCDEFGHIJKZ"
#define PORT		12345
#define COMPACT		1
#define MAX_RECV	0x100000

#define MAX_STR		0x100000

#define QUEUE_LEN	5

#define MIN(a,b)	((a)<(b)?(a):(b))

struct state;
struct peer;

struct connect_req
{
	uint64_t 		connection_id;
	uint32_t 		action;
	uint32_t 		transaction_id;
};

struct connect_res
{
	uint32_t 			action;
	uint32_t 			transaction_id;
	uint64_t 		connection_id;
};

struct announce_req
{
	uint64_t 		connection_id;
	uint32_t 			action;
	uint32_t 			transaction_id;
	unsigned char 	info_hash[20];
	unsigned char 	peer_id[20];
	uint64_t 		downloaded;
	uint64_t 		left;
	uint64_t 		uploaded;
	uint32_t 			event;
	uint32_t 			ip_addr;
	uint32_t 			key;
	uint32_t 			num_want;
	short 			  port;
};

struct announce_res
{
	int			interval;
	char		*tracker_id;
	int			complete;
	int			incomplete;
	struct peer *peer;
	int			peer_num;
};

void encode_url(char *enc_str, char *str, int len);
int http_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char *hostname, char *path, int sockfd);
// inline uint64_t htonll(uint64_t h);
int udp_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, struct addrinfo *res, int sockfd);
int announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char *tracker_url);
int decode_peers(struct peer **peer, unsigned char *block, int len);
