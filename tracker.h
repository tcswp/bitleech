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
	char		*tracker_id;
	int			complete;
	int			incomplete;
	struct peer *peer;
	int			peer_num;
};

void encode_url(char *enc_str, char *str, int len);
int http_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char *hostname, char *path, int sockfd);
// inline long long htonll(long long h);
int udp_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, struct addrinfo *res, int sockfd);
char *announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char **announce_list);
int decode_peers(struct peer **peer, unsigned char *block, int len);
