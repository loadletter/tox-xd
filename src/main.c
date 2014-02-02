#include <stdio.h>
#include <unistd.h>
#include <tox/tox.h>
#include <signal.h>

#include "misc.h"
#include "fileop.h"
#include "conf.h"

#define DHTSERVERS "/tmp/DHTservers"

static void toxconn_do(Tox *m)
{
	static int conn_try = 0;
	static int conn_err = 0;
	static int dht_on = FALSE;

	if (!dht_on && !tox_isconnected(m) && !(conn_try++ % 100))
	{
		if (!conn_err)
		{
			conn_err = init_connection(m, DHTSERVERS);
			INFO("Establishing connection...");
				
			if(conn_err)
				WARNING("Auto-connect failed with error code %i", conn_err);
		}
	}
	else if (!dht_on && tox_isconnected(m))
	{
		dht_on = TRUE;
		INFO("DHT connected.");
	}
	else if (dht_on && !tox_isconnected(m))
	{
		dht_on = FALSE;
		WARNING("DHT disconnected. Attempting to reconnect.");
	}

	tox_do(m);
}

int main(void)
{
	struct sigaction sigu1a;
	sigu1a.sa_handler = file_recheck_callback;
	sigaction(SIGUSR1, &sigu1a, NULL);
	
	while(TRUE)
	{
		file_do("../");
		usleep(20 * 1000);
	}
	return 0;
}
