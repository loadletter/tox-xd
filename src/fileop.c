#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <ftw.h>
#include "fileop.h"
#include "crypto/crc32.h"
#include "crypto/md5.h"
#include "crypto/sha256.h"

#define HASHING_BUFSIZE 64 * 1024 /* make it bigger? */
#define SHRDIR_MAX_DESCRIPTORS 6

static FileNode **shr_list = NULL;
static uint shr_list_len = 0;
static FileNode **new_list = NULL;
static uint new_list_len = 0;
static volatile sig_atomic_t file_recheck = FALSE;

static void (*file_new_c)(FileNode *, int) = NULL;

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
			perrlog("fopen");
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
			perrlog("fclose"); /* TODO: maybe do something more safe? */
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
		perrlog("fopen");
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
		perrlog("fclose");
	
	return(0);
}

/* Run on every file of the shared dir, if a file is not alredy in the shared array
 *  or has different size/mtime the new_list array is enlarged and a new
 *  fileinfo struct is allocated */
static int file_walk_callback(const char *path, const struct stat *sptr, int type)
{
	int i, n = -1;
	if (type == FTW_DNR)
		ywarn("Directory %s cannot be traversed.\n", path);
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
				perrlog("realloc");
				return -1;
			}
			new_list[new_list_len] = malloc(sizeof(FileNode));
			if(new_list[new_list_len] == NULL)
			{
				perrlog("malloc");
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
		perrlog("realpath");
		return -1;
	}
	rc = ftw(abspath, file_walk_callback, SHRDIR_MAX_DESCRIPTORS);
	free(abspath);
	
	/* no new files found */
	if(new_list_len == 0)
		file_recheck = FALSE;
	
	return rc; 
}

/* signal handler */
void file_recheck_callback(int signo)
{
	if(signo == SIGUSR1)
		file_recheck = TRUE;
}

/* If the recheck flag is set and there is no hashing operations running
 * walks the directory.
 * 
 * If there are new files and nothing is currently hashing pick the latest
 * file from the new_list and check if it's alredy in shr_list:
 * - if totally new, reallocate the shared list to accomodate the new
 * - if preexisting in shr_list, free the old and point it to the data of the new
 * then shrink the new_list, removing the last element (frees it when it's empty),
 * finally allocate a FileHash struct for the new element in shr_list and set a flag for hashing
 * 
 * If the hashing flag was set, take it's number and start the hashing,
 * when the entire file has been read or an error occurs clear the hashing flag.*/
 int file_do(char *shrdir)
{
	int i, t, rc;
	int n = -1;
	static int hashing = -1;
	static int last;
	
	if(file_recheck && new_list_len == 0 && hashing == -1)
	{
		file_walk_shared(shrdir);
	}
	
	i = new_list_len - 1; /* Starts from last element */
	if(new_list_len > 0 && hashing == -1)
	{
		for(t=0;t<shr_list_len;t++)
		{
			if(strcmp(new_list[i]->file, shr_list[t]->file) == 0)
			{
				n = i;
				break;
			}
		}
		
		if(n == -1)
		{
			shr_list = realloc(shr_list, sizeof(FileNode *) * (shr_list_len + 1));
			if(shr_list == NULL)
			{
				perrlog("realloc");
				return -1;
			}
			shr_list[shr_list_len] = new_list[i];
			last = shr_list_len;
			shr_list_len++;
		}
		else
		{
			free(shr_list[t]->file);
			free(shr_list[t]->info);
			free(shr_list[t]);
			shr_list[t] = new_list[n];
			last = t;
		}
		
		new_list = realloc(new_list, sizeof(FileNode *) * (new_list_len - 1));
		new_list_len--;
		if(new_list == NULL && new_list_len != 0)
		{
			perrlog("realloc");
			return -1;
		}
		
		shr_list[last]->info = malloc(sizeof(FileHash));
		if(shr_list[last]->info == NULL)
		{
			perrlog("malloc");
			return -1;
		}
		
		hashing = last;
	}

	if(hashing >= 0)
	{
		rc = file_checksumcalc_noblock(shr_list[last]->info, shr_list[last]->file);
		if(rc <= 0)
		{
			hashing = -1;
			if(new_list_len == 0)
				file_recheck = FALSE;
			
			if(file_new_c != NULL)
				file_new_c(shr_list[last], last);
			ydebug("Hash: %i - %s", last, shr_list[last]->file);
		}
		
		if(rc < 0)
			return -1;
	}
	
	return 0;
}

void file_new_set_callback(void (*func)(FileNode *, int))
{
	file_new_c = func;
}

/* ENTERPRISE extern */
FileNode **file_get_shared(void)
{
	return shr_list;
}
int file_get_shared_len(void)
{
	return shr_list_len;
}

/*
 * file format:
 * put all predictable things in the beginning
 * put filename last and without newline
 * example:
 * crc32
 * md5
 * sha256
 * mtime
 * size
 * file
 */
FileNode *filenode_load(char *path)
{
	char md5[33], sha256[65], filename[PATH_MAX + 1];
	char *pos;

	FILE *fp = fopen(path, "wb");
	if(fp == NULL)
	{
		perrlog("fopen");
		return NULL;
	}
	
	FileNode *fn = malloc(sizeof(FileNode));
	if(fn == NULL)
	{
		perrlog("malloc");
		return NULL;
	}
	
	fn->info = malloc(sizeof(FileHash));
	if(fn->info == NULL)
	{
		perrlog("malloc");
		free(fn);
		return NULL;
	}
	
	if(fscanf(fp, "%u\n", &fn->info->crc32) != 1)
		goto parserr;
	if(fscanf(fp, "%32s\n", md5) != 1)
		goto parserr;
	if(fscanf(fp, "%64s\n", sha256) != 1)
		goto parserr;
	if(fscanf(fp, "%lld\n", (long long * ) &fn->mtime) != 1)
		goto parserr;
	if(fscanf(fp, "%lld\n", (long long * ) &fn->size) != 1)
		goto parserr;
		
	int rc = fread(filename, 1, PATH_MAX, fp);
	ydebug("Read: %i", rc); //DEBUG
	
	if(ferror(fp))
	{
		perror("fread");
		goto parserr;
	}
	
	if(feof(fp))
	{
		int i;
		pos = md5;
		for (i = 0; i < 32; ++i, pos += 2)
			sscanf(pos, "%2hhx", &fn->info->md5[i]);
		pos = sha256;
		for (i = 0; i < 64; ++i, pos += 2)
			sscanf(pos, "%2hhx", &fn->info->sha256[i]);
		return fn;
	}
	
	parserr:
		yerr("Error parsing: %s", path);
		free(fn->info);
		free(fn);
		return NULL;
}

int filenode_dump(FileNode *fnode, char *path)
{
	char *md5, *sha256;
	FILE *fp = fopen(path, "wb");
	if(fp == NULL)
	{
		perrlog("fopen");
		return -1;
	}
	
	md5 = human_readable_id(fnode->info->md5, 16);
	sha256 = human_readable_id(fnode->info->sha256, 32);
	
	fprintf(fp, "%u\n", fnode->info->crc32);
	fprintf(fp, "%s\n", md5);
	fprintf(fp, "%s\n", sha256);
	fprintf(fp, "%lld\n", (long long) fnode->mtime);
	fprintf(fp, "%lld\n", (long long) fnode->size);
	fprintf(fp, "%s", fnode->file);
	
	free(md5);
	free(sha256);
	
	if(fclose(fp) != 0)
	{
		perrlog("fclose");
		return -1;
	}
	return 0;
}

int directory_count(char *path)
{
	DIR * dirp;
	struct dirent * entry;
	int count = 0;
	
	dirp = opendir(path);
	if(dirp == NULL)
	{
		perrlog("opendir");
		return -1;
	}
	
	while((entry = readdir(dirp)) != NULL)
	{
		if (entry->d_type == DT_REG) /* If the entry is a regular file */
			count++;
	}
	
	if(closedir(dirp) != 0);
	{
		perrlog("closedir");
		return -1;
	}
	
	return count;
}

int filenode_load_fromdir(char *cachedir)
{
	FileNode *fn;
	char pathbuf[PATH_MAX + 20];
	int i;
	
	int dircount = directory_count(cachedir);
	if(dircount == 0)
		return 0;
	if(dircount < 0)
		return -1;
	
	while(i < dircount)
	{
		snprintf(pathbuf, sizeof(pathbuf), "%s/%i", cachedir, i);
		
		if(access(pathbuf, R_OK|W_OK))
		{
			fn = filenode_load(pathbuf);
			if(fn != NULL)
			{
				shr_list = realloc(shr_list, sizeof(FileNode *) * (shr_list_len + 1));
				if(shr_list == NULL)
				{
					perrlog("realloc");
					return -1;
				}
				shr_list[shr_list_len] = fn;
				shr_list_len++;
			}
		}
		
		i++;
	}
	
	return i;
}

/* TODO: realloc return should be different than the passed variable, to prevent memory leaks if it becomes null */

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
