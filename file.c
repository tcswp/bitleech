#include "torrc.h"

extern struct metainfo metainfo;
extern int info_begin, info_end;

int file_num, multi_file_mode;
int num_pieces, last_piece_length, last_block_length;

void save_state(int)
{
  int fd;
  struct iovec iov[6];
  extern char *hostname;
  extern struct state state;
  
  mkdir(SAVE_DIR, 0777);
  chdir(SAVE_DIR);
  
  fd = open(save_file, O_CREAT|O_WRONLY);
  printf("Saving state...");
  
  iov[0].iov_base = state.have;        iov[0].iov_len = num_pieces;
  iov[1].iov_base = state.requests;    iov[1].iov_len = num_pieces;
  iov[2].iov_base = &state.requested;  iov[2].iov_len = sizeof int;
  iov[3].iov_base = &state.uploaded;   iov[3].iov_len = sizeof int;
  iov[4].iov_base = &state.downloaded; iov[4].iov_len = sizeof int;
  iov[5].iov_base = &state.left;       iov[5].iov_len = sizeof int;
  
  writev(fd, iov, 6);
  close(fd);
  
  strcpy(state.event, "stopped");
  announce(metainfo.info_hash, NULL, &state, &hostname);
}

int mkpath(char *path, mode_t mode)
{
    char *pch;
	int ret;
	
    pch = strchr(path, '/');
    while (pch != NULL)
    {
        *pch = '\0';
        ret = mkdir(path, mode);
        *pch = '/';
        if (ret == -1)
			return -1;
        pch = strchr(pch+1, '/');
    }
    return 0;
}

struct metainfo parse_torrent_file(char *filename)
{
	FILE *fp;
	int i, filesize;
	char benc[MAX_FILE];
	
	struct metainfo new_metainfo;
	
	unsigned int info_size;
	unsigned char sha1_hash[20];
	
	int total_length = 0;

	if ((fp = fopen(filename, "r")) == NULL)
	{
		errexit("error opening %s", filename);
	}
	
	/* get file size */
	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	rewind(fp);

	/* read file into string */
	for (i=0; i < filesize; i++)
		benc[i] = fgetc(fp);

	if (ferror(fp) != 0)
	{
		errexit("error reading %s\n", filename);
	}
	
	parse_dict(&set_metainfo, &new_metainfo, benc);
		
	//printf("info: %.*s\n\n\n\n", metainfo.info_end-metainfo.info_begin,benc+metainfo.info_begin);
	
	info_size = info_end-info_begin;
	SHA1((unsigned char *)benc+info_begin, info_size, sha1_hash);
	memcpy(new_metainfo.info_hash, sha1_hash, 20);
	
	if (multi_file_mode)
	{
		for (i = 0; i < file_num; i++)
			total_length += new_metainfo.file[i].length;
	}
	else
		total_length = new_metainfo.length;
	
	num_pieces = ceil((float)total_length/new_metainfo.piece_length);
	last_piece_length = total_length-((num_pieces-1)*new_metainfo.piece_length);
	last_block_length = last_piece_length % BLOCK_LEN;
	
	fclose(fp);
	
	return new_metainfo;
}

void save_piece(unsigned char *piece, int index)
{
	unsigned long eof = 0;
	int i;
  extern int **files;
	
	int begin = index*metainfo.piece_length;
	int length = piece_len(index);
	int offset;
	
	int p1len, p2len;

	if (!multi_file_mode)
		pwrite(metainfo.name, piece, length, begin);
	else
	{
		for (i = 0; i < file_num; i++)
		{/*
      -------------------------------
      |p1  |p2  |p3  |p4  |p5  |p6  |
      -------------------------------
      \__/\____________/\___________/
       f1      f2            f3        */
       
			eof += metainfo.file[i].length;
			offset = begin - (eof - metainfo.file[i].length);
			if (begin < eof && begin+length <= eof) // piece fits nicely into file
			{
				pwrite(files[i], piece, length, offset);
				break;
			}
			else if (begin < eof && begin+length > eof) // piece spans across files
			{
				p1len = eof-begin;
				pwrite(files[i], piece, p1len, offset);	
				p2len = (begin+length)-eof;
				pwrite(files[i+1], piece+p1len, p2len, offset);
				break;
			}
		}
	}
}
