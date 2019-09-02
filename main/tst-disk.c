/**
 * disk interface test.
 * 
 * Compile:
 * gcc -g -Wall tst-disk.c disk.c cel_link.c cel_string.c
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>

static int disk_filter(sarah_disk_t *disk, int step)
{
    switch ( step ) {
    case -1:
        if ( strncmp(disk->name, "/dev/loop", 9) == 0 
            || strncmp(disk->name, "/dev/ram", 8) == 0 ) {
            return 1;
        }
        break;
    case SARAH_DISK_FILL_PT:
        if ( sarah_is_valid_disk_type(disk->format) > 0 ) {
            return 1;
        }
        break;
    }

    return 0;
}

int main(int argc, char **argv)
{
    cel_link_t list;
    sarah_disk_t *disk;

    // initialize
    cel_link_init(&list);

    sarah_get_disk_list(
        &list, SARAH_DISK_FILL_PT | SARAH_DISK_FILL_ST, disk_filter
    );

    while ( (disk = cel_link_remove_first(&list)) != NULL ) {
        sarah_print_disk(disk);
        printf("+------------------------+\n");
        free_disk_entry((void **) &disk);
    }

    // do the clean up
    cel_link_destroy(&list, NULL);

    return 0;
}
