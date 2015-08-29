#include "torrc.h"

extern struct metainfo *mi;

// request pipeline
/*void enqueue(int p, unsigned char *request)
{
	struct request_item *item = malloc(sizeof (*item));
	
	if (md->peer[p]->front == NULL)
		md->peer[p]->front = item;
	else
		md->peer[p]->back->next = item;
	
	md->peer[p]->back = item;
	memcpy(md->peer[p]->back->data, request, );	
	
	md->peer[p].qd_requests++;
}*/

// returns 4-byte size of message
static inline unsigned int get_msg_size(char *msg){return ntohl(*(long *)msg)+4;}

static inline int  get_bit(unsigned char *bf, int i){return (bf[i/8]>>(7-(i%8)))&1;};
static inline void set_bit(unsigned char *bf, int i, int bit){bf[i/8]|=(bit<<(7-(i%8)));};

int min(struct state *state)
{
	int i;
	int min;

	min = -1;
	for (i = 0; i < mi->num_pieces; i++)
		if (state->piece_freq[i] && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i))
		{
			min = i;	
			break;
		}

	if (min != -1)
		for (i = min+1; i < mi->num_pieces; i++)
			if ((state->piece_freq[i] < state->piece_freq[min]) && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i))
				min = i;

 	return min;
}

int parse_msgs(struct state *state, int i)
{
	int recv_size, msg_offset, msg_size;

	int index, length, begin;
	int p, j;
	
	struct msg msg;

	unsigned char *id;
	
	if ((recv_size = recv(state->peer[i].sock,state->peer[i].recv_buffer+state->peer[i].recv_len,MAX_MSG,0)) < 1)
		return -1; // error receiving

	state->peer[i].recv_len += recv_size;
		
	if (!state->peer[i].connected && state->peer[i].recv_len >= sizeof(struct handshake))
	{
		if (!strcmp(state->peer[i].recv_buffer+1,PSTR)) // full handshake sent
		{
			printf("received handshake\n");
			state->peer[i].connected = true;
			state->peer[i].bitfield = calloc(ceil(((float)mi->num_pieces)/8), sizeof(char));
			recv_size -= sizeof(struct handshake);
			state->peer[i].recv_len -= sizeof(struct handshake);
			memmove(state->peer[i].recv_buffer,state->peer[i].recv_buffer+sizeof(struct handshake),recv_size);
		}
		else return -1;
	}
	
	msg_offset = 0;
	msg_size = 0;
	while (state->peer[i].recv_len >= 4)
	{
		msg_size = get_msg_size(state->peer[i].recv_buffer+msg_offset);
		if (msg_size > state->peer[i].recv_len)
			break;
		
		if (msg_size == 4)
			printf("keep-alive\n");
		else
		{
			id = (unsigned char *) state->peer[i].recv_buffer+msg_offset+4;

			switch (*id)
			{
				case 0: 
					printf("choke\n");
					state->peer[i].recvd_choke = true;
					break;
				case 1: 
					printf("unchoke\n");
					state->peer[i].recvd_choke = false;
					break;
				case 2: printf("interested\n"); break;
				case 3: printf("not interested\n"); break;
				case 4: 
					// if bitfield not sent make one.
					index = ntohl(*(long *)(id+1));
					//printf("have (Idx:%x)\n", index);
					set_bit(state->peer[i].bitfield, index, 1);
					state->piece_freq[index]++;
					break;
				case 5:
					printf("bitfield\n");
					memcpy(state->peer[i].bitfield,id+1,ceil(((float)mi->num_pieces)/8));
					
					for (j = 0; j < mi->num_pieces; j++)
						state->piece_freq[j] += get_bit(state->peer[i].bitfield, j);
					
					break;    
				case 6: printf("request\n"); break;
				case 7:
					printf("piece\n");
					length = ntohl(*(long *)(id-4))-9;
					index = ntohl(*(long *)(id+1));
					begin = ntohl(*(long *)(id+5));
					if (index != state->peer[i].curr_index)
					{
						printf("peer sent wrong piece\n");
						break;
					}
					
					memcpy(state->peer[i].curr_piece+begin, id+9, length);
					state->qd_requests--;
					state->peer[i].send_time = 0;
					
					if (begin+length == mi->piece_length)
					{
						if (check_hash(state->peer[i].curr_piece, index))
						{
							printf("downloaded piece %x\n", index);
							set_bit(state->have, index, 1);
							set_bit(state->pending_reqs, index, 0);
							
							//write_to_file(state->peer[i].curr_piece, begin);
							
							msg.length = htonl(5);
							msg.id = HAVE;
							msg.have = htonl(index);
							
							for (p = 0; p < state->peer_num; p++)
								send(state->peer[p].sock, &msg, 9, 0);
							
							state->peer[i].curr_index = min(state);
							printf("selected piece %x\n", state->peer[i].curr_index);
						}
						else
						{
							printf("piece did not match hash!\n");
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
	int i, sockfd;
	struct sockaddr_in peer_addr;
	struct pollfd peer_fds[MAX_PEERS];
	
	struct handshake hs;
	struct msg msg;
		
	hs = (struct handshake)
	{
		.pstrlen  = 0x13,
		.pstr	  = PSTR,
		.reserved = 0,
		.peer_id  = PEER_ID,
	};
	memcpy(&(hs.info_hash), mi->info_hash,20);
	
	srand(time(NULL));
			
	printf("connecting to peers...\n");
	for (i = 0; i < state->peer_num; i++)
	{
		if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
			err("error opening socket");

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
	}
	
	state->piece_freq 	= calloc(mi->num_pieces,sizeof (int));
	state->have 	 	= calloc(ceil(((float)mi->num_pieces)/8),sizeof (unsigned char));
	state->pending_reqs  = calloc(ceil(((float)mi->num_pieces)/8), sizeof (unsigned char));
	state->qd_requests  = 0;
	
	signal(SIGPIPE, SIG_IGN);
	while (1)
	{			
		if (poll(peer_fds, state->peer_num, -1) == -1)
			err("error polling");
		
		for (i = 0; i < state->peer_num; i++)
		{
			if (peer_fds[i].revents & POLLOUT && !state->peer[i].sent_hs)
			{
				state->peer[i].recv_buffer	= malloc(100000);
				state->peer[i].recv_len		= 0;
				state->peer[i].recvd_choke	= true;
				state->peer[i].sent_choke	= false;
				state->peer[i].recvd_interested = false;
				state->peer[i].sent_interested = false;
				state->peer[i].bitfield		= 0;
				state->peer[i].sent_interested = false;
				state->peer[i].send_time = 0;
				state->peer[i].curr_piece	= calloc(mi->piece_length, sizeof (unsigned char));
				state->peer[i].piece_offset = 0;
				state->peer[i].block_len = BLOCK_LEN;
				state->peer[i].send_time = 0;
				state->peer[i].sent_hs = true;
				state->peer[i].connected = false;
				
				state->peer[i].curr_index = rand()%mi->num_pieces;

				send(peer_fds[i].fd, &hs, sizeof(hs), 0);
			}
			
			if ((peer_fds[i].revents & POLLIN) && state->peer[i].sent_hs)
			{
				parse_msgs(state, i);
				
// 				if (state->peer[i].send_time+5 > time(NULL))
// 				{
// 					state->peer[i].sent_choke = true;
// 					continue;
// 				}
				
				if (state->qd_requests < QUEUE_LEN && state->peer[i].connected && get_bit(state->peer[i].bitfield, state->peer[i].curr_index))
				{
					if (state->peer[i].recvd_choke && !state->peer[i].sent_interested && !state->peer[i].sent_choke)
					{
						msg.length	= htonl(1);
						msg.id		= INTERESTED;
						send(peer_fds[i].fd, &msg, 5, 0);
						state->peer[i].sent_interested = true;
					}
					else if (!state->peer[i].recvd_choke && !state->peer[i].sent_choke && !state->peer[i].send_time)
					{
						msg.length			= htonl(13);
						msg.id				= REQUEST;
						msg.request.index	= htonl(state->peer[i].curr_index);
						msg.request.begin	= htonl(state->peer[i].piece_offset);
						msg.request.length	= htonl(state->peer[i].block_len);
						
						printf("sending request for index %x\n", state->peer[i].curr_index);
						send(peer_fds[i].fd, &msg, 17, 0);
						set_bit(state->pending_reqs, state->peer[i].curr_index, 1);
						state->qd_requests++;
						state->peer[i].send_time = time(NULL);
					}
				}
			}
		}
	}
}