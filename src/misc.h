#ifndef MISC_H
#define MISC_H
#include <errno.h>
#include "ylog.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define perrlog(msg) yerr("%s: %s", msg, strerror(errno))

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
	int exists;
} FileNode;

unsigned char *hex_string_to_bin(char hex_string[]);
char *human_readable_id(uint8_t *address, uint16_t length);
char *gnu_basename(char *path);
void human_readable_filesize(char *dest, off_t size);
#endif
