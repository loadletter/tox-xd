#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ftw.h>
#include "fileop.h"
#include "crypto/crc32.h"
#include "crypto/md5.h"
#include "crypto/sha256.h"

#define HASHING_BUFSIZE 64 * 1024
#define SHRDIR_MAX_DESCRIPTORS 6

FileNode **shr_list = NULL;
uint shr_list_len = 0;
FileNode **new_list = NULL;
uint new_list_len = 0;

int file_checksumcalc_noblock(FileHash *dest, char *filename)
{
	static FILE *f = NULL;
	uint i;
	int rc;
	uint8_t buf[HASHING_BUFSIZE];
	static uint32_t crc32;
	static MD5_CTX md5_ctx;
	static sha256_context sha_ctx;

	if(!f)
	{
		if(!(f = fopen(filename, "rb")))
		{
			perror("fopen");
			return(-1);
		}
		crc32 = 0;
		MD5_Init(&md5_ctx);
		sha256_starts(&sha_ctx);
	}
	
	if((i = fread(buf, 1, sizeof(buf), f)) > 0)
	{
		crc32 = crc32_compute(crc32, buf, i);
		MD5_Update(&md5_ctx, buf, i);
		sha256_update(&sha_ctx, buf, i);
		rc = i;
	}
	else
	{
		dest->crc32 = crc32;
		MD5_Final(dest->md5, &md5_ctx);
		sha256_finish(&sha_ctx, dest->sha256);
		if(fclose(f) != 0)
			perror("fclose"); /* TODO: maybe do something more safe? */
		f = NULL;
		rc = 0;
	}
	
	/* rc > 0: still hashing
	 * rc == 0: complete
	 * rc < 0: error*/
	return(rc);
}

int file_checksumcalc(FileHash *dest, char *filename)
{
	FILE *f;
	uint i = 0;
	uint8_t buf[HASHING_BUFSIZE];
	uint32_t crc32;
	MD5_CTX md5_ctx;
	sha256_context sha_ctx;

	if(!(f = fopen(filename, "rb")))
	{
		perror("fopen");
		return(-1);
	}
	
	crc32 = 0;
	MD5_Init(&md5_ctx);
	sha256_starts(&sha_ctx);
	
	while((i = fread(buf, 1, sizeof(buf), f)) > 0)
	{
		crc32 = crc32_compute(crc32, buf, i);
		MD5_Update(&md5_ctx, buf, i);
		sha256_update(&sha_ctx, buf, i);
	}
	
	dest->crc32 = crc32;
	MD5_Final(dest->md5, &md5_ctx);
	sha256_finish(&sha_ctx, dest->sha256);
	
	if(fclose(f) != 0)
		perror("fclose");
	
	return(0);
}

static int file_walk_callback(const char *path, const struct stat *sptr, int type)
{
	int i, n = -1;
	if (type == FTW_DNR)
		fprintf(stderr, "Directory %s cannot be traversed.\n", path);
	if (type == FTW_F)
	{
		for(i=0;i<shr_list_len;i++)
		{
			if((strcmp(path, shr_list[i]->file) == 0) &&\
			(sptr->st_mtime == shr_list[i]->mtime) &&\
			(sptr->st_size == shr_list[i]->size))
			{
				n = i;
				break;
			}
		}
		
		if(n == -1)
		{
			new_list = realloc(new_list, sizeof(FileNode *) * (new_list_len + 1));
			if(new_list == NULL)
			{
				perror("realloc");
				return -1;
			}
			new_list[new_list_len] = malloc(sizeof(FileNode));
			if(new_list[new_list_len] == NULL)
			{
				perror("malloc");
				return -1;
			}
			new_list[new_list_len]->file = strdup(path);
			new_list[new_list_len]->info = NULL;
			new_list[new_list_len]->mtime = sptr->st_mtime;
			new_list[new_list_len]->size = sptr->st_size;
			new_list_len++;
		}
	}
	return 0;
}

int file_walk_shared(char *shrdir)
{
	char *abspath = NULL;
	int rc;
	
	abspath = realpath(shrdir, abspath);
	if(abspath == NULL)
	{
		perror("realpath");
		return -1;
	}
	rc = ftw(abspath, file_walk_callback, SHRDIR_MAX_DESCRIPTORS);
	free(abspath);
	
	return rc; 
}
/* TODO: remove the first for and find a way to run checksumcalc_noblock
int file_do(void)
{
	int i, t;
	int n = -1;
	int o = -1;

	for(i=new_list_len;i!=0;i--)
	{
		n = -1;
		o = -1;
		for(t=0;t<shr_list_len;t++)
		{
			if(strcmp(new_list[i]->file, shr_list[t]->file) == 0)
			{
				n = i;
				o = t;
				break;
			}
		}
		
		if(n == -1)
		{
			shr_list = realloc(shr_list, sizeof(FileNode *) * (shr_list_len + 1));
			if(shr_list == NULL)
			{
				perror("realloc");
				return -1;
			}
			shr_list[shr_list_len] = new_list[n];
			new_list = realloc(new_list, sizeof(FileNode *) * (new_list_len - 1));
			new_list_len--;
		}
		else
		{
			free(shr_list[t]->file);
			free(shr_list[t]->info);
			free(shr_list[t]);
			shr_list[t] = new_list[n];
			new_list = realloc(new_list, sizeof(FileNode *) * (new_list_len - 1));
			new_list_len--;
		}
		
		
	}

}*/

/* test checksumcalc 
 * gcc crypto/sha256.c crypto/md5.c crypto/crc32.c fileop.c -o fileop -Wall -march=native -O3*/
#if 0
static void printhash(FileHash fi, int rc)
{
	int i;
	putchar('\n');
	printf("%i\n", rc);
	printf("crc32: %.2X\nmd5: ", fi.crc32);
	for(i=0;i<16;i++)
		printf("%.2X", fi.md5[i]);
	printf("\nsha256: ");
	for(i=0;i<32;i++)
		printf("%.2X", fi.sha256[i]);
	putchar('\n');
}

static void printfnodes(int rc)
{
	int i;
	printf("%i\n", rc);
	for(i=0;i<new_list_len;i++)
	{
		printf("%lu - %lu - %s\n", new_list[i]->size, new_list[i]->mtime, new_list[i]->file);
	}
	putchar('\n');
}

int main()
{
	FileHash test;
	int rc;
	char testfile[] = "./fileop.c";
	
	rc = file_checksumcalc(&test, testfile);
	printhash(test, rc);
		
	while((rc = file_checksumcalc_noblock(&test, testfile)) > 0);
		//printf("%i ", rc);
	printhash(test, rc);
	
	rc = file_walk_shared("../");
	printfnodes(rc);
	return 0;
}
#endif
