#include <stdio.h>
#include <unistd.h>
#include <tox/tox.h>
#include <signal.h>
#include <string.h>

#include "misc.h"
#include "fileop.h"
#include "conf.h"

#define DHTSERVERS "/tmp/DHTservers"

static volatile sig_atomic_t main_loop_running = TRUE;

void main_loop_stop(int signo)
{
	if(signo == SIGINT || signo == SIGTERM)
		main_loop_running = FALSE;
}

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

void on_request(uint8_t *key, uint8_t *data, uint16_t length, void *m)
{
	char *keystr;
	data[length - 1] = '\0'; /* make sure the message is null terminated */
	
	keystr = human_readable_id(key, TOX_CLIENT_ID_SIZE);
	yinfo("Friend request from %s (%s)", keystr, data);
	int friendnum = tox_add_friend_norequest(m, key);
	
	if(friendnum == -1)
		yerr("Failed to add %s", keystr);
	else
		yinfo("Added %s as friend n° %i", keystr, friendnum);
		
	free(keystr);
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
	/*tox_callback_connection_status(m, on_connectionchange, NULL);*/
	tox_callback_friend_request(m, on_request, m);
	/*tox_callback_friend_message(m, on_message, NULL);
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

void printownid(Tox *m)
{
	char *idstr;
	uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
	
	tox_get_address(m, address);
	idstr = human_readable_id(address, TOX_FRIEND_ADDRESS_SIZE);
	
	yinfo("ID: %s", idstr);
	free(idstr);
}


int main(void)
{
	ylog_set_level(YLOG_DEBUG, getenv("YLOG_LEVEL"));
	
	struct sigaction sigu1a;
	memset(&sigu1a, 0, sizeof(sigu1a));
	sigu1a.sa_handler = file_recheck_callback;
	sigaction(SIGUSR1, &sigu1a, NULL);
	
	struct sigaction sigint_term;
	memset(&sigint_term, 0, sizeof(sigint_term));
	sigint_term.sa_handler = main_loop_stop;
	sigaction(SIGINT, &sigint_term, NULL);
	sigaction(SIGTERM, &sigint_term, NULL);
	
	Tox *m = toxconn_init(FALSE);
	printownid(m);
	
	while(main_loop_running)
	{
		file_do("../");
		toxconn_do(m);
		usleep(20 * 1000);
	}
	
	ywarn("SIGINT/SIGTERM received, terminating...");
	
	tox_kill(m);
	
	return 0;
}
