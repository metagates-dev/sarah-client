/**
 * DISK interface test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    cel_link_t list;
    sarah_disk_stat_t *disk;

    // initialize
    cel_link_init(&list);

    sarah_get_disk_stat(&list);
    while ( (disk = cel_link_remove_first(&list)) != NULL ) {
        sarah_print_disk_stat(disk);
        free_disk_stat_entry(&disk);
    }

    // do the clean up
    cel_link_destroy(&list, NULL);

    return 0;
}
