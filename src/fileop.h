#ifndef FILEOP_H
#define FILEOP_H
#include <stdint.h>
#include "misc.h"
typedef struct FileHash {
	uint32_t crc32;
	uint8_t md5[16];
	uint8_t sha256[32];
} FileHash;

typedef struct FileNode {
	char *file;
	FileHash *info;
	time_t mtime;
	off_t size;
} FileNode;

int file_checksumcalc(FileHash *dest, char *filename);
int file_checksumcalc_noblock(FileHash *dest, char *filename);
time_t file_lastmod(char *filename);
#endif
