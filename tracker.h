#define PEER_ID		"-nanoBT-ABCDEFGHIJKL"
#define PORT		12345
#define COMPACT		1
#define MAX_RECV	0x100000

void err(char *msg);
void encode_url(char *enc_str, char *str, int len);
int http_announce(struct state *state, char *hostname, char *path, char *port, int sockfd);
inline long long htonll(long long h);
int udp_announce(struct state *state, char *hostname, char *path, char *port, struct addrinfo *res, int sockfd);
void announce();
void decode_peers(struct state *s, unsigned char *block, int len);

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