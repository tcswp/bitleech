#include "torrc.h"

extern struct metainfo metainfo;

struct metainfo parse_torrent_file(char *filename)
{
	FILE *fp;
	int i, filesize;
	char benc[MAX_FILE];
	char c;
	
	struct metainfo new_metainfo;
	
	unsigned int info_size;
	unsigned char sha1_hash[20];

	if ((fp = fopen(filename, "r")) == NULL)
	{
		fprintf(stderr, "error opening %s\n", filename);
		exit(1);
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
		fprintf(stderr, "error reading %s\n", filename);
		exit(1);
	}
	
	parse_dict(&set_metainfo, &new_metainfo, benc);
		
	//printf("info: %.*s\n\n\n\n", metainfo.info_end-metainfo.info_begin,benc+metainfo.info_begin);
	
	info_size = metainfo.info_end-metainfo.info_begin;
	SHA1((unsigned char *)benc+metainfo.info_begin, info_size, sha1_hash);
	memcpy(metainfo.info_hash, sha1_hash, 20);
	
	fclose(fp);
	
	return new_metainfo;
}

void write_to_file(unsigned char *piece, int index)
{
	FILE *fp, *fp1, *fp2;
	unsigned long flen;
	int i;
	
	int begin = index*metainfo.piece_length;
	int length = piece_length(metainfo, index);
	
	unsigned char p1[length];
	unsigned char p2[length];
	int p1len, p2len;

	if (!metainfo.multi_file_mode)
	{
		fp = fopen(metainfo.name, "r+b");
		fseek(fp, begin, SEEK_SET);
		fwrite(piece, sizeof (char), length, fp);
		fclose(fp);
	}
	else
	{
		flen = 0;		
		for (i = 0; i < metainfo.file_num; i++)
		{
			if (i > 1)
				begin -= metainfo.file[i-1].length;
			
			flen += metainfo.file[i].length;
			if (begin < flen && begin+length <= flen) // piece fits nicely into file
			{
				fp = fopen(metainfo.file[i].path, "r+b");
				fseek(fp, begin, SEEK_SET);
				fwrite(piece, sizeof (char), length, fp);
				fclose(fp);
				break;
			}
			else if (begin < flen && begin+length > flen) // piece spans across files
			{
				
				
				p1len = flen-begin;
				memcpy(p1, piece, p1len);
				fp1 = fopen(metainfo.file[i].path, "r+b");
				fseek(fp1, begin, SEEK_SET);
				fwrite(p1, sizeof (char), p1len, fp1);
				fclose(fp1);
				
				p2len = (begin+length)-flen;
				memcpy(p2, piece+p1len, p2len);
				fp2 = fopen(metainfo.file[i+1].path, "r+b");
				fseek(fp2, 0, SEEK_SET);
				fwrite(p2, sizeof (char), p2len, fp2);
				fclose(fp2);
				
				break;
			}
		}
	}
}