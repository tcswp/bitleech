#include "torrc.h"

extern struct metainfo metainfo;
extern int info_begin, info_end;

int file_num, multi_file_mode;

int num_pieces, last_piece_length, last_block_length;

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

void write_piece_to_file(char *path, unsigned char *piece, int length, int offset)
{
	FILE *fp;

	fp = fopen(path, "r+b");
	fseek(fp, offset, SEEK_SET);
	fwrite(piece, sizeof (char), length, fp);
	fclose(fp);
}

void save_piece(unsigned char *piece, int index)
{
	unsigned long eof = 0;
	int i;
	
	int begin = index*metainfo.piece_length;
	int length = piece_len(index);
	int offset;
	
	int p1len, p2len;

	if (!multi_file_mode)
		write_piece_to_file(metainfo.name, piece, length, begin);
	else
	{
		for (i = 0; i < file_num; i++)
		{
			eof += metainfo.file[i].length;
			offset = begin - (eof - metainfo.file[i].length);
			if (begin < eof && begin+length <= eof) // piece fits nicely into file
			{
				write_piece_to_file(metainfo.file[i].path, piece, length, offset);
				break;
			}
			else if (begin < eof && begin+length > eof) // piece spans across files
			{
				p1len = eof-begin;
				write_piece_to_file(metainfo.file[i].path, piece, p1len, offset);	
				p2len = (begin+length)-eof;
				write_piece_to_file(metainfo.file[i+1].path, piece+p1len, p2len, offset);
				break;
			}
		}
	}
}