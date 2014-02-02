#define MINLINE    50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAXLINE   256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXSERVERS 50
#define SERVERLEN (MAXLINE - TOX_CLIENT_ID_SIZE - 7)

static int  linecnt = 0;
static char servers[MAXSERVERS][SERVERLEN];
static uint16_t ports[MAXSERVERS];
static uint8_t keys[MAXSERVERS][TOX_CLIENT_ID_SIZE];

int serverlist_load(void)
{
	FILE *fp = NULL;

	fp = fopen(SRVLIST_FILE, "r");

	if (fp == NULL)
		return 1;

	char line[MAXLINE];
	while (fgets(line, sizeof(line), fp) && linecnt < MAXSERVERS) {
		if (strlen(line) > MINLINE) {
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

	if (linecnt < 1) {
		fclose(fp);
		return 2;
	}

	fclose(fp);
	return 0;
}

int init_connection_helper(Tox *m, int linenumber)
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
static uint8_t init_connection_serverlist_loaded = 0;
int init_connection(Tox *m)
{
	if (linecnt > 0) /* already loaded serverlist */
		return init_connection_helper(m, rand() % linecnt) ? 0 : 4;

	/* only once:
	 * - load the serverlist
	 * - connect to "everyone" inside
	 */
	if (!init_connection_serverlist_loaded) {
		init_connection_serverlist_loaded = 1;
		int res = serverlist_load();
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
int store_data(Tox *m, char *path)
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
		perror("malloc");
		return 2;
	}

	tox_save(m, buf);

	fd = fopen(path, "wb");
	if (fd == NULL)
	{
		perror("fopen");
		free(buf);
		return 3;
	}

	if (fwrite(buf, len, 1, fd) != 1)
	{
		perror("fwrite");
		free(buf);
		fclose(fd);
		return 4;
	}

	free(buf);
	fclose(fd);
	return 0;
}
/*
int load_data(Tox *m, char *path)
{
	FILE *fd;
	size_t len;
	uint8_t *buf;

	if ((fd = fopen(path, "rb")) != NULL) {
		fseek(fd, 0, SEEK_END);
		len = ftell(fd);
		fseek(fd, 0, SEEK_SET);

		buf = malloc(len);

		if (buf == NULL) {
			fclose(fd);
			endwin();
			fprintf(stderr, "malloc() failed. Aborting...\n");
			exit(EXIT_FAILURE);
		}

		if (fread(buf, len, 1, fd) != 1) {
			free(buf);
			fclose(fd);
			endwin();
			fprintf(stderr, "fread() failed. Aborting...\n");
			exit(EXIT_FAILURE);
		}

		tox_load(m, buf, len);

		uint32_t i = 0;
		uint8_t name[TOX_MAX_NAME_LENGTH];

		while (tox_get_name(m, i, name) != -1)
			on_friendadded(m, i++, false);

		free(buf);
		fclose(fd);
	} else {
		int st;

		if ((st = store_data(m, path)) != 0) {
			endwin();
			fprintf(stderr, "Store messenger failed with return code: %d\n", st);
			exit(EXIT_FAILURE);
		}
	}
}
*/
