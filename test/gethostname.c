/*
 * get host name
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char hostname[60] = {'\0'};
    int ret = gethostname(hostname, sizeof(hostname));

    printf("ret: %d, hostname: %s\n", ret, hostname);

    return 0;
}
