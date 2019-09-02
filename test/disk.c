/**
 * disk information fetch test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef struct disk_stat_entry
{
    char name[64];          // file system name
    char type[32];          // file system type

    unsigned long int inode;    // inode number
    unsigned long int iused;    // used inode number
    unsigned long int size;     // total size in KB
    unsigned long int used;     // total size used in KB
} disk_stat_t;

void print_disk_stat(disk_stat_t *d)
{
    printf(
        "name=%s, type=%s, inode=%ld, iused=%ld, size=%ld, used=%ld\n",
        d->name, d->type, d->inode, d->iused, d->size, d->used
    );
}

int is_valid_disk_type(char *type)
{
    register int i, len = 10;
    char *all_types[] = {
        // Windows
        "fat16",
        "fat32",
        "ntfs",
        "exfat",

        "hpfs",

        // Linux
        "ext2",
        "ext3",
        "ext4",

        "jfs",
        "xfs"
    };

    for ( i = 0; i < len; i++ ) {
        if ( strcasecmp(type, all_types[i]) == 0 ) {
            return 0;
        }
    }

    return 1;
}

// internal function to get the disk stat information
int get_disk_stat(disk_stat_t *d)
{
    char buff[256] = {'\0'};
    char *cmd = "df -k --output=source,fstype,itotal,iused,size,used";
    FILE *fd;

    fd = popen(cmd, "r");
    if ( fd == NULL ) {
        return 1;
    }

    // clear the first line
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        return 2;
    }

    // get all the lines
    while ( 1 ) {
        if ( fgets(buff, sizeof(buff), fd) == NULL ) {
            break;
        }

        sscanf(
            buff, 
            "%s %s %ld %ld %ld %ld", 
            d->name, d->type, &d->inode, &d->iused, &d->size, &d->used
        );

        // check the type
        if ( is_valid_disk_type(d->type) == 0 ) {
            print_disk_stat(d);
        }
    }

    fclose(fd);
    return 0;
}


int main(int argc, char **argv)
{
    disk_stat_t disk;
    get_disk_stat(&disk);
    return 0;
}
