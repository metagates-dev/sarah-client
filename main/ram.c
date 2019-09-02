/**
 * sarah ram statistics interface implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// interface to get the RAM statistics information
SARAH_API int sarah_get_ram_stat(sarah_ram_stat_t *m)
{
    FILE *fd;
    char buff[256] = {'\0'};

    fd = fopen("/proc/meminfo", "r");
    if ( fd == NULL ) {
        return 1;
    }

    // get the total value
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    sscanf(buff, "%*s %ld", &m->total);

    // get the free value
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    sscanf(buff, "%*s %ld", &m->free);

    // get the available value
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    sscanf(buff, "%*s %ld", &m->available);

    fclose(fd);
    return 0;
}

SARAH_API void sarah_print_ram_stat(const sarah_ram_stat_t *m)
{
    printf(
        "Total=%ld, Free=%ld, Available=%ld\n", 
        m->total, m->free, m->available
    );
}
