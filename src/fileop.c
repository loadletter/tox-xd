/* list dir in posix os
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

int
main (void)
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir ("./");
	if (dp != NULL)
	 {
	   while (ep = readdir (dp))
		 puts (ep->d_name);
	   (void) closedir (dp);
	 }
	else
	 perror ("Couldn't open the directory");
	
	printf("%i\n", NAME_MAX);

	return 0;
}*/
