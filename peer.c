#include "torrc.h"

extern struct metainfo *mi;

// returns 4-byte size of message
static inline unsigned int get_msg_size(char *msg){return ntohl(*(long *)msg)+4;}

static inline int  get_bit(unsigned char *bf, int i){return (bf[i/8]>>(7-(i%8)))&1;};
static inline void set_bit(unsigned char *bf, int i){bf[i/8]^=(1<<(7-(i%8)));};

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

int rarest_first(struct state *state, int p)
{
	int i;
	int rarest;

	rarest = -1;
	for (i = 0; i < mi->num_pieces; i++)
		if (state->piece_freq[i] && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i) && get_bit(state->peer[p].bitfield, i))
		{
			rarest = i;	
			break;
		}

	if (rarest != -1)
		for (i = rarest+1; i < mi->num_pieces; i++)
			if (state->piece_freq[i] && (state->piece_freq[i] < state->piece_freq[rarest]) && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i) && get_bit(state->peer[p].bitfield, i))
				rarest = i;

 	return rarest;
}

int parse_msgs(struct state *state, int i)
{
	int recv_size, msg_offset, msg_size;

	int index, length, begin;
	int j;
	
	struct handshake hs;

	unsigned char *id;
		
	if ((recv_size = recv(state->peer[i].sock,state->peer[i].recv_buffer+state->peer[i].recv_len,MAX_MSG,0)) < 1)
		return -1; // error receiving or nothing to get

	state->peer[i].recv_len += recv_size;
		
	if (!state->peer[i].connected)
	{
		if (state->peer[i].recv_len < sizeof(struct handshake))
			return -2;
		memcpy(&hs, state->peer[i].recv_buffer, sizeof (struct handshake));
		for (j = 0; j < state->peer_num; j++)
			if (state->peer[j].connected && i != j && !strcmp(hs.peer_id,state->peer[j].peer_id))
			{
				LOG("peer %d has same peer id as peer %d", j, i);
				close(state->peer[i].sock);
				return -1;
			}
		if (strcmp(hs.peer_id, PEER_ID) && hs.pstrlen == sizeof (PSTR)-1
			&& !strcmp(hs.pstr, PSTR) && !memcmp(mi->info_hash, hs.info_hash, 20))
		{
			//printf("received handshake\n");
			state->num_connected++;
			printf("connected to %d peers\n", state->num_connected);
			state->peer[i].connected = true;
			state->peer[i].bitfield = calloc(ceil(((float)mi->num_pieces)/8), sizeof(char));
			memcpy(state->peer[i].peer_id, hs.peer_id, 20);
			recv_size -= sizeof(struct handshake);
			state->peer[i].recv_len -= sizeof(struct handshake);
			memmove(state->peer[i].recv_buffer,state->peer[i].recv_buffer+sizeof(struct handshake),recv_size);
			
			state->peer[i].recvd_choke	= true;
			state->peer[i].sent_choke	= false;
			state->peer[i].recvd_interested = false;
			state->peer[i].sent_interested = false;
			state->peer[i].piece_offset = 0;
			state->peer[i].block_len = BLOCK_LEN;
			state->peer[i].ready = false;
			state->peer[i].sent_req = false;
			state->peer[i].curr_piece	= calloc(mi->piece_length, sizeof (unsigned char));
			state->peer[i].downloaded = 0;
			
			memset(&state->peer[i].queue, 0, sizeof (struct queue));
		}
		else
		{
			LOG("one of the handshake fields is not right");
			close(state->peer[i].sock);
			return -1;
		}
	}
	
	msg_offset = 0;
	msg_size = 0;
	while (state->peer[i].recv_len >= 4)
	{
		msg_size = get_msg_size(state->peer[i].recv_buffer+msg_offset);
		if (msg_size > state->peer[i].recv_len)
			break;
		
		if (msg_size == 4)
		{
			printf("keep-alive\n");
			for (j=0;j<mi->num_pieces;j++)
			{
				if (!get_bit(state->have, j) && get_bit(state->pending_reqs, j))
				{
					printf("piece %x is in the pending requests\n", j);
				}
			}
			for (j=0;j<state->peer_num;j++)
				if (state->peer[j].queue.front != NULL)
					printf("peer %d has piece %x in the queue\n", j, state->peer[j].queue.front->piece);
			printf("got %d/%d\n", state->got, mi->num_pieces);
			printf("requested %d/%d\n", state->requested, mi->num_pieces);
			printf("%d/%d choked peers\n", state->num_choked, state->num_connected);
		}
		else
		{
			id = (unsigned char *) state->peer[i].recv_buffer+msg_offset+4;

			switch (*id)
			{
				case 0: 
					//printf("choke\n");
					state->peer[i].recvd_choke = true;
					break;
				case 1: 
					//printf("unchoke\n");
					state->peer[i].recvd_choke = false;
					break;
				case 2: printf("interested\n"); break;
				case 3: printf("not interested\n"); break;
				case 4: 
					index = ntohl(*(long *)(id+1));
					//printf("have (Idx:%x)\n", index);
					set_bit(state->peer[i].bitfield, index);
					state->piece_freq[index]++;
					break;
				case 5:
					//printf("bitfield\n");
					memcpy(state->peer[i].bitfield,id+1,ceil(((float)mi->num_pieces)/8));
					
					for (j = 0; j < mi->num_pieces; j++)
						state->piece_freq[j] += get_bit(state->peer[i].bitfield, j);
					state->peer[i].ready = true;
					break;    
				case 6: printf("request\n"); break;
				case 7:
					//printf("piece\n");
					length = ntohl(*(long *)(id-4))-9;
					index = ntohl(*(long *)(id+1));
					begin = ntohl(*(long *)(id+5));
					
					if (index != state->peer[i].queue.front->piece || get_bit(state->have, index))
					{
						break;
					}
					
					memcpy(state->peer[i].curr_piece+begin, id+9, length);
					
					if (begin+length == mi->piece_length)
					{
						set_bit(state->pending_reqs, index);
						
						if (check_hash(state->peer[i].curr_piece, index))
						{
							LOG("downloaded piece %x from peer %d", index, i);
							state->got++;
							printf("%.02f%%\b\b\b\b\b\b\b", ((float)state->got*100)/mi->num_pieces);
							fflush(stdout);
							set_bit(state->have, index);
							state->peer[i].downloaded++;
							if (state->endmode)
							{
								for (j = 0; j < state->peer_num; j++)
								{
									pop(&state->peer[j].queue);
									// also send out cancels
								}
							}
							else
							{
								pop(&state->peer[i].queue);
							}
							
							//write_to_file(state->peer[i].curr_piece, begin);
							
// 							msg.length = htonl(5);
// 							msg.id = HAVE;
// 							msg.have = htonl(index);
// 							
// 							for (p = 0; p < state->peer_num; p++)
// 								send(state->peer[p].sock, &msg, 9, 0);

// 							index = rarest_first(state, i);
// 							if (index == -1)
// 								break;
// 							enqueue(&state->peer[i].queue, index);
// 							set_bit(state->pending_reqs, index, 1);
							
						}
						else
						{
							//printf("piece did not match hash!\n");
							break;
						}
						
						memset(state->peer[i].curr_piece, 0, mi->piece_length);
						state->peer[i].piece_offset = 0;
						state->peer[i].block_len = BLOCK_LEN;
					}
					else
					{
						state->peer[i].piece_offset += state->peer[i].block_len;
						if ((state->peer[i].piece_offset+state->peer[i].block_len) > mi->piece_length)
							state->peer[i].block_len = mi->piece_length-state->peer[i].piece_offset;
					}		
					
					state->peer[i].sent_req = false;
					
					break; 
				case 8: printf("cancel\n"); break;
				case 9: printf("port\n"); break;
				default: printf("unidentified message. it probably arrived out of order.\n");
			}
		}
			msg_offset += msg_size;
			state->peer[i].recv_len -= msg_size;
	}
	
	memmove(state->peer[i].recv_buffer, state->peer[i].recv_buffer+msg_offset, state->peer[i].recv_len);
	
	return 0;
}

void start_pwp(struct state *state)
{
	int i, j, sockfd;
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
	memcpy(&(hs.info_hash), mi->info_hash,20);
			
	printf("connecting to peers...\n");
	for (i = 0; i < state->peer_num; i++)
	{
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			err("error opening socket");
		flags = fcntl(sockfd, F_GETFL, 0);
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

		// fill in peer info
		memset(&peer_addr, 0,  sizeof(peer_addr));
		peer_addr.sin_family      = AF_INET;
		peer_addr.sin_addr.s_addr = htonl(state->peer[i].ip);
		peer_addr.sin_port        = htons(state->peer[i].port);

		connect(sockfd, (struct sockaddr *) &peer_addr, sizeof (peer_addr));
				
		state->peer[i].connected	= false;
		state->peer[i].sock			= sockfd;
		
		peer_fds[i].fd 		= sockfd;
		peer_fds[i].events	= POLLOUT | POLLIN;
		peer_fds[i].revents = 0;
	}
	
	state->piece_freq 	 = calloc(mi->num_pieces,sizeof (int));
	state->have 	 	 = calloc(ceil(((float)mi->num_pieces)/8),sizeof (unsigned char));
	state->pending_reqs  = calloc(ceil(((float)mi->num_pieces)/8), sizeof (unsigned char));
	state->requests		 = calloc(ceil(((float)mi->num_pieces)/8), sizeof (unsigned char));
	state->requested	 = 0;
	state->got			 = 0;
	state->num_choked	 = 0;
	state->num_connected = 0;
	state->endmode		 = false;
	
	signal(SIGPIPE, SIG_IGN);
	while (state->got != mi->num_pieces)
	{		
		poll(peer_fds, state->peer_num, -1);
	/*	
		if (state->requested == mi->num_pieces && !state->endmode)
		{
			printf("starting endgame mode\n");
			state->endmode = true;
			
			for (i = 0; i < mi->num_pieces; i++)
			{
				if (!get_bit(state->have, i))
					for (j = 0; j < state->peer_num; j++)
					{
						enqueue(&state->peer[j].queue, i);
					}
			}
		}
		*/
		for (i = 0; i < state->peer_num; i++)
		{
			if (!state->peer[i].sent_hs && peer_fds[i].revents & POLLOUT)
			{
				state->peer[i].recv_buffer	= malloc(100000);
				state->peer[i].recv_len		= 0;
				state->peer[i].sent_hs 		= true;
				state->peer[i].connected 	= false;

				send(peer_fds[i].fd, &hs, sizeof(hs), 0);
				continue;
			}
			
			if (state->peer[i].sent_choke && !state->endmode)
				continue;
			
			if (state->peer[i].connected && state->peer[i].sent_req && state->peer[i].send_time+5 < time(NULL))
			{
				printf("peer %d timed out on piece %x...\n", i, state->peer[i].queue.front->piece);
				set_bit(state->pending_reqs, state->peer[i].queue.front->piece);
				printf("removed piece %x from the pending requests\n", state->peer[i].queue.front->piece);
				pop(&state->peer[i].queue);
		
				state->peer[i].sent_choke = true;
				state->num_choked++;
				continue;
			}
			
			if (state->peer[i].ready)
			{
			
				if (state->peer[i].ready && state->peer[i].queue.front == NULL)
				{
					index = rarest_first(state, i);
					if (index == -1)
						continue;
					printf("assigned piece %x to peer %d\n", index, i);
					enqueue(&state->peer[i].queue, index);
					set_bit(state->pending_reqs, index);
				}
				
				if (state->peer[i].recvd_choke && state->peer[i].sent_interested && state->peer[i].send_time+5 < time(NULL))
				{
					state->peer[i].sent_choke = true;
					set_bit(state->pending_reqs, state->peer[i].queue.front->piece);
					pop(&state->peer[i].queue);
				}
				
				if (state->peer[i].recvd_choke && !state->peer[i].sent_interested)
				{
					msg.length	= htonl(1);
					msg.id		= INTERESTED;
					send(peer_fds[i].fd, &msg, 5, 0);
					state->peer[i].sent_interested = true;
					state->peer[i].send_time = time(NULL);
				}
				else if (!state->peer[i].recvd_choke && !state->peer[i].sent_req)
				{
					msg.length			= htonl(13);
					msg.id				= REQUEST;
					msg.request.index	= htonl(state->peer[i].queue.front->piece);
					msg.request.begin	= htonl(state->peer[i].piece_offset);
					msg.request.length	= htonl(state->peer[i].block_len);
					
					if (send(peer_fds[i].fd, &msg, 17, 0) != 17)
						printf("send did not take entire message!\n");
					if (state->peer[i].piece_offset == 0)
					{
						LOG("peer %d downloading piece %x", i, state->peer[i].queue.front->piece);
						if (!get_bit(state->requests, state->peer[i].queue.front->piece))
						{
							state->requested++;
							set_bit(state->requests, state->peer[i].queue.front->piece);
						}
					}
					state->peer[i].send_time = time(NULL);
					state->peer[i].sent_req	 = true;
				}
			}
			
			parse_msgs(state, i);
			
		}
	}
	printf("done.\n");
}
