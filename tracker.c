#include "torrc.h"

extern struct metainfo *mi;

void err(char *msg)
{
	perror(msg);
	exit(1);
}

void encode_url(char* enc_str, char *str, int len)
{
	int i;
	char c[2] = {0}, hex[10] = {0};

	for (i = 0; i < len; i++)
	{
		if (isdigit(str[i]) || isalpha(str[i])
			 || str[i] == '.' || str[i] == '-'
			 || str[i] == '_' || str[i] == '~')
		{
			sprintf(c, "%c", str[i]);
			strcat(enc_str, c);
		}
		else
		{
			sprintf(hex, "%%%02hhx", str[i]);
			strcat(enc_str, hex);
		}
	}
}

void decode_peers(struct state *s, unsigned char *block, int len)
{
	int i;
	s->peer_num = (len/6>MAX_PEERS)?MAX_PEERS:len/6;
	for (i = 0; i < s->peer_num; i++)
	{
		s->peer[i].ip   = ntohl(*(long *)(block+i*6));
		s->peer[i].port = ntohs(*(short *)(block+i*6+4));
	}
}

int http_announce(struct state *state, char *hostname, char *path, char *port, int sockfd)
{	
	char request[MAX_STR], buffer[MAX_STR];
	int response_size;
	char req_params[MAX_STR];

	char peer_id_enc[25] = {0};
	char info_hash_enc[61] = {0};
	char *message_offset;

	int pos = 0;
	
	encode_url(info_hash_enc,(char *) mi->info_hash, 20);
	encode_url(peer_id_enc, PEER_ID, 20);
	strcpy(state->event, "started");

	sprintf(req_params, "/%s?info_hash=%s&compact=%d&peer_id=%s&port=%d&uploaded=%d&downloaded=%d&left=%lu&event=%s"
	,path,info_hash_enc,COMPACT,peer_id_enc,PORT,state->uploaded,state->downloaded,state->left,state->event);
	
	sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", req_params, hostname);

	printf("sending announce request to tracker...\n");
	if (send(sockfd, request, strlen(request), 0) == -1)
		err("error sending announce request to tracker");

	printf("receiving announce from tracker...\n");
	if ((response_size = recv(sockfd, buffer, MAX_STR, 0)) == -1)
		err("error receiving announce from tracker");
	
	message_offset = strstr(buffer, "\r\n\r\n") + 4;
	// subtract header length
	response_size -= (message_offset - buffer);
	// remove http header
	memcpy(buffer, message_offset, response_size);
	
	parse_dict(state, buffer, &pos, 1);
	
	return 0;
}

inline long long htonll(long long h){return (long long) htonl(h)<<32|htonl(h>>32);}
inline long long ntohll(long long n){return (long long) ntohl(n)<<32|ntohl(n>>32);}

int udp_announce(struct state *state, char *hostname, char *path, char *port, struct addrinfo *res, int sockfd)
{
	int recv_size;

	struct connect_req  creq;
	struct connect_res  cres;
	struct announce_req areq;

	unsigned char ares[MAX_RECV] = {0};
	long action;
	long transaction_id;
	
	srand(time(NULL));

	creq = (struct connect_req) {
		.connection_id 	= htonll(0x41727101980),
		.action 		= 0,
		.transaction_id = htonl(rand())
	};

	if (sendto(sockfd, &creq, sizeof (struct connect_req), 0, res->ai_addr, res->ai_addrlen) == -1)
		err("error sending UDP connect message");
	printf("sent connect request.\n");

	if ((recv_size = recvfrom(sockfd, &cres, sizeof (struct connect_req), 0, res->ai_addr, &(res->ai_addrlen))) < 1)
		err("error receiving UDP connect response");
	printf("received connect response.\n");

	if (recv_size != 16)
		err("invalid response from UDP tracker");

	if (creq.transaction_id != cres.transaction_id)
		err("UDP tracker sent unexpected transaction value. exiting...");

	if (cres.action != 0)
		err("UDP tracker sent unexpected action. exiting...");

	srand(time(NULL));
	areq = (struct announce_req) {
		.connection_id  = cres.connection_id,
		.action 		= htonl(1),
		.transaction_id = htonl(rand()),
		.downloaded		= htonll(state->downloaded),
		.left			= htonll(state->left),
		.uploaded		= htonll(state->uploaded),
		.event			= 0,
		.ip_addr		= 0,
		.key			= 0,
		.num_want		= -1,
		.port			= htons(PORT)
	};

	memcpy(areq.info_hash,mi->info_hash,20);
	memcpy(areq.peer_id,PEER_ID,20);
	
	if (sendto(sockfd, &areq, sizeof(struct announce_req), 0, res->ai_addr, res->ai_addrlen) == -1)
		err("error sending UDP announce message");
	printf("sent announce message.\n");

	if ((recv_size = recvfrom(sockfd, ares, MAX_RECV, 0, res->ai_addr, &(res->ai_addrlen))) < 1)
		err("error receiving UDP announce response");

	if (recv_size < 20)
		err("UDP announce response is under 20 bytes!");

	memcpy(&action, ares, 4);
	memcpy(&transaction_id, ares+4, 4);

	if (transaction_id != areq.transaction_id)
	{
		printf("sent transaction id    : %lx\n", areq.transaction_id);
		printf("received transaction id: %lx\n", transaction_id);
		err("UDP tracker sent unexpected transaction value. exiting...");
	}

	if (ntohl(action) != 1)
	{
		printf("tracker sent action %lx: ", action);
		printf("%s\n", ares+8);
		err("UDP tracker sent unexpected action");
	}

	state->interval   = ntohl(*(long *)(ares+8));
	state->complete   = ntohl(*(long *)(ares+12));
	state->incomplete = ntohl(*(long *)(ares+16));

	decode_peers(state, ares+20, recv_size-20);
	
	return 0;
}

void announce(struct state *state)
{
	char tracker_url[MAX_STR];
	char hostname[MAX_STR], proto[5], port[6], path[32];

	int sockfd;
	struct addrinfo hints, *res, *resp;
	int gai_status;
	
	int i;
	
	for (i = 0; mi->announce_list[i] != NULL; i++)
	{
		putchar('\n');
	
		strcpy(tracker_url, mi->announce_list[i]);

		if (sscanf(tracker_url, "%*[^:]://%[^:,/]", hostname) != 1)
			err("url hostname pattern match failed");
		printf("hostname is %s\n", hostname);

		if (sscanf(tracker_url, "%[^:]:", proto) != 1)
			err("url protocol pattern match failed");
		printf("protocol is %s\n", proto);

		if (sscanf(tracker_url, "%*[^:]://%*[^:]:%[0-9]", port) != 1)
		{
			if (strcmp(proto, "http") == 0)
				strcpy(port, "80");
			else
				err("url port pattern match failed");
		}
		printf("port is %s\n", port);

		if (sscanf(tracker_url, "%*[^:]://%*[^/]/%[^?]?", path) != 1)
			err("url path is messed up? no ...../announce<-");
		printf("path is %s\n", path);

		memset(&hints, 0,  sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = !strcmp(proto, "udp") ? SOCK_DGRAM : SOCK_STREAM;

		if ((gai_status = getaddrinfo(hostname, port, &hints, &res)) != 0)
		{
			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(gai_status));
			exit(1);
		}

		for (resp = res; resp != NULL; resp = res->ai_next)
			if ((sockfd = socket(resp->ai_family, resp->ai_socktype, resp->ai_protocol)) != -1)
			{
				printf("found tracker.\n");
				break;
			}

		if (resp == NULL)
			err("trackers are not available");

		if (!strcmp(proto, "udp"))
		{
			printf("trying udp tracker...\n");
			if (udp_announce(state, hostname, path, port, res, sockfd) == 0)
				break;
		}
		else if (!strcmp(proto, "http"))
		{
			printf("trying http tracker...\n");

			if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
				err("can't connect to tracker.");

			if (http_announce(state, hostname, path, port, sockfd) == 0)
				break;
		}
		else err("invalid protocol");
	}

	freeaddrinfo(res);
}