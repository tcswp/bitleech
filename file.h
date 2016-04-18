struct metainfo parse_torrent_file(char *filename);
void save_piece(unsigned char *piece, int index);
void write_piece_to_file(char *path, unsigned char *piece, int length, int offset);
int mkpath(char *path, mode_t mode);
void save_state(int sig);
