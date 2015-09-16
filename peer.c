#include "torrc.h"

extern struct metainfo *mi;

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
			if (state->piece_freq[i] && (state->piece_freq[i] < state->piece_freq[min]) && !get_bit(state->have, i) && !get_bit(state->pending_reqs, i))
				min = i;

 	return min;
}

int parse_msgs(struct state *state, int i)
{
	int recv_size, msg_offset, msg_size;

	int index, length, begin;
	int p, j;
	
	struct msg msg;
	struct handshake hs;

	unsigned char *id;
	
	static int got, num_connected;
	
	if ((recv_size = recv(state->peer[i].sock,state->peer[i].recv_buffer+state->peer[i].recv_len,MAX_MSG,0)) < 1)
		return -1; // error receiving

	state->peer[i].recv_len += recv_size;
		
	if (!state->peer[i].connected && state->peer[i].recv_len >= sizeof(struct handshake))
	{
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
			num_connected++;
			printf("connected to %d peers\n", num_connected);
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
			state->peer[i].sent_interested = false;
			state->peer[i].piece_offset = 0;
			state->peer[i].block_len = BLOCK_LEN;
			state->peer[i].send_time = 0;
			state->peer[i].curr_piece	= calloc(mi->piece_length, sizeof (unsigned char));
			state->peer[i].qlen = 0;
			state->peer[i].requestq[qlen] = -1;
			state->peer[i].downloaded = 0;
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
				if (!get_bit(state->have, j))
					printf("need piece %d\n", j);
				if (get_bit(state->pending_reqs, j))
					printf("but piece %d is in the pending rquests\n", j);
			}
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
					// if bitfield not sent make one.
					index = ntohl(*(long *)(id+1));
					//printf("have (Idx:%x)\n", index);
					set_bit(state->peer[i].bitfield, index, 1);
					state->piece_freq[index]++;
					break;
				case 5:
					//printf("bitfield\n");
					memcpy(state->peer[i].bitfield,id+1,ceil(((float)mi->num_pieces)/8));
					
					for (j = 0; j < mi->num_pieces; j++)
						state->piece_freq[j] += get_bit(state->peer[i].bitfield, j);
					
					break;    
				case 6: printf("request\n"); break;
				case 7:
					//printf("piece\n");
					length = ntohl(*(long *)(id-4))-9;
					index = ntohl(*(long *)(id+1));
					begin = ntohl(*(long *)(id+5));
					if (index != state->peer[i].requestq[state->peer[i].qlen] || get_bit(state->have, index))
					{
						break;
					}
					
					memcpy(state->peer[i].curr_piece+begin, id+9, length);
					state->qd_requests--;
					state->peer[i].send_time = 0;
					
					if (begin+length == mi->piece_length)
					{
						if (check_hash(state->peer[i].curr_piece, index))
						{
							got++;
							printf("%.02f%%\b\b\b\b\b\b\b", ((float)got*100)/mi->num_pieces);
							fflush(stdout);
							set_bit(state->have, index, 1);
							state->peer[i].downloaded++;
							state->peer[i].qlen--;
							
							write_to_file(state->peer[i].curr_piece, begin);
							
							msg.length = htonl(5);
							msg.id = HAVE;
							msg.have = htonl(index);
							
							for (p = 0; p < state->peer_num; p++)
								send(state->peer[p].sock, &msg, 9, 0);
							
						}
						else
						{
							//printf("piece did not match hash!\n");
							break;
						}
						
						set_bit(state->pending_reqs, index, 0);
						
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
	int i, j, sockfd;
	struct sockaddr_in peer_addr;
	struct pollfd peer_fds[MAX_PEERS];
	
	struct handshake hs;
	struct msg msg;
	
	int index;
			
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
			if (!state->peer[i].sent_hs && peer_fds[i].revents & POLLOUT)
			{
				state->peer[i].recv_buffer	= malloc(100000);
				state->peer[i].recv_len		= 0;
				state->peer[i].sent_hs = true;
				state->peer[i].connected = false;

				send(peer_fds[i].fd, &hs, sizeof(hs), 0);
			}
			
			if ((peer_fds[i].revents & POLLIN) && state->peer[i].sent_hs)
			{
				if (parse_msgs(state, i) == -1)
					continue;
				// assign first piece if new peer
				if (state->peer[i].requestq[state->peer[i].qlen] == -1)
					for (j = 0; j < mi->num_pieces; j++)
						if (!get_bit(state->pending_reqs, j) && !get_bit(state->have, j) && get_bit(state->peer[i].bitfield, j))
							state->peer[i].requestq[state->peer[i].qlen] = j;
				
// 				if (!state->peer[i].send_time && state->peer[i].send_time+5 < time(NULL))
// 				{
// 					set_bit(state->pending_reqs, state->peer[i].requestq[state->peer[i].qlen], 0);
//					state->peer[i].sent_choke = true;
// 					state->peer[i].send_time = 0;
// 					
// 					continue;
// 				}
							
				if (state->peer[i].downloaded > 0)
					for (j = state->peer[i].qlen; j < QUEUE_LEN; j++)
					{
						index = min(state);
						if (index == -1)
							break;
						state->peer[i].requestq[state->peer[i].qlen++] = index;
						set_bit(state->pending_reqs, index)
					}
				
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
					msg.request.index	= htonl(state->peer[i].requestq[state->peer[i].qlen]);
					msg.request.begin	= htonl(state->peer[i].piece_offset);
					msg.request.length	= htonl(state->peer[i].block_len);
					
					//printf("sending request for index %x\n", state->peer[i].requestq[state->peer[i].qlen]);
					send(peer_fds[i].fd, &msg, 17, 0);
					set_bit(state->pending_reqs, state->peer[i].requestq[state->peer[i].qlen], 1);
					state->peer[i].send_time = time(NULL);
				}
			}
		}
	}
}
