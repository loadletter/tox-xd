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
