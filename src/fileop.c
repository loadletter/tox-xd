#include <sys/types.h>
#include <stdio.h>
#include "fileop.h"
#include "crypto/crc32.h"
#include "crypto/md5.h"
#include "crypto/sha256.h"

#define HASHING_BUFSIZE 64 * 1024

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
		return(1);
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
	
	return(0);
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
 * gcc crypto/sha256.c crypto/md5.c crypto/crc32.c fileop.c -o fileop -Wall -march=native -O3
int main()
{
	FileInfo test;
	int rc, i;
	
	rc = file_checksumcalc(&test, "./fileop.c");
	printf("%i - %ull\n", rc, test.size);
	printf("crc32: %.2X\nmd5: ", test.crc32);
	for(i=0;i<16;i++)
		printf("%.2X", test.md5[i]);
	printf("\nsha256: ");
	for(i=0;i<32;i++)
		printf("%.2X", test.sha256[i]);
	putchar('\n');
	return 0;
}
*/
