#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "ylog.h"

void perrlog(const char *msg)
{
	yerr("%s: %s", msg, strerror(errno));
}

// XXX: FIX
unsigned char *hex_string_to_bin(char hex_string[])
{
	size_t len = strlen(hex_string);
	unsigned char *val = malloc(len);

	if (val == NULL)
	{
		perrlog("malloc");
		return NULL;
	}

	char *pos = hex_string;
	size_t i;

	for (i = 0; i < len; ++i, pos += 2)
		sscanf(pos, "%2hhx", &val[i]);

	return val;
}

char *human_readable_id(uint8_t *address, uint16_t length)
{
    char id[length * 2 + 1];
    int i;

    memset(id, 0, sizeof(id));
    for (i = 0; i < length; i++)
        sprintf(id,"%s%02hhX", id, address[i]);
    return strdup(id);
}

/* cut absolute pathname to filename */
char *gnu_basename(char *path)
{
	char *base = strrchr(path, '/');
	return base ? base+1 : path;
}
