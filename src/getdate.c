
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>

int
main(ac, av)
  int ac;
  char **av;
{
    struct stat stat_buf;

    if(ac != 2)
	return 10;

    if(!stat(av[1], &stat_buf))
    {
	char buf[100];
	strftime(buf, 99, "%m%d%H%M%y", gmtime(&stat_buf.st_mtime));
	puts(buf);
	return 0;
    }
    return 10;
}
