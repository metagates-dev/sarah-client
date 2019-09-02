/**
 * get cpu information statistics test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef struct cpu_info_entry
{
    char vendor_id[16];         // CPU vendor_id
    unsigned int model;         // CPU model
    char model_name[64];        // CPU model name

    float mhz;                  // CPU MHZ
    unsigned int cache_size;    // CPU cache size in Kbytes
    unsigned int physical_id;   // CPU physical id
    unsigned int core_id;       // CPU core id
    unsigned int cores;         // CPU cores
} cpu_info_t;

void print_cpu_info(cpu_info_t *info)
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

void right_trim(char *str)
{
    register int i;
    register int slen = strlen(str);
    for ( i = slen - 1; i >= 0; i-- ) {
        if ( str[i] != ' ' && str[i] != '\t' && str[i] != '\r' ) {
            str[i+1] = '\0';
            break;
        }
    }
}

int get_cpu_info(cpu_info_t *info)
{
    char buff[1024] = {'\0'};
    char key[24] = {'\0'}, value[1024] = {'\0'};
    FILE *fd;

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
            "%[^:]:%*[ ]%[^\n]",
            key, value
        );

        right_trim(key);
        right_trim(value);
        // printf("key=%s, value=%s\n", key, value);
        
        // attribute fill
        if ( strcasecmp(key, "vendor_id") == 0 ) {
            strcpy(info->vendor_id, value);
        } else if ( strcasecmp(key, "model") == 0 ) {
            info->model = atoi(value);
        } else if ( strcasecmp(key, "model name") == 0 ) {
            strcpy(info->model_name, value);
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
        }

        // detect the border for new section
        if ( strcasecmp(key, "power management") == 0 ) {
            print_cpu_info(info);
        }
    }

    fclose(fd);
    return 0;
}

int main(int argc, char **argv)
{
    cpu_info_t info;
    get_cpu_info(&info);
    return 0;
}
