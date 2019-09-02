/**
 * Linux bash executor implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sarah.h"
#include "sarah_utils.h"

/**
 * Basic bash command executor.
 *
 * @param   executor
 * @param   cmd
 * @param   argv
*/
SARAH_API int script_bash_dostring(sarah_executor_t *executor, char *cmd, char *argv)
{
    char buffer[1024];
    FILE *fd = NULL;

    if ( (fd = popen(cmd, "r")) == NULL ) {
        return 1;
    }

    // read the output of the execution
    while ( 1 ) {
        if ( fgets(buffer, sizeof(buffer), fd) == NULL ) {
            break;
        }

        // max limitation checking
        // if ( maxlen < 0 ) {
        //     // ignore
        // } else if ( sarah_executor_data_available(executor, strlen(buffer)) == 1 ) {
        //     break;
        // }

        // Fail to append the buffer or not enough space for it
        // here we just broke the read operation ...
        if ( sarah_executor_data_append(executor, buffer) != 0 ) {
            break;
        }
    }

    pclose(fd);
    return 0;
}

/**
 * Basic bash file executor, need x privileges of the source bash file.
 *
 * @param   executor
 * @param   file
 * @param   argv
*/
SARAH_API int script_bash_dofile(sarah_executor_t *executor, char *file, char *argv)
{
    char cmd[256] = {'\0'};

    // check if the specified file is executable or not
    if ( access(file, X_OK) == -1 ) {
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "bash %s", file);
    return script_bash_dostring(executor, cmd, argv);
}
