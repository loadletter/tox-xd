#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "fileop.h"
#include "crypto/crc32.h"
#include "crypto/md5.h"
#include "crypto/sha256.h"

#define HASHING_BUFSIZE 64 * 1024

int file_checksumcalc_noblock(FileInfo *dest, char *filename)
{
	static FILE *f = NULL;
	uint i;
	int rc;
	static uint fsize = 0;
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
		
		fsize = 0;
		crc32 = 0;
		MD5_Init(&md5_ctx);
		sha256_starts(&sha_ctx);
	}
	
	
	if((i = fread(buf, 1, sizeof(buf), f)) > 0)
	{
		fsize += i;
		crc32 = crc32_compute(crc32, buf, i);
		MD5_Update(&md5_ctx, buf, i);
		sha256_update(&sha_ctx, buf, i);
		rc = i;
	}
	else
	{
		dest->size = fsize;
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

int file_checksumcalc(FileInfo *dest, char *filename)
{
	FILE *f;
	uint i = 0, fsize = 0;
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
		fsize += i;
		crc32 = crc32_compute(crc32, buf, i);
		MD5_Update(&md5_ctx, buf, i);
		sha256_update(&sha_ctx, buf, i);
	}
	
	dest->size = fsize;
	dest->crc32 = crc32;
	MD5_Final(dest->md5, &md5_ctx);
	sha256_finish(&sha_ctx, dest->sha256);
	
	if(fclose(f) != 0)
		perror("fclose");
	
	return(0);
}

time_t file_lastmod(char *filename)
{
	struct stat s;
	if(stat(filename, &s) != 0)
	{
		perror("stat");
		return -1;
	}
	
	return s.st_mtime;
}

/* list dir in posix os
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

int
main (void)
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir ("./");
	if (dp != NULL)
	 {
	   while (ep = readdir (dp))
		 puts (ep->d_name);
	   (void) closedir (dp);
	 }
	else
	 perror ("Couldn't open the directory");
	
	printf("%i\n", NAME_MAX);

	return 0;
}
*/

/* test checksumcalc 
 * gcc crypto/sha256.c crypto/md5.c crypto/crc32.c fileop.c -o fileop -Wall -march=native -O3*/
#if 0
static void printhash(FileInfo fi, int rc)
{
	int i;
	putchar('\n');
	printf("%i - %lu\n", rc, fi.size);
	printf("crc32: %.2X\nmd5: ", fi.crc32);
	for(i=0;i<16;i++)
		printf("%.2X", fi.md5[i]);
	printf("\nsha256: ");
	for(i=0;i<32;i++)
		printf("%.2X", fi.sha256[i]);
	putchar('\n');
}

int main()
{
	FileInfo test;
	int rc;
	char testfile[] = "./fileop.c";
	
	printf("%ld\n", file_lastmod(testfile));
	rc = file_checksumcalc(&test, testfile);
	printhash(test, rc);
		
	while((rc = file_checksumcalc_noblock(&test, testfile)) != 0);
		//printf("%i ", rc);
	printhash(test, rc);
	
	return 0;
}
#endif
