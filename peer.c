#include "torrc.h"

static inline int  get_bit(unsigned char *bf, int i){return (bf[i/8]>>(7-(i%8)))&1;};
static inline void set_bit(unsigned char *bf, int i){bf[i/8]^=(1<<(7-(i%8)));};

// this should be inline
int piece_length(struct metainfo *metainfo, int index)
{
	return ((index==metainfo->num_pieces-1)?metainfo->last_piece_length:metainfo->piece_length);	
}

int rarest_first(struct state *state, unsigned char *bitfield, int num_pieces)
{
	int i;
	int rarest;

	rarest = -1;
	for (i = 0; i < num_pieces; i++)
		if (state->piece_freq[i] && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i) && get_bit(bitfield, i))
		{
			rarest = i;	
			break;
		}

	if (rarest != -1)
		for (i = rarest+1; i < num_pieces; i++)
			if (state->piece_freq[i] && (state->piece_freq[i] < state->piece_freq[rarest]) && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i) && get_bit(bitfield, i))
				rarest = i;

 	return rarest;
}

switch (msg.id)
{
	case 0: 
		//printf("choke\n");
		peer[i].am_choking = true;
		break;
	case 1: 
		//printf("unchoke\n");
		peer[i].am_choking = false;
		break;
	case 2: printf("interested\n"); break;
	case 3: printf("not interested\n"); break;
	case 4: 
		//index = ntohl(*(long *)(id+1));
		//printf("have (Idx:%x)\n", index);
		set_bit(peer[i].bitfield, msg.have);
		if (!peer[i].recvd_bitfield)
			peer[i].recvd_bitfield = true;
		state->piece_freq[msg.have]++;
		break;
	case 5:
		//printf("bitfield\n");
		memcpy(peer[i].bitfield,msg.bitfield,ceil(((float)metainfo->num_pieces)/8));
		
		for (j = 0; j < metainfo->num_pieces; j++)
			state->piece_freq[j] += get_bit(peer[i].bitfield, j);
		peer[i].recvd_bitfield = true;
		break;    
	case 6: printf("request\n"); break;
	case 7:
		//printf("piece\n");
		for (j = 0; j < peer[i].q_size; j++)
			if (peer[i].request[j].index == msg.piece.index)
				break;
		
		if (peer[i].request[j].index != msg.piece.index || get_bit(state->have, msg.piece.index))
		{
			break;
		}
			
		memcpy(peer[i].request[j].piece+msg.piece.begin, msg.piece.block, msg.length-9);
		peer[i].request[j].blocks_downloaded++;
		peer[i].blocks_downloaded++;
		
		if ((peer[i].request[j].blocks_downloaded-1)*BLOCK_LEN+(msg.length-9) == piece_length(metainfo, msg.piece.index))
		{
			set_bit(state->pending_reqs, msg.piece.index);
			
			if (check_hash(peer[i].request[j].piece, msg.piece.index, metainfo))
			{
				state->got++;
				printf("downloaded piece %x from peer %d\n", msg.piece.index, i);
				printf("%.02f%%\b\b\b\b\b\b\b", ((float)state->got*100)/metainfo->num_pieces);
				
				printf("got %d/%d\n", state->got, metainfo->num_pieces);
				printf("requested %d/%d\n", state->requested, metainfo->num_pieces);
				
				fflush(stdout);
				set_bit(state->have, msg.piece.index);
				peer[i].pieces_downloaded++;

				write_to_file(metainfo, peer[i].request[j].piece, msg.piece.index);
				
				// send out haves
			}
			else
			{
				printf("piece %x did not match hash!\n", msg.piece.index);
				break;
			}
		}
		
		if (peer[i].blocks_downloaded == QUEUE_LEN)
		{
			peer[i].sent_req = false;
		}
		
		if (peer[i].q_size == peer[i].pieces_downloaded)
		{
			peer[i].q_size = 0;
			peer[i].blocks_downloaded = 0;
		}
		
		break; 
	case 8: printf("cancel\n"); break;
	case 9: printf("port\n"); break;
	default: printf("unidentified message.\n");
}

int parse_msgs(struct msg **msg, unsigned char *msg_stream, int *left)
{
	int msg_size, m;
	unsigned char *msg_offset;
	
	struct msg msg;

	msg_size = 0;
	m = 0;
	
	msg_offset = msg_stream;
	while (*left >= 4)
	{
		msg_size = ntohl(*(long *)(msg_offset))+4;
		if (msg_size > *left)
			break;

		msg[m]->length = ntohl(*(long *)msg_offset);
		msg[m]->id	   = *((char *)msg_offset+4);
		switch (msg[m]->id)
		{
			case 4:
				msg[m]->have = ntohl(*(long *)(msg_offset+5));
				break;
			case 5:
				msg[m]->bitfield = msg_offset+5;
				break;
			case 7:
				msg[m]->piece.index = ntohl(*(long *)(msg_offset+5));
				msg[m]->piece.begin = ntohl(*(long *)(msg_offset+9));
				msg[m]->piece.block = msg_offset+13;
				break;
		}
			
		msg_offset += msg_size;
		*left -= msg_size;
		m++;
	}
	
	return m;
}

void init_state(struct state *state, int num_pieces)
{
	// need to fetch these values from a save file
	*state = (struct state)
	{
		.uploaded = 0, 
		.downloaded = 0,
		.left  = num_pieces,
		.event = "started"
	};
	
	state->piece_freq 	 = calloc(num_pieces,sizeof (int));
	state->have 	 	 = calloc(ceil(((float)num_pieces)/8),sizeof (unsigned char));
	state->pending_reqs  = calloc(ceil(((float)num_pieces)/8), sizeof (unsigned char));
	state->requests		 = calloc(ceil(((float)num_pieces)/8), sizeof (unsigned char));
	state->requested	 = 0;
	state->got			 = 0;
	state->num_choked	 = 0;
	state->num_connected = 0;
	state->endgame_mode	 = false;
}

void init_peer(struct metainfo *metainfo, struct peer *peer)
{
	int j;
	
	peer->connected = true;
	peer->bitfield = calloc(ceil(((float)metainfo->num_pieces)/8), sizeof(char));
	
	peer->am_choking	= true;
	peer->peer_choking	= true;
	peer->peer_interested = false;
	peer->am_interested = false;
	peer->recvd_bitfield = false;
	peer->sent_req = false;
	peer->pieces_downloaded = 0;
	peer->blocks_downloaded = 0;
	peer->timeouts	= 0;
	
	peer->q_size = 0;
	
	for (j = 0; j < ceil((float)QUEUE_LEN/(metainfo->piece_length/BLOCK_LEN))+1; j++)
	{
		memset(&peer->request[j], 0, sizeof (struct request));
		peer->request[j].piece = malloc(metainfo->piece_length);
	}
}

int make_requests(struct msg_request **req, struct metainfo *metainfo, struct state *state, struct peer *peer)
{
	int i ,j = 0;
	int index;
	int num_pieces;
	int num_blocks;
	int blocks_queued = 0;
	struct req *last_req;
	
	num_pieces = ceil((float)QUEUE_LEN/(metainfo->piece_length/BLOCK_LEN));
	
	if (!is_empty(peer->queue))
	{
		last_req = get_tail(peer->queue);
		if (piece_length(metainfo, last_req->index) != last_req->blocks_queued*BLOCK_LEN)
		{
			num_pieces++;
			blocks_queued = last_req->blocks_queued;
		}
	}
	
	for (i = 0; i < num_pieces; i++)
	{
		if (!blocks_queued)
		{
			index = rarest_first(state, peer->bitfield, metainfo->num_pieces);
			if (index == -1)
				break;
			
			set_bit(state->pending_reqs, index);

			if (!get_bit(state->requests, index))
			{
				state->requested++;
				set_bit(state->requests, index);
			}
			
			insert_piece(peer->queue, index);
		}
		
		//check this to be sure
		if (ceil((float)piece_length(index)/BLOCK_LEN)-blocks_queued > QUEUE_LEN)
			num_blocks = QUEUE_LEN;
		else
			num_blocks = ceil((float)piece_length(index)/BLOCK_LEN)-blocks_queued;
		
		for (j = 0; j < num_blocks; j++)
		{
			req[i+j]->index = index;
			req[i+j]->begin = (j+blocks_queued)*BLOCK_LEN; 
			
			if (index == metainfo->num_pieces-1 && (j+blocks_queued+1)*BLOCK_LEN > metainfo->last_piece_length)
			{
				req[i+j]->length = metainfo->last_piece_length-(j+blocks_queued)*BLOCK_LEN;
			}
			else
				req[i+j]->length = BLOCK_LEN;
		}
	}
	return i+j;
}

int check_handshake(struct handshake *hs, unsigned char *info_hash, struct peer *peer, int i, int peer_num)
{
	int j;

	for (j = 0; j < peer_num; j++)
		if (peer[j].connected && i != j && !strcmp(hs->peer_id,peer[j].peer_id))
			return 0;
	if (!(strcmp(hs->peer_id, PEER_ID) && hs->pstrlen == sizeof (PSTR)-1
		&& !strcmp(hs->pstr, PSTR) && !memcmp(info_hash, hs->info_hash, 20)))
		return 0;
	
	return 1;
}

int send_request(int sockfd, int index, int begin, int length)
{
	int ret;
	
	msg.length			= htonl(13);
	msg.id				= REQUEST;
	msg.request.index	= htonl(index);
	msg.request.begin	= htonl(begin);
	msg.request.length	= htonl(length);
	
	ret = send(sockfd, &msg, 17, 0);
	return ret;
}

int send_have(int sockfd, int index)
{
	int ret;
	
	msg.length = htonl(5);
	msg.id 	   = HAVE;
	msg.have   = htonl(index);
	
	ret = send(peer[p].sock, &msg, 9, 0);
	return ret;
}

int send_interested(int sockfd)
{
	int ret;
	
	msg.length	= htonl(1);
	msg.id		= INTERESTED;
	
	ret = send(sockfd, &msg, 5, 0);
	return ret;
}

int send_handshake(int sockfd, unsigned char *info_hash)
{
	int ret;
	struct handshake hs;
	
	hs = (struct handshake)
	{
		.pstrlen  = 0x13,
		.pstr	  = PSTR,
		.reserved = 0,
		.peer_id  = PEER_ID,
	};
	
	memcpy(&(hs.info_hash), info_hash,20);
	
	ret = send(sockfd, &hs, sizeof(hs), 0);
	return ret;
}

void connect_peers(struct pollfd *peer_fds, struct peer *peer, int peer_num)
{
	int i, sockfd;
	
	struct sockaddr_in peer_addr;
	
	printf("connecting to peers...\n");
	for (i = 0; i < peer_num; i++)
	{
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			err("error opening socket");
		flags = fcntl(sockfd, F_GETFL, 0);
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

		// fill in peer info
		memset(&peer_addr, 0,  sizeof(peer_addr));
		peer_addr.sin_family      = AF_INET;
		peer_addr.sin_addr.s_addr = htonl(peer[i].ip);
		peer_addr.sin_port        = htons(peer[i].port);

		connect(sockfd, (struct sockaddr *) &peer_addr, sizeof (peer_addr));
		
		peer_fds[i]->fd		 = sockfd;
		peer_fds[i]->events  = POLLOUT | POLLIN;
		peer_fds[i]->revents = 0;
	}
}

void start_pwp(struct peer *peer, int peer_num, struct state *state, struct metainfo *metainfo)
{
	int i, j, k;
	
	struct pollfd peer_fds[MAX_PEERS];
	struct handshake, peer_hs;
	struct msg msg;
	
	int recv_size;
	unsigned char recv_buffer[RECV_MAX];
	
	int index, begin, length, flags;

	connect_peers(&peer_fds, peer, peer_num);
	
	signal(SIGPIPE, SIG_IGN);
	while (state->got != metainfo->num_pieces)
	{		
		poll(peer_fds, peer_num, -1);
		
		if (state->requested == metainfo->num_pieces && !state->endgame_mode)
		{
			printf("starting endgame mode\n");
			state->endgame_mode = true;
		}
		
		for (i = 0; i < peer_num; i++)
		{
			if (!peer[i].sent_hs && peer_fds[i].revents & POLLOUT)
			{
				send_handshake(peer_fds[i].fd, metainfo->info_hash);
				peer[i].sent_hs = true;
				continue;
			}
			
			if (peer_fds[i].revents & POLLIN)
			{
				if ((recv_size = recv(peer_fds[i].fd, recv_buffer, RECV_MAX, MSG_PEEK)) < 1)
					continue;
				
				if (!peer[i].connected)
				{
					if (recv_size < sizeof(struct handshake))
						continue;
					
					recv(peer_fds[i].fd, recv_buffer, sizeof (struct handshake), 0);
					memcpy(&peer_hs, recv_buffer, sizeof (struct handshake));
					
					if (check_handshake(&peer_hs, metainfo->info_hash, peer, i, peer_num))
					{
						memcpy(peer[i].peer_id, peer_hs.peer_id, 20);
						init_peer(state, metainfo, &peer[i]);
						
						state->num_connected++;
						printf("connected to %d peers\n", state->num_connected);
					}
					else
					{
						close(peer_fds[i].fd);
						continue;
					}
				}

				left = recv_size;
				parse_msgs(msgs, recv_buffer, &left);
				recv(peer_fds[i].fd, recv_buffer, recv_size-left, 0);
				
				if (peer[i].recvd_bitfield)
				{
					if (peer[i].am_choking && !peer[i].am_interested)
					{
						send_interested(peer_fds[i].fd);
						peer[i].am_interested = true;
						peer[i].send_time = time(NULL);
					}
					
					if (!peer[i].am_choking && peer[i].am_interested && !peer[i].sent_req)
					{
						num_reqs = make_requests();
						for (j = 0; j < num_reqs; j++)
						{
							send_request(peer_fds[i].fd,);
						}
						
						peer[i].send_time = time(NULL);
						peer[i].sent_req  = true;
					}
				}
			}
		}
	}
	printf("done.\n");
}

if ((peer[i].sent_req||peer[i].am_interested) && peer[i].send_time+PEER_TIMEOUT < time(NULL))
{
	for (j = 0; j < peer[i].q_size; j++)
		if (!get_bit(state->have, peer[i].request[j].index))
			set_bit(state->pending_reqs, peer[i].request[j].index);
	
	if (peer[i].timeouts >= MAX_TIMEOUT)
	{
		printf("peer %d timed out\n", i);
		peer[i].am_interested = false;
	}
	else
		peer[i].timeouts++;
	
	peer[i].pieces_downloaded = 0;
	peer[i].blocks_downloaded = 0;
	peer[i].q_size = 0;
	peer[i].sent_req = false;
	continue;
}
