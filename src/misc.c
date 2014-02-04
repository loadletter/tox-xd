#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
