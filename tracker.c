#include "torrc.h"

void encode_url(char* enc_str, char *str, int len)
{
	int i;
	char fmt[9], encc[4], c;

	for (i = 0; i < len; i++)
	{
		c = str[i];
		strcpy(fmt, (isdigit(c)||isalpha(c)||
			c=='.'||c=='-'||c=='_'||c=='~')
			?"%c":"%%%02hhx");

		sprintf(encc, fmt, c);
		strcat(enc_str, encc);
	}
}

int decode_peers(struct peer **peer, unsigned char *block, int len)
{
	int i, num_peers;
	
	num_peers = MIN(len/6,MAX_PEERS);
	*peer = malloc(num_peers * sizeof(struct peer));
	
	for (i = 0; i < num_peers; i++)
	{
		(*peer)[i].ip   = ntohl(*(long *)(block+i*6));
		(*peer)[i].port = ntohs(*(short *)(block+i*6+4));
		(*peer)[i].flags = 0;
	}
	
	return num_peers;
}

int http_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char *hostname, char *path, char *port, int sockfd)
{	
	char request[MAX_STR], buffer[MAX_STR];
	int response_size;
	char req_params[MAX_STR];

	char peer_id_enc[25] = {0};
	char info_hash_enc[61] = {0};
	char *message_offset;
	
	encode_url(info_hash_enc,(char *) info_hash, 20);
	encode_url(peer_id_enc, PEER_ID, 20);

	sprintf(req_params, "/%s?info_hash=%s&compact=%d&peer_id=%s&port=%d&uploaded=%d&downloaded=%d&left=%d&event=%s"
	,path,info_hash_enc,COMPACT,peer_id_enc,PORT,state->uploaded,state->downloaded,state->left,state->event);
	
	sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", req_params, hostname);

	debug_print("sending announce request to tracker...");
	if (send(sockfd, request, strlen(request), 0) == -1)
	{
		logerr();
		return -1;
	}

	debug_print("receiving announce from tracker...");
	if ((response_size = recv(sockfd, buffer, MAX_STR, 0)) == -1)
	{
		logerr();
		return -1;
	}
	
	message_offset = strstr(buffer, "\r\n\r\n") + 4;
	// subtract header length
	response_size -= (message_offset - buffer);
	// remove http header
	memcpy(buffer, message_offset, response_size);
	
	parse_dict(&set_ares, ares, buffer);
	
	return 0;
}

#define htonll(n)	(((long long)htonl(n))<<32|htonl((long long)(n)>>32))

int udp_announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char *hostname, char *path, char *port, struct addrinfo *res, int sockfd)
{
	int recv_size;

	struct connect_req  creq;
	struct connect_res  cres;
	struct announce_req areq;

	unsigned char response[MAX_RECV] = {0};
	long action;
	long transaction_id;
	
	struct timeval tv;
	
	srand(time(NULL));

	creq.connection_id 	= htonll(0x41727101980);
	creq.action 		= 0;
	creq.transaction_id = htonl(rand());
	
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
	{
		logerr("error setting sock option");
		return -1;
	}

	if (sendto(sockfd, &creq, sizeof (struct connect_req), 0, res->ai_addr, res->ai_addrlen) == -1)
	{
		logerr("error sending");
		return -1;
	}
	debug_print("sent connect request.");

	if ((recv_size = recvfrom(sockfd, &cres, sizeof (struct connect_req), 0, res->ai_addr, &(res->ai_addrlen))) < 1)
	{
		logerr("error receiving");
		return -1;
	}
	debug_print("received connect response.");

	if (recv_size != 16)
	{
		logerr("invalid response from UDP tracker");
		return -1;
	}

	if (creq.transaction_id != cres.transaction_id)
	{
		logerr("UDP tracker sent unexpected transaction value");
		return -1;
	}

	if (cres.action != 0)
	{
		logerr("UDP tracker sent unexpected action.");
		return -1;
	}

	srand(time(NULL));
	areq = (struct announce_req)
	{
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

	memcpy(areq.info_hash,info_hash,20);
	memcpy(areq.peer_id,PEER_ID,20);
	
	if (sendto(sockfd, &areq, sizeof(struct announce_req), 0, res->ai_addr, res->ai_addrlen) == -1)
	{
		logerr("sendto");
		return -1;
	}
	debug_print("sent announce message.");

	if ((recv_size = recvfrom(sockfd, response, MAX_RECV, 0, res->ai_addr, &(res->ai_addrlen))) < 1)
	{
		logerr("recvfrom");
		return -1;
	}
	debug_print("received announce response");
	
	if (recv_size < 20)
	{
		logerr("UDP announce response is under 20 bytes!");
		return -1;
	}

	memcpy(&action, response, 4);
	memcpy(&transaction_id, response+4, 4);

	if (transaction_id != areq.transaction_id)
	{
		logerr("sent transaction id    : %lx", areq.transaction_id);
		logerr("received transaction id: %lx", transaction_id);
		logerr("UDP tracker sent unexpected transaction value.");
		return -1;
	}

	if (ntohl(action) != 1)
	{
		logerr("tracker sent action %lx: ", action);
		logerr("%s", response+8);
		logerr("UDP tracker sent unexpected action");
		return -1;
	}

	ares->interval   = ntohl(*(long *)(response+8));
	ares->complete   = ntohl(*(long *)(response+12));
	ares->incomplete = ntohl(*(long *)(response+16));
	ares->peer_num 	 = decode_peers(&ares->peer, response+20, recv_size-20);
	
	return 0;
}

void announce(unsigned char *info_hash, struct announce_res *ares, struct state *state, char **announce_list)
{
	char tracker_url[MAX_STR];
	char hostname[MAX_STR], proto[5], port[6], path[32];

	int sockfd;
	struct addrinfo hints, *res, *resp;
	int gai_status;
	
	int i;
	
	for (i = 0; announce_list[i] != NULL; i++)
	{	
		strcpy(tracker_url, announce_list[i]);

		if (sscanf(tracker_url, "%*[^:]://%[^:,/]", hostname) != 1)
		{
			logerr("url hostname pattern match failed");
			continue;
		}
		debug_print("hostname is %s", hostname);

		if (sscanf(tracker_url, "%[^:]:", proto) != 1)
		{
			logerr("url protocol pattern match failed");
			continue;
		}
		debug_print("protocol is %s", proto);

		if (sscanf(tracker_url, "%*[^:]://%*[^:]:%[0-9]", port) != 1)
		{
			if (strcmp(proto, "http") == 0)
				strcpy(port, "80");
			else
			{
				logerr("url port pattern match failed");
				continue;
			}
		}
		debug_print("port is %s", port);

		if (sscanf(tracker_url, "%*[^:]://%*[^/]/%[^?]?", path) != 1)
		{
			logerr("url path is messed up? no ...../announce<-");
			continue;
		}
		debug_print("path is %s", path);

		memset(&hints, 0,  sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = !strcmp(proto, "udp") ? SOCK_DGRAM : SOCK_STREAM;

		if ((gai_status = getaddrinfo(hostname, port, &hints, &res)) != 0)
		{
			logerr("%s", gai_strerror(gai_status));
			continue;
		}

		for (resp = res; resp != NULL; resp = res->ai_next)
			if ((sockfd = socket(resp->ai_family, resp->ai_socktype, resp->ai_protocol)) != -1)
			{
				debug_print("found tracker.");
				break;
			}

		if (resp == NULL)
		{
			debug_print("couldn't find tracker \"%s\"", hostname);
			continue;
		}

		if (!strcmp(proto, "udp"))
		{
			debug_print("trying udp tracker...");
			if (udp_announce(info_hash, ares, state, hostname, path, port, res, sockfd) == 0)
				break;
		}
		else if (!strcmp(proto, "http"))
		{
			debug_print("trying http tracker...");

			if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
			{
				logerr("connect()");
				continue;
			}

			if (http_announce(info_hash, ares, state, hostname, path, port, sockfd) == 0)
				break;
		}
		else debug_print("invalid protocol");
	}
	
	freeaddrinfo(res);
}
