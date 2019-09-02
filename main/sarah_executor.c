/**
 * sarah lua based script language -- April executor test program
 * 
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sarah.h"

int main(int argc, char *argv[])
{
    int i, errno;
    char *engine = NULL, *file = NULL, *args = NULL;

    sarah_node_t node;
    sarah_executor_t executor;

    for ( i = 0; i < argc; i++ ) {
        if ( (i + 1) < argc ) {
            if ( strcmp(argv[i], "-e") == 0 ) {
                engine = argv[i+1];
            } else if ( strcmp(argv[i], "-f") == 0 ) {
                file = argv[i+1];
            } else if ( strcmp(argv[i], "-a") == 0 ) {
                args = argv[i+1];
            }
        }
    }

    if ( engine == NULL || file == NULL ) {
        printf("Usage: %s -e engine name -f script path -a string arguments\n", argv[0]);
        return 0;
    }

    // basic initialization
    if ( sarah_node_init(&node, "/etc/sarah/") != 0 ) {
        printf("[Error]: Unable to initialize the node");
        return 1;
    }

    if ( sarah_executor_init(&node, &executor) != 0 ) {
        printf("[Error]: Unable to initialize the executor");
        return 1;
    }

    // lua string execution
    sarah_executor_set_syntax(&executor, engine);
    sarah_executor_clear(&executor);
    errno = sarah_executor_dofile(&executor, file, args);
    printf(
        "errno=%d, errstr=%s, result=%s\n", 
        errno, 
        sarah_executor_get_errstr(&executor),
        sarah_executor_get_data(&executor)
    );

    // resource clean up
    // printf("\n+------ resource clean up ------+\n");
    // sarah_print_executor(&executor);
    sarah_executor_destroy(&executor);
    sarah_node_destroy(&node, 0);

    return 0;
}
