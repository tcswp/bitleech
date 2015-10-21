#include "torrc.h"

int main(int argc, char **argv)
{
	struct metainfo metainfo = {0};
	struct announce_res ares = {0};
	struct state state;
	char *name = argv[1];

	printf("NanoBT 1.0\nAuthor: Cheeve\n\n");

	read_torrent_file(&metainfo, name);
		
	// need to fetch these values from a save file
	state = (struct state)
	{
		.uploaded = 0, 
		.downloaded = 0,
		.left  =  metainfo.num_pieces,
		.event = "started"
	};
	
	do
	{
		announce(metainfo.info_hash, &ares, &state, metainfo.announce_list);
	}
	while (!ares.peer_num && sleep(ares.interval));
	
	start_pwp(ares.peer, ares.peer_num, &state, &metainfo);

	return 0;
}
