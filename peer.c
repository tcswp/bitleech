#include "torrc.h"

#define get_bit(bf,i) bf[i/8]>>(7-(i%8)))&1
#define set_bit(bf,i) bf[i/8]^=(1<<(7-(i%8)))
#define mask_size(n)  ceil(((float)n)/8)

extern struct metainfo metainfo;

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

int parse_msgs(struct msg *msg, unsigned char *msg_stream, int *left)
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
			case HAVE:
				msg[m]->have = ntohl(*(long *)(msg_offset+5));
				break;
			case BITFIELD:
				msg[m]->bitfield = msg_offset+5;
				break;
			case PIECE:
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

void init_state(struct state *state)
{
	// need to fetch these values from a save file
	*state = (struct state)
	{
		.uploaded = 0, 
		.downloaded = 0,
		.left  = metainfo.num_pieces,
		.event = "started"
	};
	
	state->piece_freq 	 = calloc(metainfo.num_pieces,sizeof (char));
	state->have 	 	 = calloc(mask_size(metainfo.num_pieces), sizeof (char));
	state->pending_reqs  = calloc(mask_size(metainfo.num_pieces), sizeof (char));
	state->requests		 = calloc(mask_size(metainfo.num_pieces), sizeof (char));
	state->requested	 = 0;
	state->got			 = 0;
	state->num_choked	 = 0;
	state->num_connected = 0;
}

void init_peer(struct peer *peer)
{
	peer->bitfield = calloc(mask_size(metainfo.num_pieces), sizeof(char));
	
	peer->flags = CONNECTED | AM_CHOKING | PEER_CHOKING;
	
	peer->pieces_downloaded = 0;
	peer->blocks_downloaded = 0;
	peer->timeouts	= 0;
	
	init_queue(peer->queue);
}

int make_requests(struct msg_request *mreq, struct state *state, struct peer *peer)
{
	int i ,j;
	int index;
	int num_pieces;
	int num_blocks;
	int blocks_queued = 0;
	struct req *req;
	
	//num_pieces = ceil((float)QUEUE_LEN/(metainfo.piece_length/BLOCK_LEN));
	
	if (!is_empty(peer->queue))
	{
		req = get_tail(peer->queue);
		if (ceil((float)piece_length(req->index)/BLOCK_LEN) != req->blocks_queued)
		{
			blocks_queued = req->blocks_queued;
		}
		else
		{
			for (i = 0; i < get_len(peer->queue); i++)
				pop(peer->queue);
		}
	}
	
	for (i = 0; i < QUEUE_LEN; i+=num_blocks)
	{
		if (!blocks_queued)
		{
			index = rarest_first(state, peer->bitfield, metainfo.num_pieces);
			if (index == -1)
				break;
			
			set_bit(state->pending_reqs, index);

			if (!get_bit(state->requests, index))
			{
				state->requested++;
				set_bit(state->requests, index);
			}
			
			req = insert_piece(peer->queue, index);
		}
		else
			index = req->index;
		
		num_blocks = MIN(ceil((float)piece_length(index)/BLOCK_LEN)-blocks_queued,QUEUE_LEN-num_blocks);
		
		for (j = 0; j < num_blocks; j++, req->blocks_queued++)
		{
			mreq[i+j]->index = index;
			mreq[i+j]->begin = (j+blocks_queued)*BLOCK_LEN; 
			
			if (index == metainfo.num_pieces-1 && (j+blocks_queued+1)*BLOCK_LEN > metainfo.last_piece_length)
			{
				mreq[i+j]->length = metainfo.last_block_length;//metainfo.last_piece_length-(j+blocks_queued)*BLOCK_LEN;
			}
			else
				mreq[i+j]->length = BLOCK_LEN;
		}
		blocks_queued = 0;
	}
	return i;
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

void handle_msgs(struct peer *peer, struct msg **msgs, int num_msgs)
{
	int i, j;
	struct msg msg;
	struct req *req;
	
	for (i = 0; i < num_msgs; i++)
	{
		msg = msgs[i];
		switch (msg.id)
		{
			case CHOKE:				peer->flags |= AM_CHOKING; 			break;
			case UNCHOKE:			peer->flags &= ~AM_CHOKING;			break;
			case INTERESTED:		peer->flags |= PEER_INTERESTED;		break;
			case NOT_INTERESTED:	peer->flags &= ~PEER_INTERESTED;	break;
			
			case HAVE: 
				set_bit(peer->bitfield, msg.have);
				if (!(peer->flags & RECVD_BITFIELD))
					peer->flags |= RECVD_BITFIELD;
				state->piece_freq[msg.have]++;
				break;
				
			case BITFIELD:
				memcpy(peer->bitfield, msg.bitfield, mask_size(metainfo.num_pieces));
				for (j = 0; j < metainfo.num_pieces; j++)
					state->piece_freq[j] += get_bit(peer->bitfield, j);
				peer->flags |= RECVD_BITFIELD;;
				break; 
				
			case REQUEST: printf("request\n"); break;
			
			case PIECE:
				
				req = get_req(peer->queue, msg.piece.index);
				if (req == NULL || get_bit(state->have, msg.piece.index)) break;
					
				memcpy(req->piece+msg.piece.begin, msg.piece.block, msg.length-9);
				req->blocks_downloaded++;
				peer->blocks_downloaded++;
				
				if ((req->blocks_downloaded-1)*BLOCK_LEN+(msg.length-9) == piece_length(msg.piece.index))
				{
					set_bit(state->pending_reqs, msg.piece.index);
					
					if (check_hash(req->piece, msg.piece.index))
					{
						state->got++;
						printf("downloaded piece %x from peer %d\n", msg.piece.index, i);
						//printf("%.02f%%\b\b\b\b\b\b\b", ((float)state->got*100)/metainfo.num_pieces);
						printf("got %d/%d\n", state->got, metainfo.num_pieces);
						printf("requested %d/%d\n", state->requested, metainfo.num_pieces);
						//fflush(stdout);
						set_bit(state->have, msg.piece.index);
						peer->pieces_downloaded++;

						write_to_file(req->piece, msg.piece.index);
						
						// send out haves
					}
					else
					{
						printf("piece %x did not match hash!\n", msg.piece.index);
						break;
					}
				}
				
				if (peer->blocks_downloaded == QUEUE_LEN)
				{
					peer->flags &= ~SENT_REQ;
				}
				
				break; 
			case CANCEL: printf("cancel\n"); break;
			case PORT: printf("port\n"); break;
			default: printf("unsupported message.\n");
		}
	}
}

void start_pwp(struct peer *peer, int peer_num, struct state *state)
{
	int i, j, k;
	
	struct pollfd peer_fds[MAX_PEERS];
	struct handshake, peer_hs;
	struct msg msg[1024];
	struct msg_request req[QUEUE_LEN];
	
	int recv_size;
	unsigned char recv_buffer[RECV_MAX];
	
	int num_msgs;
	
	int index, begin, length, flags;

	connect_peers(&peer_fds, peer, peer_num);
	
	signal(SIGPIPE, SIG_IGN);
	while (state->got != metainfo.num_pieces)
	{		
		poll(peer_fds, peer_num, -1);
		
		if (state->requested == metainfo.num_pieces && !state->endgame_mode)
		{
			printf("starting endgame mode\n");
			state->endgame_mode = true;
		}
		
		for (i = 0; i < peer_num; i++)
		{
			if (!(peer[i].flags & SENT_HS) && peer_fds[i].revents & POLLOUT)
			{
				send_handshake(peer_fds[i].fd, metainfo.info_hash);
				peer[i].flags |= SENT_HS;
				continue;
			}
			
			if ((peer[i].flags & SENT_REQ || peer[i].flags & AM_INTERESTED) && peer[i].send_time+PEER_TIMEOUT < time(NULL))
			{
				for (j = 0; j < get_len(peer->queue); j++)
				{
					index = pop_req(peer[i].queue);
					if (!get_bit(state->have, index))
						set_bit(state->pending_reqs, index);
				}
				
				if (peer[i].timeouts >= MAX_TIMEOUT)
				{
					printf("peer %d timed out\n", i);
					peer[i].flags &= ~AM_INTERESTED;
				}
				else
					peer[i].timeouts++;
				
				peer[i].blocks_downloaded = 0;
				peer[i].flags &= ~SENT_REQ;
			}
			
			if (peer_fds[i].revents & POLLIN)
			{
				if ((recv_size = recv(peer_fds[i].fd, recv_buffer, RECV_MAX, MSG_PEEK)) < 1)
					continue;
				
				if (!(peer[i].flags & CONNECTED))
				{
					if (recv_size < sizeof(struct handshake))
						continue;
					
					recv(peer_fds[i].fd, recv_buffer, sizeof (struct handshake), 0);
					recv_size -= sizeof (struct handshake);
					memcpy(&peer_hs, recv_buffer, sizeof (struct handshake));
					
					if (check_handshake(&peer_hs, metainfo.info_hash, peer, i, peer_num))
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
				num_msgs = parse_msgs(&msg, recv_buffer, &left);
				recv(peer_fds[i].fd, recv_buffer, recv_size-left, 0);
				handle_msgs(peer, &msg, num_msgs);
				
				if (peer[i].flags & RECVD_BITFIELD)
				{
					if (peer[i].flags & AM_CHOKING && !(peer[i].flags & AM_INTERESTED))
					{
						send_interested(peer_fds[i].fd);
						peer[i].flags |= AM_INTERESTED;
						peer[i].send_time = time(NULL);
					}
					
					if (!(peer[i].flags & AM_CHOKING) && peer[i].flags & AM_INTERESTED && !(peer[i].flags & SENT_REQ))
					{
						num_reqs = make_requests(&req, state, peer);
						for (j = 0; j < num_reqs; j++)
						{
							send_request(peer_fds[i].fd, req[j].index, req[j].begin, req[j].length);
						}
						
						peer[i].send_time = time(NULL);
						peer[i].flags |= SENT_REQ;
					}
				}
			}
		}
	}
	printf("done.\n");
}
