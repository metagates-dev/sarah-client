/**
 * CPU statistics and information interface implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


SARAH_API float sarah_cal_cpu_occupy(
    const sarah_cpu_time_t *s1, const sarah_cpu_time_t *s2)
{
    long double s1_cost, s2_cost;
    long double u1_cost, u2_cost; 

    // cal the cpu time of s1 and s2
    s1_cost = s1->user + s1->nice + s1->system + s1->idle;
    s2_cost = s2->user + s2->nice + s2->system + s2->idle;

    if ( s2_cost - s1_cost == 0 ) {
        return 0.0;
    }

    // cal user and the system cpu time diff 
    u1_cost = s1->user + s1->nice + s1->system;
    u2_cost = s2->user + s2->nice + s2->system;
    return (float)( ((u2_cost - u1_cost) / (s2_cost - s1_cost)) * 100 );
}

SARAH_API int sarah_get_cpu_time(sarah_cpu_time_t *s)
{
    FILE *fd;
    char buff[256] = {'\0'};

    fd = fopen("/proc/stat", "r");
    if ( fd == NULL ) {
        return 1;
    }

    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    sscanf(
        buff, 
        "%15s %u %u %u %u", 
        s->name, &s->user, &s->nice, &s->system, &s->idle
    );

    fclose(fd);
    return 0;
}

SARAH_API void sarah_print_cpu_time(const sarah_cpu_time_t *s)
{
    printf(
        "name=%s, user=%u, nice=%u, system=%u, idle=%u\n", 
        s->name, s->user, s->nice, s->system, s->idle
    );
}

SARAH_API void sarah_cal_cpu_stat(
    sarah_cpu_stat_t *stat, sarah_cpu_time_t *t1, sarah_cpu_time_t *t2)
{
    memcpy(&stat->round_1, t1, sizeof(sarah_cpu_time_t));
    memcpy(&stat->round_2, t2, sizeof(sarah_cpu_time_t));
    stat->load_avg = sarah_cal_cpu_occupy(t1, t2);
}

SARAH_API void sarah_print_cpu_stat(sarah_cpu_stat_t *stat)
{
    printf("round_1: \n");
    sarah_print_cpu_time(&stat->round_1);
    printf("round_2: \n");
    sarah_print_cpu_time(&stat->round_2);
    printf("load_avg=%f\n", stat->load_avg);
}



SARAH_API sarah_cpu_info_t *new_cpu_info_entry()
{
    sarah_cpu_info_t *info = (sarah_cpu_info_t *) sarah_malloc(sizeof(sarah_cpu_info_t));
    if ( info == NULL ) {
        SARAH_ALLOCATE_ERROR("new_cpu_info_entry", sizeof(sarah_cpu_info_t));
    }

    memset(info, 0x00, sizeof(sarah_cpu_info_t));
    return info;
}

SARAH_API void free_cpu_info_entry(void **ptr)
{
    sarah_cpu_info_t **info = (sarah_cpu_info_t **) ptr;
    if ( *info != NULL ) {
        sarah_free(*info);
        *ptr = NULL;
    }
}

SARAH_API int sarah_get_cpu_info(cel_link_t *list)
{
    FILE *fd;
    sarah_cpu_info_t *info = NULL;
    char buff[1024] = {'\0'};
    char key[32] = {'\0'}, value[1024] = {'\0'};

    fd = fopen("/proc/cpuinfo", "r");
    if ( fd == NULL ) {
        return 1;
    }

    while ( 1 ) {
        if ( fgets(buff, sizeof(buff), fd) == NULL ) {
            break;
        }

        memset(key, 0x00, sizeof(key));
        memset(value, 0x00, sizeof(value));
        sscanf(
            buff,
            "%31[^:]:%*[ ]%1023[^\n]",
            key, value
        );

        cel_right_trim(key);
        cel_right_trim(value);
        // printf("key=%s, value=%s\n", key, value);

        /*
         * detect the border of the processor
         * then create a new entry and append it to the link
        */
        if ( strcasecmp(key, "processor") == 0 ) {
            info = new_cpu_info_entry();
            cel_link_add_last(list, info);
        } else if ( info == NULL ) {
            // ignore the error
        } else if ( strcasecmp(key, "vendor_id") == 0 ) {
            strcpy(info->vendor_id, value);
        } else if ( strcasecmp(key, "model") == 0 ) {
            info->model = atoi(value);
        } else if ( strcasecmp(key, "model name") == 0 ) {
            strncpy(info->model_name, value, sizeof(info->model_name) - 1);
        } else if ( strcasecmp(key, "cpu MHz") == 0 ) {
            info->mhz = atof(value);
        } else if ( strcasecmp(key, "cache size") == 0 ) {
            info->cache_size = atoi(value);
        } else if ( strcasecmp(key, "physical id") == 0 ) {
            info->physical_id = atoi(value);
        } else if ( strcasecmp(key, "core id") == 0 ) {
            info->core_id = atoi(value);
        } else if ( strcasecmp(key, "cpu cores") == 0 ) {
            info->cores = atoi(value);
        } else if ( strcasecmp(key, "power management") == 0 ) {
            // clear the current info
            info = NULL;
        }
    }

    fclose(fd);
    return 0;
}

SARAH_API void sarah_print_cpu_info(const sarah_cpu_info_t *info)
{
    printf(
        "vendor_id=%s, model=%u, model_name=%s, MHz=%f, ",
        info->vendor_id, info->model, info->model_name, info->mhz
    );
    
    printf(
        "cache_size=%u, physical_id=%u, core_id=%u, cores=%u\n",
        info->cache_size, info->physical_id, info->core_id, info->cores
    );
}


SARAH_API int sarah_get_cpu_loadavg(sarah_cpu_loadavg_t *loadavg)
{
    FILE *fd;
    char buff[256] = {'\0'};

    fd = fopen("/proc/loadavg", "r");
    if ( fd == NULL ) {
        return 1;
    }

    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    sscanf(
        buff, 
        "%f %f %f", 
        &loadavg->t_1m, &loadavg->t_5m, &loadavg->t_15m
    );

    fclose(fd);
    return 0;
}

SARAH_API void sarah_print_cpu_loadavg(sarah_cpu_loadavg_t *loadavg)
{
    printf(
        "t_1m=%f, t_5m=%f, t_15m=%f\n",
        loadavg->t_1m, loadavg->t_5m, loadavg->t_15m
    );
}
