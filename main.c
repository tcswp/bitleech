#include "torrc.h"

struct metainfo metainfo;

int main(int argc, char **argv)
{
	struct announce_res ares;
	struct state state;
	char *name = argv[1];

	printf("NanoBT 1.0\nAuthor: Cheeve\n\n");

	metainfo = parse_torrent_file(name);
	
	if (!access(metainfo.name, F_OK))
	{
		do
		{
			printf("\"%s\" already exists. overwrite? [y/n]\n", metainfo->name);
			c = getchar();
		}
		while (!(c == 'y' || c == 'n'));
		if (c == 'n') exit(0);
	}
	
	if (metainfo.multi_file_mode)
	{
		mkdir(metainfo.name, 0777);
		if (chdir(metainfo.name) == -1)
			err("chdir\n");
		for (i = 0; i < metainfo.file_num; i++)
			fopen(metainfo.file[i].path, "w");
	}
	else
		fopen(metainfo.name, "w");
	
	init_state(&state);
	
	do
	{
		announce(metainfo.info_hash, &ares, &state, metainfo.announce_list);
	}
	while (!ares.peer_num && sleep(ares.interval));
	
	start_pwp(ares.peer, ares.peer_num, &state);

	return 0;
}
