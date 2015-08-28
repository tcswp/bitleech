#include "torrc.h"

struct metainfo *mi;

int main(int argc, char **argv)
{
	struct state state;
	mi = malloc(sizeof (struct metainfo));

	char *name = argv[1];

	printf("NanoBT 1.0\nAuthor: Cheeve\n\n");

	open_torrent_file(name);

	printf("\n\nSHA1 info hash: %20s\n", mi->info_hash);

	announce(&state);

	if (state.peer_num == 0)
		printf("no peers...\n");
	else
		start_pwp(&state);

	return 0;
}
