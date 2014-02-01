#ifndef FILESEND_H
#define FILESEND_H
#include <time.h>

#include "fileop.h"

#define MAX_FILES 256 /* filenum in Tox API is uint8_t */
#define FILE_PIECE_SIZE 1024
#define TIMEOUT_FILESENDER 300
#define MAX_PIECES_SEND 100 /* Max number of pieces to send per file per call to do_file_senders() */

/* Different than toxic! */
typedef struct {
    FILE *file;
    int friendnum;
    bool active;
    uint8_t filenum;
    uint8_t nextpiece[FILE_PIECE_SIZE];
    uint16_t piecelen;
    FileNode *details;
    time_t timestamp;
} FileSender;

#endif
