#ifndef FILEOP_H
#define FILEOP_H
#include <stdint.h>
#include "misc.h"
typedef struct FileInfo {
	off_t size;
	uint32_t crc32;
	uint8_t md5[16];
	uint8_t sha256[32];
} FileInfo;

typedef struct FileNode {
   char *file;
   FileInfo *info;
   time_t mtime;
   int packn;
   tox_list tox_lst;
} FileNode;

int file_checksumcalc(FileInfo *dest, char *filename);
int file_checksumcalc_noblock(FileInfo *dest, char *filename);
time_t file_lastmod(char *filename);
#endif
