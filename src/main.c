#include <stdio.h>
#include <unistd.h>
#include <tox/tox.h>
#include <signal.h>
#include "misc.h"
#include "fileop.h"

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
