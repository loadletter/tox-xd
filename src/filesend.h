#ifndef FILESEND_H
#define FILESEND_H
#include <time.h>
#include <stdint.h>

#include "fileop.h"
#include "misc.h"

#define FSEND_MAX_FILES 256 /* filenum in Tox API is uint8_t */
#define FSEND_PIECE_SIZE 1024
#define FSEND_TIMEOUT 300
#define FSEND_MAX_PIECES 100 /* Max number of pieces to send per file per call to do_file_senders() */

/* Different than toxic! */
typedef struct {
    FILE *file;
    int friendnum;
    uint8_t active;
    uint8_t filenum;
    uint8_t nextpiece[FSEND_PIECE_SIZE];
    uint16_t piecelen;
    FileNode *details;
    time_t timestamp;
} FileSender;

#endif
