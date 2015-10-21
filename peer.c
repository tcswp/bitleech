#include "torrc.h"

extern struct metainfo metainfo;

static inline int  get_bit(unsigned char *bf, int i){return (bf[i/8]>>(7-(i%8)))&1;};
static inline void set_bit(unsigned char *bf, int i){bf[i/8]^=(1<<(7-(i%8)));};

void ntohmsg(struct msg *msg, unsigned char *stream)
{
	msg->length = ntohl(*(long *)stream);
	msg->id		= *((char *)stream+4);
	switch (msg->id)
	{
		case 4:
			msg->have = ntohl(*(long *)(stream+5));
			break;
		case 5:
			msg->bitfield = stream+5;
			break;
		case 7:
			msg->piece.index = ntohl(*(long *)(stream+5));
			msg->piece.begin = ntohl(*(long *)(stream+9));
			msg->piece.block = stream+13;
			break;
	}
}
// this should be inline
int piece_length(struct metainfo *metainfo, int index)
{
	return ((index==metainfo->num_pieces-1)?metainfo->last_piece_length:metainfo->piece_length);	
}

int rarest_first(struct peer *peer, struct state *state, int p, int num_pieces)
{
	int i;
	int rarest;

	rarest = -1;
	for (i = 0; i < num_pieces; i++)
		if (state->piece_freq[i] && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i) && get_bit(peer[p].bitfield, i))
		{
			rarest = i;	
			break;
		}

	if (rarest != -1)
		for (i = rarest+1; i < num_pieces; i++)
			if (state->piece_freq[i] && (state->piece_freq[i] < state->piece_freq[rarest]) && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i) && get_bit(peer[p].bitfield, i))
				rarest = i;

 	return rarest;
}

int parse_msgs(struct peer *peer, int peer_num, struct state *state, struct metainfo *metainfo, int i)
{
	int recv_size, msg_offset, msg_size;

	//int index, length, begin;
	int j;
	
	struct handshake hs;

	struct msg msg;
	//unsigned char *id;
			
	if ((recv_size = recv(peer[i].sock,peer[i].recv_buffer+peer[i].recv_len,MAX_MSG,0)) < 1)
		return -1; // error receiving or nothing to get

	peer[i].recv_len += recv_size;
		
	if (!peer[i].connected)
	{
		if (peer[i].recv_len < sizeof(struct handshake))
			return -2;
		memcpy(&hs, peer[i].recv_buffer, sizeof (struct handshake));
		for (j = 0; j < peer_num; j++)
			if (peer[j].connected && i != j && !strcmp(hs.peer_id,peer[j].peer_id))
			{
				LOG("peer %d has same peer id as peer %d", j, i);
				close(peer[i].sock);
				return -1;
			}
		if (strcmp(hs.peer_id, PEER_ID) && hs.pstrlen == sizeof (PSTR)-1
			&& !strcmp(hs.pstr, PSTR) && !memcmp(metainfo->info_hash, hs.info_hash, 20))
		{
			//printf("received handshake\n");
			state->num_connected++;
			printf("connected to %d peers\n", state->num_connected);
			peer[i].connected = true;
			peer[i].bitfield = calloc(ceil(((float)metainfo->num_pieces)/8), sizeof(char));
			memcpy(peer[i].peer_id, hs.peer_id, 20);
			recv_size -= sizeof(struct handshake);
			peer[i].recv_len -= sizeof(struct handshake);
			memmove(peer[i].recv_buffer,peer[i].recv_buffer+sizeof(struct handshake),recv_size);
			
			peer[i].recvd_choke	= true;
			peer[i].sent_choke	= false;
			peer[i].recvd_interested = false;
			peer[i].sent_interested = false;
			peer[i].recvd_bitfield = false;
			peer[i].sent_req = false;
			peer[i].pieces_downloaded = 0;
			peer[i].timeouts	= 0;
			
			if (metainfo->piece_length/BLOCK_LEN>QUEUE_LEN)
				peer[i].q_size = metainfo->piece_length/BLOCK_LEN;
			else
				peer[i].q_size = QUEUE_LEN*BLOCK_LEN/metainfo->piece_length;
			
			// allocate space for the max number of reqs
			for (j = 0; j < peer[i].q_size; j++)
			{
				peer[i].request[j].piece = malloc(metainfo->piece_length);
				peer[i].request[j].blocks_downloaded = 0;
			}
			
			peer[i].q_size = 0;
		}
		else
		{
			LOG("one of the handshake fields is not right");
			close(peer[i].sock);
			return -1;
		}
	}
	
	msg_offset = 0;
	msg_size = 0;
	while (peer[i].recv_len >= 4)
	{
		msg_size = ntohl(*(long *)(peer[i].recv_buffer+msg_offset))+4;
		if (msg_size > peer[i].recv_len)
			break;
		
		if (msg_size == 4)
		{
			printf("keep-alive\n");
			for (j=0;j<metainfo->num_pieces;j++)
			{
				if (!get_bit(state->have, j) && get_bit(state->pending_reqs, j))
				{
					printf("piece %x is in the pending requests\n", j);
				}
			}

			printf("got %d/%d\n", state->got, metainfo->num_pieces);
			printf("requested %d/%d\n", state->requested, metainfo->num_pieces);
			printf("%d/%d choked peers\n", state->num_choked, state->num_connected);
		}
		else
		{
			ntohmsg(&msg, peer[i].recv_buffer+msg_offset);
			switch (msg.id)
			{
				case 0: 
					//printf("choke\n");
					peer[i].recvd_choke = true;
					break;
				case 1: 
					//printf("unchoke\n");
					peer[i].recvd_choke = false;
					break;
				case 2: printf("interested\n"); break;
				case 3: printf("not interested\n"); break;
				case 4: 
					//index = ntohl(*(long *)(id+1));
					//printf("have (Idx:%x)\n", index);
					set_bit(peer[i].bitfield, msg.have);
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
					
					//if (msg.piece.begin+(msg.length-9) == piece_length(metainfo, msg.piece.index))
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

// 							if (state->endmode)
// 							{
// 								for (j = 0; j < peer_num; j++)
// 								{
// 									pop(&peer[j].queue);
// 									// also send out cancels
// 								}
// 							}
// 							else
// 							{
// 								pop(&peer[i].queue);
// 							}
							
							write_to_file(metainfo, peer[i].request[j].piece, msg.piece.index);
							
// 							msg.length = htonl(5);
// 							msg.id = HAVE;
// 							msg.have = htonl(index);
// 							
// 							for (p = 0; p < peer_num; p++)
// 								send(peer[p].sock, &msg, 9, 0);
							
						}
						else
						{
							printf("piece %x did not match hash!\n", msg.piece.index);
							break;
						}
						peer[i].request[j].blocks_downloaded = 0;
					}
					
					if (peer[i].pieces_downloaded == peer[i].q_size)
					{
						peer[i].sent_req = false;
						peer[i].pieces_downloaded = 0;
						peer[i].q_size = 0;
					}
					
					break; 
				case 8: printf("cancel\n"); break;
				case 9: printf("port\n"); break;
				default: printf("unidentified message.\n");
			}
		}
			msg_offset += msg_size;
			peer[i].recv_len -= msg_size;
	}
	
	memmove(peer[i].recv_buffer, peer[i].recv_buffer+msg_offset, peer[i].recv_len);
	
	return 0;
}

inline void request_piece(struct metainfo *metainfo, struct state *state, int index, int sockfd)
{
	int block_len = BLOCK_LEN;
	int piece_offset = 0;
	
	struct msg msg;
	
	int i;
	
	for (i = 0; i < ceil((float)piece_length(metainfo, index)/BLOCK_LEN); i++)
	{
		if (index == metainfo->num_pieces-1)
			block_len = (metainfo->last_piece_length < BLOCK_LEN) ? metainfo->last_piece_length : BLOCK_LEN;
		
		msg.length			= htonl(13);
		msg.id				= REQUEST;
		msg.request.index	= htonl(index);
		msg.request.begin	= htonl(piece_offset);
		msg.request.length	= htonl(block_len);
		
		if (send(sockfd, &msg, 17, 0) == -1)
		{
			perror("send");
		}
		if (piece_offset == 0)
		{
			if (!get_bit(state->requests, index))
			{
				state->requested++;
				set_bit(state->requests, index);
			}
		}
		
		piece_offset += block_len;
		if (index == metainfo->num_pieces-1)
		{
			if ((piece_offset+block_len) > metainfo->last_piece_length)
				block_len = metainfo->last_piece_length-piece_offset;
		}
		else if ((piece_offset+block_len) > metainfo->piece_length)
			block_len = metainfo->piece_length-piece_offset;
	}
}

void start_pwp(struct peer *peer, int peer_num, struct state *state, struct metainfo *metainfo)
{
	int i, j, k, sockfd;
	struct sockaddr_in peer_addr;
	struct pollfd peer_fds[MAX_PEERS];
	
	struct handshake hs;
	struct msg msg;
	
	int index, flags;
			
	hs = (struct handshake)
	{
		.pstrlen  = 0x13,
		.pstr	  = PSTR,
		.reserved = 0,
		.peer_id  = PEER_ID,
	};
	memcpy(&(hs.info_hash), metainfo->info_hash,20);
			
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
				
		peer[i].connected	= false;
		peer[i].sock		= sockfd;
		
		peer_fds[i].fd 		= sockfd;
		peer_fds[i].events	= POLLOUT | POLLIN;
		peer_fds[i].revents = 0;
	}
	
	state->piece_freq 	 = calloc(metainfo->num_pieces,sizeof (int));
	state->have 	 	 = calloc(ceil(((float)metainfo->num_pieces)/8),sizeof (unsigned char));
	state->pending_reqs  = calloc(ceil(((float)metainfo->num_pieces)/8), sizeof (unsigned char));
	state->requests		 = calloc(ceil(((float)metainfo->num_pieces)/8), sizeof (unsigned char));
	state->requested	 = 0;
	state->got			 = 0;
	state->num_choked	 = 0;
	state->num_connected = 0;
	state->endmode		 = false;
	
	signal(SIGPIPE, SIG_IGN);
	while (state->got != metainfo->num_pieces)
	{		
		poll(peer_fds, peer_num, -1);
		
// 		if (state->requested == metainfo->num_pieces && !state->endmode)
// 		{
// 			printf("starting endgame mode\n");
// 			state->endmode = true;
// 			
// 			for (i = 0; i < metainfo->num_pieces; i++)
// 			{
// 				if (!get_bit(state->have, i))
// 					for (j = 0; j < peer_num; j++)
// 					{
// 						if (peer[j].connected)
// 						{
// 							if (peer[j].queue.front != NULL && peer[j].queue.front->piece == i)
// 								continue;
// 							peer[j].sent_req = false;
// 							enqueue(&peer[j].queue, i);
// 						}
// 					}
// 			}
// 		}
		
		for (i = 0; i < peer_num; i++)
		{
			if (!peer[i].sent_hs && peer_fds[i].revents & POLLOUT)
			{
				peer[i].recv_buffer	= malloc(100000);
				peer[i].recv_len	= 0;
				peer[i].sent_hs 	= true;
				peer[i].connected 	= false;

				send(peer_fds[i].fd, &hs, sizeof(hs), 0);
				continue;
			}
			
			if (peer[i].sent_choke && !state->endmode)
				continue;
			
			if (peer[i].connected && !state->endmode && (peer[i].sent_req||peer[i].sent_interested) && peer[i].send_time+PEER_TIMEOUT < time(NULL))
			{
				for (j = 0; j < peer[i].q_size; j++)
					set_bit(state->pending_reqs, peer[i].request[j].index);
				
				if (peer[i].timeouts >= MAX_TIMEOUT)
				{
					printf("peer %d timed out\n", i);			
					peer[i].q_size = 0;
					peer[i].sent_choke = true;
					state->num_choked++;
					continue;
				}
				else
				{
					peer[i].pieces_downloaded = 0;
					peer[i].q_size = 0;
					peer[i].sent_req = false;
					peer[i].timeouts++;
				}
			}
			
			if (peer[i].recvd_bitfield)
			{
				if (peer[i].recvd_choke && !peer[i].sent_interested)
				{
					msg.length	= htonl(1);
					msg.id		= INTERESTED;
					send(peer_fds[i].fd, &msg, 5, 0);
					peer[i].sent_interested = true;
					peer[i].send_time = time(NULL);
				}
				
				if (!peer[i].recvd_choke && !peer[i].sent_req)
				{
					for (j = 0; j < QUEUE_LEN*BLOCK_LEN/metainfo->piece_length; j++)
					{
						index = rarest_first(peer, state, i, metainfo->num_pieces);
						if (index == -1)
						{
							break;
						}

						printf("assigned piece %x to peer %d\n", index, i);
						peer[i].request[j].index = index;
						set_bit(state->pending_reqs, index);
						peer[i].q_size++;
						
						printf("downloading piece %x from peer %d\n", index, i);
						request_piece(metainfo, state, index, peer_fds[i].fd);

					}
					peer[i].send_time = time(NULL);
					peer[i].sent_req	 = true;
				}
			}
			
			parse_msgs(peer, peer_num, state, metainfo, i);
			
		}
	}
	printf("done.\n");
}
