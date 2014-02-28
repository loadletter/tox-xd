#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
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
/* callback for new file */
static void (*file_new_c)(FileNode *, int) = NULL;
/* prototypes */
static int filenode_dump(FileNode *fnode, char *path);

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
			new_list[new_list_len]->exists = TRUE;
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

/* walks the current shared files and check if they still exist */
void file_exists_shared(void)
{
	int i;
	
	for(i=0;i<shr_list_len;i++)
	{
		if(access(shr_list[i]->file, R_OK) == -1)
		{
			shr_list[i]->exists = FALSE;
			ydebug("Removed non-existing file: %s", shr_list[i]->file);
		}
		else
		{
			shr_list[i]->exists = TRUE;
		}
	}
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
 int file_do(char *shrdir, char *cachedir)
{
	int i, t, rc;
	int n = -1;
	static int hashing = -1;
	static int last;
	
	if(file_recheck && new_list_len == 0 && hashing == -1)
	{
		file_exists_shared();
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
			
			/* execute callback for new file*/
			if(file_new_c != NULL)
				file_new_c(shr_list[last], last);
			yinfo("Hash: %i - %s", last, shr_list[last]->file);
			
			/* write filenode to cache */
			char cachepath[PATH_MAX + 20];
			snprintf(cachepath, sizeof(cachepath), "%s/%i", cachedir, last);
			filenode_dump(shr_list[last], cachepath);
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
static FileNode *filenode_load(char *path)
{
	char md5[33], sha256[65], filename[PATH_MAX + 1];
	char *pos;

	FILE *fp = fopen(path, "rb");
	if(fp == NULL)
	{
		perrlog("fopen");
		return NULL;
	}
	
	FileNode *fn = malloc(sizeof(FileNode));
	if(fn == NULL)
	{
		perrlog("malloc");
		fclose(fp);
		return NULL;
	}
	
	fn->info = malloc(sizeof(FileHash));
	if(fn->info == NULL)
	{
		perrlog("malloc");
		fclose(fp);
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
	filename[rc] = '\0';

	if(ferror(fp))
	{
		perror("fread");
		goto parserr;
	}
	
	if(feof(fp))
	{
		int i;
		pos = md5;
		for (i = 0; i < sizeof(fn->info->md5); ++i, pos += 2)
			sscanf(pos, "%2hhx", &fn->info->md5[i]);
		pos = sha256;
		for (i = 0; i < sizeof(fn->info->sha256); ++i, pos += 2)
			sscanf(pos, "%2hhx", &fn->info->sha256[i]);
		
		/* file_exists_shared will check later if they actually exist */
		fn->exists = FALSE;
		
		fn->file = strdup(filename);
		if(fn->file == NULL)
		{
			perrlog("strdup");
			goto parserr;
		}
		fclose(fp);
		return fn;
	}
	
	parserr:
		yerr("Error parsing: %s", path);
		fclose(fp);
		free(fn->info);
		free(fn);
		return NULL;
}

static int filenode_dump(FileNode *fnode, char *path)
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

static int directory_count(char *path)
{
	DIR * dirp;
	struct dirent * entry;
	int count = 0;
	int i, digitflag;
	
	dirp = opendir(path);
	if(dirp == NULL)
	{
		perrlog("opendir");
		return -1;
	}
	
	while((entry = readdir(dirp)) != NULL)
	{
		digitflag = TRUE;
		if (entry->d_type == DT_REG) /* If the entry is a regular file */
		{
			/* check that there is no extraneus file in the cachedir */
			for(i=0; i<strlen(entry->d_name); i++)
				if(!isdigit(entry->d_name[i]))
				{
					digitflag = FALSE;
					ywarn("Unknown file in cache dir, skipping: %s", entry->d_name);
					break;
				}
			
			if(digitflag)
				count++;
		}
	}
	
	if(closedir(dirp) != 0)
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
	int i = 0;
	
	int dircount = directory_count(cachedir);
	if(dircount == 0)
		return 0;
	if(dircount < 0)
		return -1;
	
	while(i < dircount)
	{
		snprintf(pathbuf, sizeof(pathbuf), "%s/%i", cachedir, i);
		ydebug("%i - %i", i, shr_list_len);
		if(access(pathbuf, R_OK|W_OK) != -1)
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
