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
			yinfo("Establishing connection...");
				
			if(conn_err)
				yerr("Auto-connect failed with error code %i", conn_err);
		}
	}
	else if (!dht_on && tox_isconnected(m))
	{
		dht_on = TRUE;
		yinfo("DHT connected.");
	}
	else if (dht_on && !tox_isconnected(m))
	{
		dht_on = FALSE;
		ywarn("DHT disconnected. Attempting to reconnect.");
	}

	tox_do(m);
}

static Tox *toxconn_init(int ipv4)
{
	/* Init core */
	int ipv6 = !ipv4;
	Tox *m = tox_new(ipv6);

	if (TOX_ENABLE_IPV6_DEFAULT && m == NULL)
	{
		yerr("IPv6 didn't initialize, trying IPv4 only");
		m = tox_new(0);
	}

	if (m == NULL)
		return NULL;

	/* Callbacks */
	/*tox_callback_connection_status(m, on_connectionchange, NULL);
	tox_callback_friend_request(m, on_request, NULL);
	tox_callback_friend_message(m, on_message, NULL);
	tox_callback_name_change(m, on_nickchange, NULL);
	tox_callback_user_status(m, on_statuschange, NULL);
	tox_callback_status_message(m, on_statusmessagechange, NULL);
	tox_callback_friend_action(m, on_action, NULL);
	tox_callback_group_invite(m, on_groupinvite, NULL);
	tox_callback_group_message(m, on_groupmessage, NULL);
	tox_callback_group_action(m, on_groupaction, NULL);
	tox_callback_group_namelist_change(m, on_group_namelistchange, NULL);
	tox_callback_file_send_request(m, on_file_sendrequest, NULL);
	tox_callback_file_control(m, on_file_control, NULL);
	tox_callback_file_data(m, on_file_data, NULL);*/

	tox_set_name(m, (uint8_t *) "Cool bot", sizeof("Cool bot"));

	return m;
}

int main(void)
{
	ylog_set_level(YLOG_DEBUG, getenv("YLOG_LEVEL"));
	
	struct sigaction sigu1a;
	sigu1a.sa_handler = file_recheck_callback;
	sigaction(SIGUSR1, &sigu1a, NULL);
	
	Tox *m = toxconn_init(FALSE);
	
	while(TRUE)
	{
		file_do("../");
		toxconn_do(m);
		usleep(20 * 1000);
	}
	
	tox_kill(m);
	
	return 0;
}
