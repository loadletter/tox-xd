#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tox/tox.h>
#include "misc.h"

#define MINLINE    50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAXLINE   256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXSERVERS 50
#define SERVERLEN (MAXLINE - TOX_CLIENT_ID_SIZE - 7)

static int  linecnt = 0;
static char servers[MAXSERVERS][SERVERLEN];
static uint16_t ports[MAXSERVERS];
static uint8_t keys[MAXSERVERS][TOX_CLIENT_ID_SIZE];

static int serverlist_load(char *filepath)
{
	FILE *fp = NULL;

	fp = fopen(filepath, "r");

	if (fp == NULL)
	{
		perrlog("fopen");
		return 1;
	}
	
	char line[MAXLINE];
	while (fgets(line, sizeof(line), fp) && linecnt < MAXSERVERS)
	{
		if (strlen(line) > MINLINE)
		{
			char *name = strtok(line, " ");
			char *port = strtok(NULL, " ");
			char *key_ascii = strtok(NULL, " ");
			/* invalid line */
			if (name == NULL || port == NULL || key_ascii == NULL)
				continue;

			strncpy(servers[linecnt], name, SERVERLEN);
			servers[linecnt][SERVERLEN - 1] = 0;
			ports[linecnt] = htons(atoi(port));

			uint8_t *key_binary = hex_string_to_bin(key_ascii);
			memcpy(keys[linecnt], key_binary, TOX_CLIENT_ID_SIZE);
			free(key_binary);

			linecnt++;
		}
	}
	
	if (linecnt < 1)
		return 2;
		
	return 0;
}

static int init_connection_helper(Tox *m, int linenumber)
{
	return tox_bootstrap_from_address(m, servers[linenumber], TOX_ENABLE_IPV6_DEFAULT,
												ports[linenumber], keys[linenumber]);
}

/* Connects to a random DHT server listed in the DHTservers file
 *
 * return codes:
 * 1: failed to open server file
 * 2: no line of sufficient length in server file
 * 3: (old, removed) failed to split a selected line in the server file
 * 4: failed to resolve name to IP
 * 5: serverlist file contains no acceptable line
 */
int init_connection(Tox *m, char *DHTservers_path)
{
	static uint8_t init_connection_serverlist_loaded = 0;
	
	if (linecnt > 0) /* already loaded serverlist */
		return init_connection_helper(m, rand() % linecnt) ? 0 : 4;

	/* only once:
	 * - load the serverlist
	 * - connect to "everyone" inside
	 */
	if (!init_connection_serverlist_loaded)
	{
		init_connection_serverlist_loaded = TRUE;
		int res = serverlist_load(DHTservers_path);
		if (res)
			return res;

		if (!linecnt)
			return 4;

		res = 6;
		int linenumber;
		for(linenumber = 0; linenumber < linecnt; linenumber++)
			if (init_connection_helper(m, linenumber))
				res = 0;

		return res;
	}

	/* empty serverlist file */
	return 5;
}


/*
 * Store Messenger to given location
 * Return 0 stored successfully
 * Return 1 file path is NULL
 * Return 2 malloc failed
 * Return 3 opening path failed
 * Return 4 fwrite failed
 */
int toxdata_store(Tox *m, char *path)
{
	if (path == NULL)
		return 1;

	FILE *fd;
	size_t len;
	uint8_t *buf;

	len = tox_size(m);
	buf = malloc(len);

	if (buf == NULL)
	{
		perrlog("malloc");
		return 2;
	}

	tox_save(m, buf);

	fd = fopen(path, "wb");
	if (fd == NULL)
	{
		perrlog("fopen");
		free(buf);
		return 3;
	}

	if (fwrite(buf, len, 1, fd) != 1)
	{
		perrlog("fwrite");
		free(buf);
		fclose(fd);
		return 4;
	}

	free(buf);
	fclose(fd);
	return 0;
}

/*
 * Load Messenger from given location, create new if location doesn't exist
 * Return 0 loaded successfully
 * Return 2 malloc failed
 * Return 4 fread failed
 * Return > 5 error storing new
 */
int toxdata_load(Tox *m, char *path)
{
	FILE *fd;
	size_t len;
	uint8_t *buf;
	int st;

	fd = fopen(path, "rb");
	if (fd != NULL)
	{
		fseek(fd, 0, SEEK_END);
		len = ftell(fd);
		fseek(fd, 0, SEEK_SET);

		buf = malloc(len);

		if (buf == NULL)
		{
			fclose(fd);
			perrlog("malloc");
			return 2;
		}

		if (fread(buf, len, 1, fd) != 1)
		{
			free(buf);
			fclose(fd);
			perrlog("fread");
			return 4;
		}

		tox_load(m, buf, len);
		
		/* TODO: callback
		uint32_t i = 0;
		uint8_t name[TOX_MAX_NAME_LENGTH];

		while (tox_get_name(m, i, name) != -1)
			on_friendadded(m, i++, false);*/

		free(buf);
		fclose(fd);
		
		return 0;
	}

	/* save current */
	st = toxdata_store(m, path);
	if (st != 0)
		return 5 + st;
	
	return 0;
}

/*
 * Get a configuration value from a key=value configuration file
 * Returns pointer to allocated memory if success
 * Return NULL if keyname is not found or on error
 */
char *get_conf_str(FILE *fp, const char *keyname)
{
	if (fp == NULL)
		return NULL;
	fseek(fp, 0, SEEK_SET);
	
	char line[4096];
	while (fgets(line, sizeof(line), fp))
	{
		char *key = strtok(line, "=");
		char *value = strtok(NULL, "\n");
		/* invalid line */
		if (key == NULL || value == NULL)
			continue;
		/* not what we are looking for or commented out */
		if (strcmp(keyname, key) != 0 || strchr(key, '#') != NULL)
			continue;
		
		return strdup(value);
	}
	
	return NULL;
}

/*
 * Get a positive number value from a key=value configuration file
 * Returns integer >= 0 if success
 * Return -1 if keyname is not found or on error
 */
int get_conf_int(FILE *fp, const char *keyname)
{
	char *strnum = get_conf_str(fp, keyname);
	if (strnum == NULL)
		return -1;
	
	int number = atoi(strnum);
	if (number == 0 && strcmp(strnum, "0") != 0)
		number = -1;
	
	free(strnum);
	return number;
}
