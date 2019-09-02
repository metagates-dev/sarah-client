/**
 * sarah process interface implementation
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


SARAH_API sarah_process_t *new_process_entry()
{
    sarah_process_t *ps = (sarah_process_t *) sarah_malloc(sizeof(sarah_process_t));
    if ( ps == NULL ) {
        SARAH_ALLOCATE_ERROR("new_process_entry", sizeof(sarah_process_t));
    }

    // Zero-Fill The Memory Block
    memset(ps, 0x00, sizeof(sarah_process_t));
    return ps;
}

SARAH_API void free_process_entry(void **ptr)
{
    sarah_process_t **ps = (sarah_process_t **) ptr;
    if ( *ps != NULL ) {
        sarah_free(*ps);
        *ps = NULL;
    }
}

/**
 * implementation to get the process list
 * 1. throught the 'ps -aux' command.
 * 2. iterate all the directory under /proc/ witch its name is pure numeric
 *
 * @param   list
 * @return  int 0 for success or failed
*/
SARAH_API int sarah_get_process_list(cel_link_t *list)
{
    char buff[1024] = {'\0'};
    char *cmd = "ps -aux";
    FILE *fd;
    sarah_process_t *ps;

    fd = popen(cmd, "r");
    if ( fd == NULL ) {
        return 1;
    }

    // clear the first line
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    // Loop and analysis all the other lines
    while ( 1 ) {
        if ( fgets(buff, sizeof(buff), fd) == NULL ) {
            break;
        }

        // Create a process entry and initialize it with
        // the parsed data
        ps = new_process_entry();
        sscanf(
            buff,
            // USER, PID, %CPU, %MEM, VSZ, RSS, TTY, STAT, START, TIME, COMMAND
            "%31s %u %f %f %u %u %15s %15s %15s %15s %255[^\n]",
            ps->user, &ps->pid, &ps->cpu_percent, &ps->mem_percent,
            &ps->vir_mem, &ps->rss_mem, ps->tty, ps->status, 
            ps->start, ps->time, ps->command
        );

        // sarah_print_process(ps);
        // clear the ps -aux command itself
        if ( strcmp(ps->command, "ps -aux") == 0 ) {
            // ignore
            free_process_entry((void **)&ps);
        } else if ( strcmp(ps->command, "sh -c ps -aux") == 0 ) {
            // ignore
            free_process_entry((void **)&ps);
        } else {
            cel_link_add_last(list, ps);
        }
    }

    fclose(fd);
    return 0;
}

SARAH_API void sarah_print_process(const sarah_process_t *ps)
{
    printf(
        "user=%s, pid=%u, cpu_percent=%f, mem_percent=%f",
        ps->user, ps->pid, ps->cpu_percent, ps->mem_percent
    );

    printf(
        "vir_mem=%d, rss_mem=%u, TTY=%s, STAT=%s, START=%s",
        ps->vir_mem, ps->rss_mem, ps->tty, ps->status, ps->start
    );

    printf(
        "time=%s, CMD=%s",
        ps->time, ps->command
    );
}
