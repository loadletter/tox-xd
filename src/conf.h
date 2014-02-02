#ifndef CONF_H
#define CONF_H
int init_connection(Tox *m, char *DHTservers_path);

int toxdata_load(Tox *m, char *path);
int toxdata_store(Tox *m, char *path);

#endif
