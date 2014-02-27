#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "misc.h"

// XXX: FIX
/* return value needs to be freed */
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
/* return value needs to be freed */
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

/* dest must be at least 8 bytes */
/* the output size isn't accurate */
void human_readable_filesize(char *dest, off_t size)
{
	char unit_letter[] = " KMGTPEZY";
	int unit_prefix = 0;
	
	while(size > 9999)
	{
		size /= 1024;
		unit_prefix++;
	}
		
	snprintf(dest, 8, "%lu %cb", size, unit_letter[unit_prefix]);
}
