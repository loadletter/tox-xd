#ifndef MISC_H
#define MISC_H

#include "ylog.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

void perrlog(const char *msg);
unsigned char *hex_string_to_bin(char hex_string[]);
char *human_readable_id(uint8_t *address, uint16_t length);
char *gnu_basename(char *path);
#endif
