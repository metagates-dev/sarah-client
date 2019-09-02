/**
 * mem info test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct mem_pack_entry
{
    unsigned long int total;        // total size in KB
    unsigned long int free;         // free size in KB
    unsigned long int available;    // available size in KB
} mem_pack_t;

// internal function to get the memory stat information
int get_mem_stat(mem_pack_t *m)
{
    FILE *fd;
    char buff[256] = {'\0'};

    fd = fopen("/proc/meminfo", "r");
    if ( fd == NULL ) {
        return 1;
    }

    // get the total value
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        return 2;
    }

    sscanf(buff, "%*s %ld", &m->total);

    // get the free value
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        return 2;
    }

    sscanf(buff, "%*s %ld", &m->free);

    // get the available value
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        return 2;
    }

    sscanf(buff, "%*s %ld", &m->available);

    fclose(fd);
    return 0;
}

// internal method to print the memory stat information
void print_mem_stat(mem_pack_t *m)
{
    printf("Total=%ld, Free=%ld, Available=%ld\n", m->total, m->free, m->available);
}

int main(int argc, char **argv)
{
    mem_pack_t m;
    get_mem_stat(&m);
    print_mem_stat(&m);
    return 0;
}
