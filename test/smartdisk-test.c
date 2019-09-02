
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../src/cel_link.h"
#include "../src/cel_link.c"

/**
 * disk information entry
 */
struct disk_entry {
    int major;
    int minor;
    int64_t blocks;
    char model[40];
    char serial_no[20];
    unsigned wwn[4];
    char name[20];
};
typedef struct disk_entry disk_entry_t;
int _get_all_disks(cel_link_t *list);
int _has_partition_table(char *name);
int _get_all_unparted_disks(cel_link_t *list, char *disks[]);
void _print_disks(const cel_link_t *list);
int _contain_number(char *str);
int _create_partition(char *disk);

/**
 * check if the disk has a partition table
 * @param name
 * @return
 */
int _has_partition_table(char *name)
{
    FILE *fp;
    char buffer[80];
    char cmd[] = {"sudo fdisk -l /dev/"};

    strcat(cmd, name);
    fp = popen(cmd, "r");
    while (!feof(fp)) {
        fgets(buffer, sizeof(buffer), fp);
        if (strstr(buffer, "Disklabel") != NULL) {
            return 1;
        }
    }
    pclose(fp);
    return 0;
}

/**
 * 判断某个磁盘是否有分区
 * 如果一个磁盘设备(/dev/sda)存在分区，分区的名称会是 /dev/sda1, /dev/sda2 ...
 * @param disks
 * @param disk_name
 * @return
 */
int _has_partition(cel_link_t *list, char *disk_name)
{
    cel_link_node_t *node;
    disk_entry_t *entry;
    char new_name[20];

    strcpy(new_name, disk_name);
    strcat(new_name, "1");
    for ( node = list->head->_next; node != list->tail; node = node->_next ) {
        entry = (disk_entry_t *) node->value;
        if (strcmp(new_name, entry->name) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * get all disks that unparted
 * @param list
 * @param disks
 * @return
 */
int _get_all_unparted_disks(cel_link_t *list, char *disks[])
{
    cel_link_node_t *node;
    disk_entry_t *entry;
    int i = 0;

    for ( node = list->head->_next; node != list->tail; node = node->_next ) {
        entry = (disk_entry_t *) node->value;

        // 过滤掉已经分过区的磁盘设备
        if (_contain_number(entry->name)) {
            continue;
        }
        // check if has a partition
        if (_has_partition(list, entry->name) == 0) {
            printf("Finding a new disk device: %s\n", entry->name);
            disks[i] = entry->name;
            i++;
        }
    }
}

/**
 * walk the list and print value
 * @param list
 */
void _print_disks(const cel_link_t *list)
{
    cel_link_node_t *node;
    disk_entry_t *entry;

    printf("%s \t\t%s \t%s \t%s \t\t%s \t\t\t\t\t\t\%s \t\t%s\n", "name", "major", "minor", "blocks", "model", "serial_no", "wwn");
    for ( node = list->head->_next; node != list->tail; node = node->_next ) {
        entry = (disk_entry_t *) node->value;
        printf("%s \t%d \t%d \t%ld \t%.40s \t%.20s \t0x%04x%04x%04x%04x\n",
               entry->name, entry->major, entry->minor, entry->blocks, entry->model, entry->serial_no,
        entry->wwn[0], entry->wwn[1], entry->wwn[2], entry->wwn[3]);
    }
}

int _get_all_disks(cel_link_t *list)
{

    FILE *fp;
    int fd;
    char  buffer[80];
    char name[30] = "/dev/";
    disk_entry_t *disk_entry;
    struct hd_driveid hid;

    fp = fopen("/proc/partitions", "r");
    fgets(buffer,sizeof(buffer),fp);

    while ( 1 ) {
        if ( fgets(buffer, sizeof(buffer), fp) == NULL ) {
            break;
        }

        if ( strlen(buffer) < 5 ) {
            continue;
        }

        disk_entry = (disk_entry_t *) malloc(sizeof(disk_entry_t));
        memset(disk_entry, 0 , sizeof(disk_entry));

        sscanf(
                buffer,
                "%d %d %ld %11s",
                &disk_entry->major, &disk_entry->minor, &disk_entry->blocks, disk_entry->name
        );
        sprintf(name, "/dev/%.20s", disk_entry->name);
        sprintf(disk_entry->name, "%.20s", name);

        if (strstr(disk_entry->name, "sd") == NULL) {
            continue;
        }
        // get disk information by ioctl
        fd = open (disk_entry->name, O_RDONLY);
        if (fd < 0) {
            perror("Fail to open the device file");
            continue;
        }
        if (ioctl(fd, HDIO_GET_IDENTITY, &hid) < 0) {
            continue;
        }
        memcpy(disk_entry->serial_no, hid.serial_no, sizeof(disk_entry->serial_no));
        memcpy(disk_entry->model, hid.model, sizeof(disk_entry->model));
        disk_entry->wwn[0] = hid.words104_125[4];
        disk_entry->wwn[1] = hid.words104_125[5];
        disk_entry->wwn[2] = hid.words104_125[6];
        disk_entry->wwn[3] = hid.words104_125[7];
        // add node to list
        cel_link_add(list, disk_entry);
    }
    fclose(fp);
    return 0;
}

/**
 * 创建磁盘分区
 * @param disk
 * @return
 */
int _create_partition(char *disk)
{
    FILE *fp;
    char buf[80] = {0};
    // 创建分区表
    if ( (fp = popen("parted -s /dev/sdb mklabel gpt", "r")) == NULL ) {
        perror("Fail to open pipeline\n");
        return 0;
    }
    fgets(buf, sizeof(buf), fp);
    if ( strlen(buf) > 0 ) {
        perror("Make disk label fail.");
        return 0;
    }
    pclose(fp);
    printf("parted success!\n");

    // 创建磁盘分区
    if ( (fp = popen("parted -s /dev/sdb mkpart sarahos ext4 2048s 100%", "r")) == NULL ) {
        perror("Fail to open pipeline\n");
        return 0;
    }
    fgets(buf, sizeof(buf), fp);
    if ( strlen(buf) == 0 ) {
        perror("Make disk partition fail.");
        return 0;
    }
    pclose(fp);
    return 0;

}

/**
 * 格式化分区
 * @param partition
 * @return
 */
int _format_disk(char *partition)
{
    FILE *fp;
    char cmd[80] = "mkfs.ext4 ";
    strcat(cmd, partition);
    // 创建分区表
    if ( (fp = popen(cmd, "r")) == NULL ) {
        perror("Fail to open pipeline\n");
        return 0;
    }
    pclose(fp);
    return 0;
}

/**
 * check if a string contain numbers
 * @param str
 * @return
 */
int _contain_number(char *str)
{
    int i;
    if (strlen(str) == 0) {
        return 0;
    }
    for(i = 0; str[i] != 0; i++) {
        if(str[i] > '0' && str[i] <= '9') {
            return 1;
        }
    }
    return 0;
}

void _print_array(char **arr, int len)
{
    for (int i = 0; i < len; ++i) {
        if (arr[i] == NULL) {
            continue;
        }
       printf("%s\n", arr[i]);
    }
}

int main(int argc, char const *argv[])
{
    cel_link_t list;
    char *disks[30];
    // init memory
    memset(disks, 0, sizeof(disks));

    cel_link_init(&list);
    _get_all_disks(&list);
    //_get_all_unparted_disks(&list, disks);
    //_print_array(disks, 30);
    _print_disks(&list);
    // free resource
    cel_link_destroy(&list, free);
    return 0;
}
