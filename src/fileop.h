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

void file_recheck_callback(int signo);
int file_do(char *shrdir);

void file_new_set_callback(void (*func)(FileNode *));
#endif
