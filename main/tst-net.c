/**
 * NETWORK interface test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    cel_link_t list;
    sarah_net_stat_t *net;
    sarah_bandwidth_stat_t *bd;

    // initialize
    cel_link_init(&list);

    printf("Network statistics: \n");
    sarah_get_net_stat(&list);
    while ( (net = cel_link_remove_first(&list)) != NULL ) {
        sarah_print_net_stat(net);
        free_net_stat_entry((void **)&net);
    }

    // Network bandwidth
    printf("Network bandwidth statistics: \n");
    sarah_get_bandwidth_stat(&list);
    while ( (bd = cel_link_remove_first(&list)) != NULL ) {
        sarah_print_bandwidth_stat(bd);
        free_bandwidth_stat_entry((void **)&bd);
    }

    // do the clean up
    cel_link_destroy(&list, NULL);

    return 0;
}
