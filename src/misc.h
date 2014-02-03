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
#endif
