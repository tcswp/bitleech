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
int piece_length(struct metainfo *metainfo, int index){return ((index==metainfo->num_pieces-1)?metainfo->last_piece_length:metainfo->piece_length);};

void enqueue(struct queue *q, int piece)
{
	struct request *req;
	req = malloc(sizeof (struct request));
	req->piece = piece;
	req->next = NULL;
	
	if (q->front == NULL)
	{
		q->front = req;
		req->prev = NULL;
	}
	else
	{
		q->back->next = req;
		req->prev = q->back;
	}
	
	q->back = req;
	
	q->size++;
}

int pop(struct queue *q)
{
	int piece;
	
	if (q->front == NULL)
		return -1;
	
	piece = q->front->piece;
	
	if (q->front == q->back)
	{
		free(q->front);
		q->front = q->back = NULL;
	}
	else
	{
		q->front = q->front->next;
		free(q->front->prev);
		q->front->prev = NULL;
	}
	
	q->size--;
	
	return piece;
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
			peer[i].ready = false;
			peer[i].sent_req = false;
			peer[i].downloaded = 0;
			
			peer[i].curr_piece	= calloc(metainfo->piece_length, sizeof (unsigned char));
			peer[i].piece_offset = 0;
			peer[i].block_len = 0;
			
			memset(&peer[i].queue, 0, sizeof (struct queue));
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
			for (j=0;j<peer_num;j++)
				if (peer[j].queue.front != NULL)
					printf("peer %d has piece %x in the queue\n", j, peer[j].queue.front->piece);
			printf("got %d/%d\n", state->got, metainfo->num_pieces);
			printf("requested %d/%d\n", state->requested, metainfo->num_pieces);
			printf("%d/%d choked peers\n", state->num_choked, state->num_connected);
		}
		else
		{
			ntohmsg(&msg, peer[i].recv_buffer+msg_offset);
			//id = (unsigned char *) peer[i].recv_buffer+msg_offset+4;

			switch (msg.id /* *id */)
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
					peer[i].ready = true;
					break;    
				case 6: printf("request\n"); break;
				case 7:
					//printf("piece\n");
// 					length = ntohl(*(long *)(id-4))-9;
// 					index = ntohl(*(long *)(id+1));
// 					begin = ntohl(*(long *)(id+5));
					
					if (msg.piece.index != peer[i].queue.front->piece || get_bit(state->have, msg.piece.index))
					{
						break;
					}
					
					memcpy(peer[i].curr_piece+msg.piece.begin, msg.piece.block, msg.length-9);
					
					if (msg.piece.begin+(msg.length-9) == piece_length(metainfo, msg.piece.index))
					{
						set_bit(state->pending_reqs, msg.piece.index);
						
						if (check_hash(peer[i].curr_piece, msg.piece.index, metainfo))
						{
							state->got++;
							LOG("downloaded piece %x from peer %d", msg.piece.index, i);
							printf("%.02f%%\b\b\b\b\b\b\b", ((float)state->got*100)/metainfo->num_pieces);
							fflush(stdout);
							set_bit(state->have, msg.piece.index);
							peer[i].downloaded++;
							if (state->endmode)
							{
								for (j = 0; j < peer_num; j++)
								{
									pop(&peer[j].queue);
									// also send out cancels
								}
							}
							else
							{
								pop(&peer[i].queue);
							}
							
							//write_to_file(peer[i].curr_piece, index, piece_length(metainfo, index));
							
// 							msg.length = htonl(5);
// 							msg.id = HAVE;
// 							msg.have = htonl(index);
// 							
// 							for (p = 0; p < peer_num; p++)
// 								send(peer[p].sock, &msg, 9, 0);

// 							index = rarest_first(peer, state, i, metainfo->num_pieces);
// 							if (index == -1)
// 								break;
// 							enqueue(&peer[i].queue, index);
// 							set_bit(state->pending_reqs, index, 1);
							
						}
						else
						{
							printf("piece did not match hash!\n");
							break;
						}
					}
					
					peer[i].sent_req = false;
					
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

void start_pwp(struct peer *peer, int peer_num, struct state *state, struct metainfo *metainfo)
{
	int i, j, sockfd;
	struct sockaddr_in peer_addr;
	struct pollfd peer_fds[MAX_PEERS];
	
	struct handshake hs;
	struct msg msg;
	
	struct request *preq;
	
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
		peer[i].sock			= sockfd;
		
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
				peer[i].recv_len		= 0;
				peer[i].sent_hs 		= true;
				peer[i].connected 	= false;

				send(peer_fds[i].fd, &hs, sizeof(hs), 0);
				continue;
			}
			
			if (peer[i].sent_choke && !state->endmode)
				continue;
			
			if (peer[i].connected && !state->endmode && peer[i].sent_req && peer[i].send_time+5 < time(NULL))
			{
				while (peer[i].queue.front != NULL)
				{
					//printf("peer %d timed out on piece %x...\n", i, peer[i].queue.front->piece);
					set_bit(state->pending_reqs, peer[i].queue.front->piece);
					//printf("removed piece %x from the pending requests\n", peer[i].queue.front->piece);
					pop(&peer[i].queue);
				}
		
				peer[i].sent_choke = true;
				state->num_choked++;
				continue;
			}
			
			if (peer[i].ready)
			{
				if (!state->endmode)
				{
					if (peer[i].queue.front == NULL)
					{
						for (j = 0; j < QUEUE_LEN*BLOCK_LEN/metainfo->piece_length; j++)
						{
							index = rarest_first(peer, state, i, metainfo->num_pieces);
							if (index == -1)
								break;
							//printf("assigned piece %x to peer %d\n", index, i);
							enqueue(&peer[i].queue, index);
							set_bit(state->pending_reqs, index);
						}
					}
					
					if (peer[i].recvd_choke && peer[i].sent_interested && peer[i].send_time+5 < time(NULL))
					{
						peer[i].sent_choke = true;
						set_bit(state->pending_reqs, peer[i].queue.front->piece);
						pop(&peer[i].queue);
					}
				
					if (peer[i].recvd_choke && !peer[i].sent_interested)
					{
						msg.length	= htonl(1);
						msg.id		= INTERESTED;
						send(peer_fds[i].fd, &msg, 5, 0);
						peer[i].sent_interested = true;
						peer[i].send_time = time(NULL);
					}
				}
				
				if (!peer[i].recvd_choke && !peer[i].sent_req && peer[i].queue.front != NULL)
				{
					for (preq = peer[i].queue.front; preq != NULL; preq = preq->next)
					{
						if (peer[i].block_len != BLOCK_LEN) //last request was for last block in a piece
						{
							memset(peer[i].curr_piece, 0, metainfo->piece_length);
							peer[i].piece_offset = 0;
							
							if (preq->piece == metainfo->num_pieces-1)
								peer[i].block_len = (metainfo->last_piece_length < BLOCK_LEN) ? metainfo->last_piece_length : BLOCK_LEN;
							else
								peer[i].block_len = BLOCK_LEN;
						}
						
						msg.length			= htonl(13);
						msg.id				= REQUEST;
						msg.request.index	= htonl(preq->piece);
						msg.request.begin	= htonl(peer[i].piece_offset);
						msg.request.length	= htonl(peer[i].block_len);
						
						if (send(peer_fds[i].fd, &msg, 17, 0) != 17)
							printf("send did not take entire message!\n");
						if (peer[i].piece_offset == 0)
						{
							LOG("peer %d downloading piece %x", i, preq->piece);
							if (!get_bit(state->requests, preq->piece))
							{
								state->requested++;
								set_bit(state->requests, preq->piece);
							}
						}
						
						peer[i].piece_offset += peer[i].block_len;
						if (preq->piece == metainfo->num_pieces-1)
						{
							if ((peer[i].piece_offset+peer[i].block_len) > metainfo->last_piece_length)
								peer[i].block_len = metainfo->last_piece_length-peer[i].piece_offset;
						}
						else if ((peer[i].piece_offset+peer[i].block_len) > metainfo->piece_length)
							peer[i].block_len = metainfo->piece_length-peer[i].piece_offset;
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
