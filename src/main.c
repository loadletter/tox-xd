#include <stdio.h>
#include <unistd.h>
#include <tox/tox.h>
#include <signal.h>
#include "misc.h"
#include "fileop.h"

int main(void)
{
	int i = 0;
	/*struct sigaction sigu1a;
	sigu1a.sa_handler = file_recheck_callback;
	sigaction(SIGUSR1, &sigu1a, NULL);*/
	
	while(TRUE)
	{
		if(i == 100)
			file_recheck_callback(SIGUSR1);
		file_do("../");
		usleep(20 * 1000);
		i++;
	}
	return 0;
}
