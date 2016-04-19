#include "bitleech.h"

struct metainfo metainfo;
extern int multi_file_mode, file_num, num_pieces;
FILE *logfp;

char save_file[29]; // sizeof b64 output + null byte: 4*ceil((double)20/3)+1
char *tracker_url;
struct state state; // this is global for sighandler. it is not otherwise accessed globally.

// void handle_args(int argc, char **argv)
// {
//   int opt;
//   
//   while ((opt = getopt(argc,argv,"")) != -1)
//   {
//     switch (opt)
//     {
//       
//     }
//   }
// }

int main(int argc, char **argv)
{
	struct announce_res ares = {0};
	char *name = argv[1];
	int i;
  struct sigaction sa;
  char path[PATH_MAX];
  char *home_dir;

	printf("bitleech 1.0\nAuthor: Steven Hastings\n\n");

	logfp = fopen("log", "a+");
	
	metainfo = parse_torrent_file(name);
  
  base64_encode(save_file, metainfo.info_hash);
  home_dir = getenv("HOME");
  strcpy(path,home_dir);
  strcat(path,"/.bitleech/");
  strcat(path,save_file);
  
	init_state(path, &state);

	for (i = 0; metainfo.announce_list[i] != NULL; i++)
		 if (announce(metainfo.info_hash, &ares, &state, 
       metainfo.announce_list[i])>-1)
       break;
     
  if (ares.peer_num)
  {
    printf("found %d peers\n", ares.peer_num);
    tracker_url = metainfo.announce_list[i];
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = save_state;
    sigaction(SIGINT, &sa, 0); //ctrl-c
    sigaction(SIGTERM, &sa, 0); // killed
    sigaction(SIGABRT, &sa, 0); // internal error
    sigaction(SIGHUP, &sa, 0); // closed terminal
    sigaction(SIGQUIT, &sa, 0); //ctrl-backslash
    start_pwp(ares.peer, ares.peer_num, &state);
  }
  else
    printf("couldn't find any peers. exiting...\n");
    
	return 0;
}
