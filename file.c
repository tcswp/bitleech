#include "torrc.h"

extern struct metainfo *mi;

void load_file(char *buf, char *filename)
{
	FILE *fp;
	int i, filesize;

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
		buf[i] = fgetc(fp);

	if (ferror(fp) != 0)
	{
		fprintf(stderr, "error reading %s\n", filename);
		exit(1);
	}

	fclose(fp);
}

void open_torrent_file(char *filename)
{
	char buffer[MAX_FILE];

	unsigned int info_size;
	unsigned char sha1_hash[20];

	load_file(buffer, filename);

	int pos = 0;
	parse_dict(NULL, buffer, &pos, 1);

	//printf("info: %.*s\n\n\n\n", mi->info_end-mi->info_begin,buffer+mi->info_begin);
	
	// add code to check if info_hash is already in the metainfo file and calculate below only if not.

	info_size = mi->info_end-mi->info_begin;
	SHA1((unsigned char *)buffer+mi->info_begin, info_size, sha1_hash);
	memcpy(mi->info_hash, sha1_hash, 20);
}

void write_to_file(unsigned char *piece, int begin)
{
	FILE *fp, *fp1, *fp2;
	int i, flen;
	
	unsigned char p1[mi->piece_length-1];
	unsigned char p2[mi->piece_length-1];
	int p1len, p2len;
	
	char path[MAX_FILE];
	
	struct stat st = {0};

	if (mi->file_num > 0 && stat(mi->name, &st) == -1)
		mkdir(mi->name, 0700);
	
	if (mi->file_num == 0)
	{
		fp = fopen(mi->name, "ab");
		fseek(fp, begin, SEEK_SET);
		fwrite(piece, sizeof (char), mi->piece_length, fp);
		fclose(fp);
	}
	else
		sprintf(path, "./%s/", mi->name);

	
	flen = 0;
	for (i = 0; i < mi->file_num; i++)
	{
		strcat(path, mi->file[i].path);
		flen += mi->file[i].length;
		if (begin < flen && begin+mi->piece_length < flen) // piece fits nicely into file
		{
			fp = fopen(path, "ab");
			fseek(fp, begin, SEEK_SET);
			fwrite(piece, sizeof (char), mi->piece_length, fp);
			fclose(fp);
			break;
		}
		else if (begin < flen && begin+mi->piece_length > flen) // piece spans across files
		{
			p1len = flen-begin;
			p2len = (begin+mi->piece_length)-flen;
			memcpy(p1, piece, p1len);
			memcpy(p2, piece+p1len, p2len);
			fp1 = fopen(path, "ab");
			fwrite(p1, sizeof (char), p1len, fp1);
			sprintf(path, "./%s/%s", mi->name, mi->file[i].path);
			fp2 = fopen(path, "ab");
			fseek(fp1, begin, SEEK_SET);
			//fseek(fp2, 0, SEEK_SET);
			fwrite(p2, sizeof (char), p2len, fp2);
			fclose(fp1);
			fclose(fp2);
			break;			
		}
	}
}