#include "torrc.h"

void read_torrent_file(struct metainfo *metainfo, char *filename)
{
	FILE *fp;
	int i, filesize;
	char benc[MAX_FILE];
	char c;
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
	
	parse_dict(&set_metainfo, metainfo, benc);
	
	if (access(metainfo->name, F_OK) != -1)
	{
		do
		{
			printf("\"%s\" already exists. overwrite? [y/n]\n", metainfo->name);
			c = getchar();
		}
		while (!(c == 'y' || c == 'n'));
		if (c == 'n') exit(0);
	}
	else
		fopen(metainfo->name, "w");
		
	//printf("info: %.*s\n\n\n\n", metainfo->info_end-metainfo->info_begin,benc+metainfo->info_begin);
	
	// add code to check if info_hash is already in the metainfo file and calculate below only if not.

	info_size = metainfo->info_end-metainfo->info_begin;
	SHA1((unsigned char *)benc+metainfo->info_begin, info_size, sha1_hash);
	memcpy(metainfo->info_hash, sha1_hash, 20);
	
	fclose(fp);
}

// void write_to_file(unsigned char *piece, int index, int length)
// {
// 	FILE *fp, *fp1, *fp2;
// 	int i, flen;
// 	
// 	unsigned char p1[length-1];
// 	unsigned char p2[length-1];
// 	int p1len, p2len;
// 	
// 	char path[MAX_FILE];
// 	
// 	//struct stat st = {0};
// 
// 	if (!metainfo->multi_file_mode)
// 	{
// 		fp = fopen(metainfo->name, "r+b");
// 		fseek(fp, index*metainfo->piece_length, SEEK_SET);
// 		fwrite(piece, sizeof (char), length, fp);
// 		fclose(fp);
// 	}
// // 	else
// // 	{
// // 		if (stat(metainfo->name, &st) == -1) // use access
// // 			mkdir(metainfo->name, 0700);
// // 		sprintf(path, "./%s/", metainfo->name);
// // 	
// // 		flen = 0;
// // 		for (i = 0; i < metainfo->file_num; i++)
// // 		{
// // 			strcat(path, metainfo->file[i].path);
// // 			flen += metainfo->file[i].length;
// // 			if (begin < flen && begin+length < flen) // piece fits nicely into file
// // 			{
// // 				fp = fopen(path, "ab");
// // 				fseek(fp, begin, SEEK_SET);
// // 				fwrite(piece, sizeof (char), length, fp);
// // 				fclose(fp);
// // 				break;
// // 			}
// // 			else if (begin < flen && begin+length > flen) // piece spans across files
// // 			{
// // 				p1len = flen-begin;
// // 				p2len = (begin+length)-flen;
// // 				memcpy(p1, piece, p1len);
// // 				memcpy(p2, piece+p1len, p2len);
// // 				fp1 = fopen(path, "ab");
// // 				fwrite(p1, sizeof (char), p1len, fp1);
// // 				sprintf(path, "./%s/%s", metainfo->name, metainfo->file[i].path);
// // 				fp2 = fopen(path, "ab");
// // 				fseek(fp1, begin, SEEK_SET);
// // 				//fseek(fp2, 0, SEEK_SET);
// // 				fwrite(p2, sizeof (char), p2len, fp2);
// // 				fclose(fp1);
// // 				fclose(fp2);
// // 				break;			
// // 			}
// // 		}
// // 	}
// }