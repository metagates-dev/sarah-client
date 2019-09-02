/**
 * sarah smark disk application with the following functions:
 *
 * 1, auto mount all the disks.
 * 2, check and do the partition, format for the newly added disk.
 * 3, do the disk dynamic post for action like add,remove,damage.
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <fcntl.h>
#include <curl/curl.h>

#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

SARAH_STATIC int _smartdisk_scan_filter(sarah_disk_t *, int);
SARAH_STATIC int _get_disk_partitions_by_offset(
    sarah_disk_t *, sarah_disk_partition_t ***);

/* smart disk entry define */
struct sarah_smartdisk_entry
{
	unsigned int interval;          /* disk scan interval in seconds */
    int auto_partition;             /* Start the auto partition ? */
    char *mount_basedir;            /* Disk mount base directory */
    char *mount_prefix;             /* mount path name prefix */
    char *mount_algorithm;          /* mount name algorithm */
};
typedef struct sarah_smartdisk_entry sarah_smartdisk_t;

SARAH_STATIC int sarah_smartdisk_init(sarah_node_t *, sarah_smartdisk_t *);
SARAH_STATIC void sarah_smartdisk_destroy(sarah_smartdisk_t *);
SARAH_STATIC int sarah_disk_mount(sarah_smartdisk_t *, sarah_disk_t *);


/**
 * initialize the smartdisk object
 *
 * @param   node
 * @param   smartdisk
 * @return  int 0 for succeed or failed
*/
SARAH_STATIC int sarah_smartdisk_init(sarah_node_t *node, sarah_smartdisk_t *smartdisk)
{
	memset(smartdisk, 0x00, sizeof(sarah_smartdisk_t));
	smartdisk->interval = 30;

    smartdisk->auto_partition = node->config.disk_auto_partition;
    smartdisk->mount_basedir = node->config.disk_mount_basedir == NULL
        ? "/data/" : node->config.disk_mount_basedir;
    smartdisk->mount_prefix = node->config.disk_mount_prefix == NULL
        ? "sarah_" : node->config.disk_mount_prefix;
    smartdisk->mount_algorithm = node->config.disk_mount_algorithm == NULL
        ? "wwn" : node->config.disk_mount_algorithm;


	return 0;
}

SARAH_STATIC void sarah_smartdisk_destroy(sarah_smartdisk_t *smartdisk)
{
    // do nothing here right now
}

/**
 * smart disk partitions define.
 *
 * 1, define the partitions according to the size of the disk
 * 2, make the partitions entry define and return it
 *
 * @param   disk
 * @param   sarah_disk_partition_t array (need to free after use it)
 * @return  int number of partitions
*/
SARAH_STATIC int _get_disk_partitions_by_offset(
    sarah_disk_t *disk, sarah_disk_partition_t ***ptr)
{
    int i, part_num = 0;
    unsigned long int cur, offset = 4294967296;  // blocks of 2TBytes
    sarah_disk_partition_t *part;
    sarah_disk_partition_t **partitions;

    for ( cur = 0; cur < disk->p_blocks; cur += offset ) {
        part_num++;
    }

    *ptr = (sarah_disk_partition_t **) sarah_calloc(
        sizeof(sarah_disk_partition_t *), part_num);
    if ( ptr == NULL ) {
        return -1;
    }

    partitions = *ptr;
    for ( i = 0, cur = 0; i < part_num - 1; i++ ) {
        part = new_disk_partition_entry();
        part->format = "ext4";
        part->start  = cur;
        part->blocks = offset;
        cur += offset + 1;  // left include
        partitions[i] = part;
    }

    part = new_disk_partition_entry();
    part->format = "ext4";
    part->start  = cur;
    part->blocks = disk->p_blocks - cur;
    partitions[part_num - 1] = part;

    return part_num;
}

/**
 * smart disk mount implementation.
 *
 * 1, mount the specified disk partition
 * 2, fill the disk partition field
 *
 * @param   smartdisk
 * @param   disk
 * @return  int
*/
SARAH_STATIC int sarah_disk_mount(sarah_smartdisk_t *smartdisk, sarah_disk_t *disk)
{
    int errno = 0, len;
    char _target[256] = {'\0'};

    /* Remove the tailing slash of the path */
    len = strlen(smartdisk->mount_basedir);
    memcpy(_target, smartdisk->mount_basedir, len);
    for ( ; _target[len-1] == '/'; len-- ) {
        _target[len - 1] = '\0';
    }

    /* check and make the mount base directory */
    // printf("mount_basedir=%s\n", smartdisk->mount_basedir);
    if ( access(smartdisk->mount_basedir, F_OK) == -1 ) {
        if ( mkdir(smartdisk->mount_basedir, S_IRWXU | S_IRWXG | S_IRWXO) == -1 ) {
            errno = 1;
            goto failed;
        }
    }

    sprintf(
        _target + len, "/%s%s_part%d",
        smartdisk->mount_prefix,
        disk->wwn,
        disk->part_no
    );

    /* check and make sure the mount target */
    if ( access(_target, F_OK) == -1 ) {
        if ( mkdir(_target, S_IRWXU | S_IRWXG | S_IRWXO) == -1 ) {
            errno = 2;
            goto failed;
        }
    }

    // printf("name=%s, _target=%s, format=%s\n", disk->name, _target, disk->format);
    if ( mount(disk->name, _target, disk->format, MS_SYNCHRONOUS, NULL) == -1 ) {
        errno = 3;
        goto failed;
    }

    // file the mount field for the disk entry
    memcpy(disk->mount, _target, strlen(_target));

    /* set the mount point permissions */
    if ( chmod(_target, S_IRUSR | S_IWUSR | S_IXUSR 
        | S_IRGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH) == -1 ) {
        errno = 4;
        goto failed;
    }

failed:

	return errno;
}


SARAH_STATIC int _smartdisk_scan_filter(sarah_disk_t *disk, int step)
{
    switch ( step ) {
    case -1:
        if ( strncmp(disk->name, "/dev/loop", 9) == 0
            || strncmp(disk->name, "/dev/ram", 8) == 0 ) {
            return 1;
        }
        break;
    case SARAH_DISK_FILL_PT:
        if ( sarah_is_valid_disk_type(disk->format) > 0 ) {
            return 1;
        }
        break;
    }

    return 0;
}


/**
 * smart disk application startup interface
 *
 * @param   booter
 * @param   node
 * @return  int
*/
SARAH_API int sarah_smartdisk_application(sarah_booter_t *booter, sarah_node_t *node)
{
	int errno, part_num;

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    pid_t boot_ppid, cur_ppid;
#endif

	sarah_smartdisk_t smartdisk;
	cel_link_t disk_list;
    cel_link_node_t *l_node;
    sarah_disk_t *disk;

    cel_ihashmap_t disk_map;
    sarah_disk_partition_t **partitions = NULL;

	// -- Basic initialize
	if ( (errno = sarah_smartdisk_init(node, &smartdisk)) != 0 ) {
		printf("[Error]: Fail to initialize the smartdisk with errno=%d\n", errno);
		return 1;
	}

	cel_link_init(&disk_list);
    cel_ihashmap_init(&disk_map, 16, 0.85);

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    /* Get and boot parent id */
    boot_ppid = getppid();
#endif

#ifdef _SARAH_GHOST_UDISK_ENABLE
    /* Default the process status to stop for ghost u-disk version */
    booter->status = SARAH_PROC_STOP;
#endif


	/*
	 * Keep firing scan the disk list do the mount work.
	 * and post the disk dynamic to the remote server.
	*/
	while ( 1 ) {
		if ( booter->status == SARAH_PROC_EXIT ) {
			break;
        } /* else if ( booter->status == SARAH_PROC_STOP ) {
            _SARAH_STOP_WARNNING("sarah-smartdisk");
            sleep(10);
            continue;
        } */

#ifdef _SARAH_ASSERT_PARENT_PROCESS
        /* check the parent process has change then break the loop */
        cur_ppid = getppid();
        if ( cur_ppid != boot_ppid ) {
            _SARAH_PPID_ASSERT_ERR("sarah-smartdisk");
            break;
        }
#endif

        printf("+-Try to get the disk list ... \n");
        if ( (errno = sarah_get_disk_list(
            &disk_list,
            SARAH_DISK_FILL_PT | SARAH_DISK_FILL_ST,
            _smartdisk_scan_filter)) > 0 ) {
            printf("[Error]: Failed with errno = %d\n", errno);
            goto loop;
        }

        printf("[Info]: With %d disk entries\n", disk_list.size);

        /* Do the disk statistics to count the disk partitions */
        cel_ihashmap_clear(&disk_map);
        for ( l_node = disk_list.head->_next;
            l_node != disk_list.tail; l_node = l_node->_next ) {
            disk = (sarah_disk_t *) l_node->value;
            if ( cel_ihashmap_exists(&disk_map, disk->wwn) == 1 ) {
                if ( disk->part_no > -1 ) {
                    cel_ihashmap_inc(&disk_map, disk->wwn, 1);
                }
            } else {
                cel_ihashmap_put(&disk_map, disk->wwn, disk->part_no == -1 ? 0 : 1);
            }
        }

        /* Analysis the disk list and do the mount as need */
        printf("+-Try to do the disk analysis ... \n");
        for ( l_node = disk_list.head->_next;
            l_node != disk_list.tail; l_node = l_node->_next ) {
            disk = (sarah_disk_t *) l_node->value;
            // sarah_print_disk(disk);
            // printf("cel_ihashmap_get(%s)=%d\n", disk->wwn, 
            //  cel_ihashmap_get(&disk_map, disk->wwn));

            /*
             * 1, Disk partition checking
             * Check the partition of the disk and do the partition
             *  as needed together with the partition format
            */
            if ( smartdisk.auto_partition == 1 
                && disk->part_no == -1 && strlen(disk->format) == 0
                && cel_ihashmap_exists(&disk_map, disk->wwn) == 1 
                    && cel_ihashmap_get(&disk_map, disk->wwn) == 0 ) {
                part_num = _get_disk_partitions_by_offset(disk, &partitions);
                if ( part_num > 0 ) {
                    printf("+-Try to create %d partitions for disk device=%s ... ", 
                        part_num, disk->name);
                    // if ( (errno = sarah_disk_partition_shell(
                    //     disk->name, partitions, part_num)) == 0 ) {
                    //     printf(" --Succeed\n");
                    // } else {
                    //     printf(" --Failed with errno=%d\n", errno);
                    // }
                }
            }

            /*
             * 2, Disk mount checking
             * disk entry with partition information and a format define
             * and the not mount yet which is mount is empty.
            */
            if ( disk->part_no > -1 && disk->p_blocks > 0
                && strlen(disk->format) > 1 && strlen(disk->mount) == 0 ) {
                printf("+-Try to mount disk device=%s ... ", disk->name);
                if ( (errno = sarah_disk_mount(&smartdisk, disk)) == 0 ) {
                    printf(" --Succeed\n");
                } else {
                    printf(" --Failed with errno=%d\n", errno);
                }
            }
        }
        printf(" --[Done]\n");

        /* resource clean up */
        printf("+-Try to do the resource clean up ... ");
        sarah_clear_link_entries(&disk_list, free_disk_entry);
        printf(" --[Done]\n");

loop:
		printf("[smartdisk]: Waiting for the next invocation ... \n");
		sleep(smartdisk.interval);
	}

    cel_ihashmap_destroy(&disk_map);
	cel_link_destroy(&disk_list, NULL);
	sarah_smartdisk_destroy(&smartdisk);

	return 0;
}
