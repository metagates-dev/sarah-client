/**
 * sarah executor test program
 * 
 * Compile:
 * gcc -g -Wall tst-executor.c node.c network.c bash.c cel_string.c cel_md5.c cel_link.c utils.c executor.c lua.c cJSON.c -I../include/ ../lib/liblua.a ../lib/libcurl.a -lm -ldl -lpthread
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"


int main(int argc, char *argv[])
{
    int i, errno;
    char *cmd[] = {"ls -l", "ls -l /etc/sarah/", NULL};

    sarah_node_t node;
    sarah_executor_t executor;

    // basic initialization
    if ( sarah_node_init(&node, "/etc/sarah/") != 0 ) {
        printf("[Error]: Unable to initialize the node");
        return 1;
    }

    if ( sarah_executor_init(&node, &executor) != 0 ) {
        printf("[Error]: Unable to initialize the executor");
        return 1;
    }

    sarah_executor_set_syntax(&executor, "bash");
    
    i = 0;
    while ( 1 ) {
        if ( cmd[i] == NULL ) {
            break;
        }

        printf("+-Try to execute %s ... \n", cmd[i]);
        sarah_executor_clear(&executor);
        errno = sarah_executor_dostring(&executor, cmd[i]);
        i++;
        if ( errno != 0 ) {
            printf("Error: Execution failed");
            continue;
        }

        printf("execution result=%s\n", sarah_executor_get_data(&executor));
    }

    // bash file execution
    printf("+------ bash file execution ------+\n");
    sarah_executor_clear(&executor);
    errno = sarah_executor_dofile(&executor, "./script_demo.sh");
    if ( errno != 0 ) {
        printf("Error: Execution failed");
        return 1;
    }

    printf("execution result=%s\n", sarah_executor_get_data(&executor));


    // lua string execution
    sarah_executor_set_syntax(&executor, "lua");
    printf("+------ lua string execution ------+\n");
    sarah_executor_clear(&executor);
    errno = sarah_executor_dostring(&executor, "print \"This the output from lua script\"; executor:appendData(\"This is the appended data from lua string executor\");");
    printf("execution result=%s\n", sarah_executor_get_data(&executor));
    printf("execution error=%s\n", sarah_executor_get_errstr(&executor));


    // lua file execution
    printf("+------ lua file execution ------+\n");
    sarah_executor_clear(&executor);
    errno = sarah_executor_dofile(&executor, "./script_demo.lua");
    printf("execution result=%s\n", sarah_executor_get_data(&executor));
    printf("execution error=%s\n", sarah_executor_get_errstr(&executor));


    // resource clean up
    printf("\n+------ resource clean up ------+\n");
    sarah_print_executor(&executor);
    sarah_executor_destroy(&executor);

    return 0;
}
