#include "torrc.h"

struct metainfo metainfo;
extern int multi_file_mode, file_num, num_pieces;
FILE *logfp;
int **files;

char save_file[4*ceil((double)20/3)+7]; // sizeof b64 output + sizeof ".state" + null byte
char *tracker_url;
struct state state; // this is global for sighandler. it is not otherwise accessed globally.

int main(int argc, char **argv)
{
	struct announce_res ares;
	char *name = argv[1];
	//char c;
	int i;
  struct sigaction sa;
  
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = save_state;
  sigaction(SIGINT, &sa, 0); //ctrl-c
  sigaction(SIGTERM, &sa, 0); // killed
  sigaction(SIGABRT, &sa, 0); // internal error
  sigaction(SIGHUP, &sa, 0); // closed terminal
  sigaction(SIGQUIT, &sa, 0); //ctrl-\

	printf("NanoBT 1.0\nAuthor: Cheeve\n\n");

	logfp = fopen("log", "a+");
  
  base64_encode(save_file, metainfo.info_hash);
  strcat(save_file, ".state");
	
	metainfo = parse_torrent_file(name);
	
  files = malloc(file_num);
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
			files[i] = open(metainfo.file[i].path, O_CREAT|O_WRONLY));
		}
	}
	else
  {
    files[0] = open(metainfo.name, O_CREAT|O_WRONLY);
  }
  
	init_state(save_file, &state);
	
	do
	{
		announce(metainfo.info_hash, &ares, &state, metainfo.announce_list);
	}
	while (!ares.peer_num && sleep(ares.interval));
	
	start_pwp(ares.peer, ares.peer_num, &state);

	return 0;
}
