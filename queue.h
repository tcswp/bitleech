struct queue
{
	int len;
	struct req *front;
	struct req *back;
};

struct req
{
	int index;
	int blocks_downloaded;
	int blocks_queued;
	unsigned char *piece;
	
	struct req *next;
	struct req *prev;
};

void init_queue(struct queue *queue);
struct req *get_req(struct queue *queue, index);
void insert_req(struct queue *queue, int index, int piece_length);
int delete_req(struct queue *queue, int index);
int is_empty(struct queue *queue);
struct req *get_tail(struct queue *queue);