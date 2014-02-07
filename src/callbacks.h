#ifndef CALLBACKS_H
#define CALLBACKS_H

int group_chat_init(Tox *m);
void on_request(uint8_t *key, uint8_t *data, uint16_t length, void *m);
void on_message(Tox *m, int friendnum, uint8_t *string, uint16_t length, void *userdata);
void on_new_file(FileNode *fn);
#endif
