#include "filesend.h"

uint8_t file_senders_index;
FileSender file_senders[FSEND_PIECE_SIZE];

void file_sender_close(int i)
{
    fclose(file_senders[i].file);
    memset(&file_senders[i], 0, sizeof(FileSender));

    int j;

    for (j = max_file_senders_index; j > 0; --j) {
        if (file_senders[j-1].active)
            break;
    }

    max_file_senders_index = j;
}

/*
void do_file_senders(Tox *m)
{
    int i;

    for (i = 0; i < file_senders_index; ++i) {
        if (!file_senders[i].active)
            continue;

        uint8_t *pathname = file_senders[i].pathname;
        uint8_t filenum = file_senders[i].filenum;
        int friendnum = file_senders[i].friendnum;
        FILE *fp = file_senders[i].file;
        uint64_t current_time = (uint64_t) time(NULL);

        // If file transfer has timed out kill transfer and send kill control
        if (timed_out(file_senders[i].timestamp, current_time, TIMEOUT_FILESENDER)) {
            ChatContext *ctx = file_senders[i].toxwin->chatwin;

            if (ctx != NULL) {
                wprintw(ctx->history, "File transfer for '%s' timed out.\n", pathname);
                alert_window(file_senders[i].toxwin, WINDOW_ALERT_2, true);
            }

            tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_KILL, 0, 0);
            close_file_sender(i);
            continue;
        }

        int pieces = 0;

        while (pieces++ < MAX_PIECES_SEND) {
            if (tox_file_send_data(m, friendnum, filenum, file_senders[i].nextpiece, 
                                   file_senders[i].piecelen) == -1)
                break;

            file_senders[i].timestamp = current_time;
            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1, 
                                             tox_file_data_size(m, friendnum), fp);

            if (file_senders[i].piecelen == 0) {
                ChatContext *ctx = file_senders[i].toxwin->chatwin;

                if (ctx != NULL) {
                    wprintw(ctx->history, "File '%s' successfuly sent.\n", pathname);
                    alert_window(file_senders[i].toxwin, WINDOW_ALERT_2, true);
                }

                tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_FINISHED, 0, 0);
                close_file_sender(i);
                break;
            }
        }
    }
}
*/

/*
void cmd_sendfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (max_file_senders_index >= (MAX_FILES-1)) {
        wprintw(window,"Please wait for some of your outgoing file transfers to complete.\n");
        return;
    }

    if (argc < 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    uint8_t *path = argv[1];

    if (path[0] != '\"') {
        wprintw(window, "File path must be enclosed in quotes.\n");
        return;
    }

    path[strlen(++path)-1] = L'\0';
    int path_len = strlen(path);

    if (path_len > MAX_STR_SIZE) {
        wprintw(window, "File path exceeds character limit.\n");
        return;
    }

    FILE *file_to_send = fopen(path, "r");

    if (file_to_send == NULL) {
        wprintw(window, "File '%s' not found.\n", path);
        return;
    }

    fseek(file_to_send, 0, SEEK_END);
    uint64_t filesize = ftell(file_to_send);
    fseek(file_to_send, 0, SEEK_SET);

    int filenum = tox_new_file_sender(m, self->num, filesize, path, path_len + 1);

    if (filenum == -1) {
        wprintw(window, "Error sending file.\n");
        return;
    }

    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (!file_senders[i].active) {
            memcpy(file_senders[i].pathname, path, path_len + 1);
            file_senders[i].active = true;
            file_senders[i].toxwin = self;
            file_senders[i].file = file_to_send;
            file_senders[i].filenum = (uint8_t) filenum;
            file_senders[i].friendnum = self->num;
            file_senders[i].timestamp = (uint64_t) time(NULL);
            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                             tox_file_data_size(m, self->num), file_to_send);

            wprintw(window, "Sending file: '%s'\n", path);

            if (i == max_file_senders_index)
                ++max_file_senders_index;

            return;
        }
    } 
}
*/

/* This should only be called on exit */
void close_file_transfers(void)
{
    int i;

    for (i = 0; i < max_file_senders_index; ++i) {
        if (file_senders[i].active)
            fclose(file_senders[i].file);
    }
}
