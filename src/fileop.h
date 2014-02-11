#ifndef FILEOP_H
#define FILEOP_H
#include <stdint.h>
#include "misc.h"

void file_recheck_callback(int signo);
int file_do(char *shrdir);

void file_new_set_callback(void (*func)(FileNode *, int));

FileNode **file_get_shared(void);
int file_get_shared_len(void);
#endif
