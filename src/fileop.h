#ifndef CRC32_H
#define CRC32_H
#include <stdint.h>

typedef struct FileInfo {
	off_t size;
	uint32_t crc32;
	uint8_t md5[16];
	uint8_t sha256[32];
} FileInfo;

int file_checksumcalc(FileInfo *dest, char *filename);
int file_checksumcalc_noblock(FileInfo *dest, char *filename);
time_t file_lastmod(char *filename);
#endif
