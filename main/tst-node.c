/**
 * Node interface test
 *
 * Compile:
 * gcc -g -Wall tst-node.c node.c cel_string.c cel_link.c network.c utils.c cJSON.c
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sarah.h"

int main(int argc, char **argv)
{
    sarah_node_t node;

    if ( sarah_node_init(&node, "/etc/sarah/") != 0 ) {
        printf("Unable to initialize the node\n");
        return 1;
    }

    sarah_print_node(&node);
    sarah_node_destroy(&node, 0);
    return 0;
}
