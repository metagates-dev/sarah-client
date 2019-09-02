/**
 * cpu interface test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sarah.h"

int main(int argc, char **argv)
{
    cel_link_t list, *link;
    sarah_cpu_info_t *info;
    sarah_cpu_time_t t1, t2;
    // float loadavg = 0;
    sarah_cpu_stat_t stat;
    sarah_cpu_loadavg_t loadavg;

    // initialize
    link = &list;
    cel_link_init(link);

    // get the cpu information
    printf("CPU information: \n");
    sarah_get_cpu_info(&list);
    while ( cel_link_size(link) > 0 ) {
        info = (sarah_cpu_info_t *) cel_link_remove_first(&list);
        sarah_print_cpu_info(info);
        free_cpu_info_entry((void **)&info);
    }

    // get the cpu statistics information
    printf("CPU statistics information: \n");
    sarah_get_cpu_time(&t1);
    sleep(1);
    sarah_get_cpu_time(&t2);

    // loadavg = sarah_cal_cpu_occupy(&t1, &t2);
    // sarah_print_cpu_time(&t1);
    // sarah_print_cpu_time(&t2);
    // printf("load average: %f\n", loadavg);
    sarah_cal_cpu_stat(&stat, &t1, &t2);
    sarah_print_cpu_stat(&stat);

    sarah_get_cpu_loadavg(&loadavg);
    sarah_print_cpu_loadavg(&loadavg);
    printf(" --[Done]\n");

    // do the clean up
    cel_link_destroy(&list, NULL);

    return 0;
}
