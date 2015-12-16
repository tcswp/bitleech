#include "torrc.h"

struct metainfo metainfo;
extern int multi_file_mode, file_num;
FILE *logfp;

int main(int argc, char **argv)
{
	struct announce_res ares;
	struct state state;
	char *name = argv[1];
	//char c;
	int i;

	printf("NanoBT 1.0\nAuthor: Cheeve\n\n");

	logfp = fopen("log", "a+");
	
	metainfo = parse_torrent_file(name);
	
	if (multi_file_mode)
	{
		if (metainfo.name) 
		{
			mkdir(metainfo.name, 0777);
			if (chdir(metainfo.name) == -1)
				perror("chdir\n");
		}
		for (i = 0; i < file_num; i++)
		{
			mkpath(metainfo.file[i].path, 0777);
			fopen(metainfo.file[i].path, "w");
		}
	}
	else fopen(metainfo.name, "w");
	
	init_state(&state);
	
	do
	{
		announce(metainfo.info_hash, &ares, &state, metainfo.announce_list);
	}
	while (!ares.peer_num && sleep(ares.interval));
	
	start_pwp(ares.peer, ares.peer_num, &state);

	return 0;
}
