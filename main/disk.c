/**
 * sarah disk statistics interface implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/statvfs.h>

#include <linux/hdreg.h>
#include <linux/fs.h>

SARAH_STATIC int _get_tailing_integer(const char *, int *);

SARAH_API int sarah_is_valid_disk_type(char *type)
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
        "vfat",     // EFI boot device

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

SARAH_API sarah_disk_stat_t *new_disk_stat_entry()
{
    sarah_disk_stat_t *disk = (sarah_disk_stat_t *)
        sarah_malloc(sizeof(sarah_disk_stat_t));
    if ( disk == NULL ) {
        SARAH_ALLOCATE_ERROR("new_disk_stat_entry", sizeof(sarah_disk_stat_t));
    }

    memset(disk, 0x00, sizeof(sarah_disk_stat_t));
    return disk;
}

SARAH_API sarah_disk_stat_t *clone_disk_stat_entry(sarah_disk_stat_t *disk)
{
    sarah_disk_stat_t *nd = new_disk_stat_entry();
    if ( nd == NULL ) {
        return NULL;
    }

    // copy the data
    memcpy(nd, disk, sizeof(sarah_disk_stat_t));
    return nd;
}

SARAH_API void free_disk_stat_entry(void **ptr)
{
    sarah_disk_stat_t **disk = (sarah_disk_stat_t **) ptr;
    if ( *disk != NULL ) {
        sarah_free(*disk);
        *ptr = NULL;
    }
}

// get the disk statistics information
SARAH_API int sarah_get_disk_stat(cel_link_t *list)
{
    char buff[256] = {'\0'};
    char *cmd = "df -k --output=source,fstype,itotal,iused,size,used";
    FILE *fd;
    sarah_disk_stat_t disk;
    sarah_disk_stat_t *d = &disk;

    fd = popen(cmd, "r");
    if ( fd == NULL ) {
        return 1;
    }

    // clear the first line
    if ( fgets(buff, sizeof(buff), fd) == NULL ) {
        fclose(fd);
        return 2;
    }

    // get all the lines
    while ( 1 ) {
        if ( fgets(buff, sizeof(buff), fd) == NULL ) {
            break;
        }

        // @see sarah.h to check the width define
        sscanf(
            buff,
            "%47s %15s %ld %ld %ld %ld",
            d->name, d->type, &d->inode, &d->iused, &d->size, &d->used
        );

        // check the type and append the disk statistics entry
        if ( sarah_is_valid_disk_type(d->type) == 0 ) {
            cel_link_add_last(list, clone_disk_stat_entry(d));
        }
    }

    fclose(fd);
    return 0;
}

SARAH_API void sarah_print_disk_stat(const sarah_disk_stat_t *d)
{
    printf(
        "name=%s, type=%s, inode=%ld, iused=%ld, size=%ld, used=%ld\n",
        d->name, d->type, d->inode, d->iused, d->size, d->used
    );
}




/* new disk interface implementation */
SARAH_API int sarah_disk_init(sarah_disk_t *disk)
{
    memset(disk, 0x00, sizeof(sarah_disk_t));
    return 0;
}

SARAH_API void sarah_disk_destroy(sarah_disk_t *disk)
{
    // for now, do nothing here
}

SARAH_API sarah_disk_t *new_disk_entry()
{
    sarah_disk_t *disk = (sarah_disk_t *) sarah_malloc(sizeof(sarah_disk_t));
    if ( disk == NULL ) {
        SARAH_ALLOCATE_ERROR("new_disk_entry", sizeof(sarah_disk_t));
    }

    sarah_disk_init(disk);
    return disk;
}

SARAH_API sarah_disk_t *clone_disk_entry(sarah_disk_t *disk)
{
    sarah_disk_t *_dst = new_disk_entry();
    if ( _dst == NULL ) {
        return NULL;
    }

    memcpy(_dst, disk, sizeof(sarah_disk_t));
    return _dst;
}

SARAH_API void free_disk_entry(void **ptr)
{
    if ( *ptr != NULL ) {
        sarah_free(*ptr);
        *ptr = NULL;
    }
}


/**
 * get the tailing integer of the specified string
 *
 * @param   _src
 * @param   integer (11 bytes for most)
 * @return  0 for succeed and none-0 for failed
*/
SARAH_STATIC int _get_tailing_integer(const char *_src, int *integer)
{
    register int i, _last = strlen(_src) - 1;
    char buffer[12] = {'\0'};

    for ( i = _last; i >= 0; i-- ) {
        if ( _src[i] < '0' || _src[i] > '9' ) {
            break;
        }
    }

    if ( i == _last ) {
        return 1;
    }

    memcpy(buffer, _src + i + 1, _last - i);
    *integer = atoi(buffer);

    return 0;
}


SARAH_API int sarah_get_disk_list(
    cel_link_t *list, int mask, sarah_disk_filter_fn_t filter)
{
    int errno, dir_len, read_line;
    int active_backup_strategy, detect_active;
    DIR *dirp = NULL;
    struct dirent *direntp = NULL;
    sarah_disk_t _dtp, *disk;
    // cel_hashmap_t *disk_map = NULL;

    cel_link_node_t *l_node;
    int disk_fd, part_no;
    struct hd_driveid drive = {0};
    struct hd_geometry geometry = {0};
    long long int block_size = 0;

    FILE *fdisk_fd = NULL, *mtab_fd = NULL;
    char buffer[256] = {'\0'};
    char device[48] = {'\0'}, format[16] = {'\0'}, mount[64] = {'\0'};
    struct statvfs dir_stat;

    if ( (dirp = opendir("/dev/")) == NULL ) {
        errno = 1;
        goto failed;
    }

    /* the length of the base directory length, like '/dev/' */
    errno = 0;
    dir_len = 5;
    active_backup_strategy = 1;
    sarah_disk_init(&_dtp);
    // disk_map = new_cel_hashmap_opacity(16, 0.85);
    while ( (direntp = readdir(dirp)) != NULL ) {
        if ( strcmp(direntp->d_name, ".") == 0
            || strcmp(direntp->d_name, "..") == 0 ) {
            continue;
        }

        if ( direntp->d_type != DT_BLK ) {
            continue;
        }

        // printf("/dev/%s, type=%d\n", direntp->d_name, (int) direntp->d_type);
        memset(&_dtp, 0x00, sizeof(_dtp));
        memcpy(_dtp.name, "/dev/", dir_len);
        memcpy(_dtp.name + dir_len, direntp->d_name, sizeof(_dtp.name) - dir_len - 1);

        /*
         * parse the partition number
         * and it should be a number greater than 0 if defined
        */
        if ( _get_tailing_integer(direntp->d_name, &part_no) == 0 ) {
            _dtp.part_no = part_no;
        } else {
            _dtp.part_no = -1;
        }

        /* ignore the record that did not pass the filter */
        if ( filter != NULL && filter(&_dtp, -1) > 0 ) {
            continue;
        }

        /*
         * check and fetch the hardware information of all the disk .
         *
         * @Note: Since there is a few seconds delay for the /dev/blk file to
         * be removed if the disk hardware block was being pull out or crashed.
         * So here we do the ioctl invoke to get the WWN, serial_no information
         * if failed we will consider this disk block invalid. -- Marked By Lion
        */
        if ( (disk_fd = open(_dtp.name, O_RDONLY)) == -1 ) {
            // fprintf(stderr, "[Error]: Failed to open %s\n", _dtp.name);
            continue;
        }

        // memset(&drive, 0x00, sizeof(drive));
        if ( ioctl(disk_fd, HDIO_GET_IDENTITY, &drive) == -1 ) {
            memcpy(_dtp.serial_no, "0x00000000", 10);
            memcpy(_dtp.model, "Unknow model", 12);
            memcpy(_dtp.wwn, "0x0000000000000000", 18);
        } else {
            active_backup_strategy = 0;
            memcpy(_dtp.serial_no, drive.serial_no, sizeof(drive.serial_no));
            memcpy(_dtp.model, drive.model, sizeof(drive.model));
            sprintf(        // encode the WWN
                _dtp.wwn, "0x%04x%04x%04x%04x",
                drive.words104_125[4], drive.words104_125[5],
                drive.words104_125[6], drive.words104_125[7]
            );
        }

        if ( ioctl(disk_fd, HDIO_GETGEO, &geometry) == 0 ) {
            _dtp.p_start = geometry.start;
        }

        if ( ioctl(disk_fd, BLKGETSIZE, &block_size) == 0 ) {
            _dtp.p_blocks = block_size;
            _dtp.p_size   = block_size * 512;
        }

        close(disk_fd);


        disk = clone_disk_entry(&_dtp);
        cel_right_trim(disk->serial_no);
        cel_right_trim(disk->model);
        cel_link_add_last(list, disk);
        // cel_hashmap_put(disk_map, disk);
    }

    /* check and do the backup hardware information fill */
    if ( active_backup_strategy == 1 ) {
        if ( (fdisk_fd = popen("fdisk -l", "r")) != NULL ) {
            while ( fgets(buffer, sizeof(buffer), fdisk_fd) != NULL ) {
                /* Clear the disk entry */
                if ( strncmp(buffer, "Disk", 4) != 0 ) {
                    continue;
                }

                memset(&_dtp, 0x00, sizeof(sarah_disk_t));
                sscanf(
                    buffer,
                    "Disk %63[^:]:%*[^,], %lld%*[^,], %lld",
                    _dtp.name, &_dtp.p_size, &_dtp.p_blocks
                );

                /* Try to detect the disk identifier */
                detect_active = 0;
                while ( fgets(buffer, sizeof(buffer), fdisk_fd) != NULL ) {
                    if ( strncmp(buffer, "Disk id", 7) == 0 ) {
                        detect_active = 1;
                        break;
                    }

                    if ( strlen(buffer) < 2 ) {     // reach a empty line delimiter
                        break;
                    }
                }

                if ( detect_active == 1 ) {
                    sscanf(buffer, "%*[^:]: %s", _dtp.wwn);
                    /* override the hd info in the global disk list */
                    for ( l_node = list->head->_next;
                        l_node != list->tail; l_node = l_node->_next ) {
                        disk = (sarah_disk_t *) l_node->value;
                        if ( strcmp(disk->name, _dtp.name) == 0 ) {
                            memcpy(disk->wwn, _dtp.wwn, strlen(_dtp.wwn) + 1);
                            disk->p_size   = _dtp.p_size;
                            disk->p_blocks = _dtp.p_blocks;
                        }
                    }
                }

                /* clear all the empty line */
                read_line = 1;
                while ( fgets(buffer, sizeof(buffer), fdisk_fd) != NULL ) {
                    if ( strlen(buffer) > 1 ) {
                        read_line = 0;
                        break;
                    }
                }

                /* Try to detect the partitions of this disk */
                detect_active = 0;
                while ( read_line == 0
                    || fgets(buffer, sizeof(buffer), fdisk_fd) != NULL ) {
                    read_line = 1;
                    if ( strstr(buffer, "Device") != NULL ) {
                        detect_active = 1;
                        break;
                    }

                    if ( strlen(buffer) < 2 ) {
                        break;
                    }
                }

                if ( detect_active == 1 ) {
                    /* scan all the partitions */
                    while ( fgets(buffer, sizeof(buffer), fdisk_fd) != NULL ) {
                        if ( strlen(buffer) < 2 ) {
                            break;
                        }

                        sscanf(
                            buffer, "%63s%*[^0-9]%ld %lld",
                            _dtp.name, &_dtp.p_start, &block_size
                        );
                        _dtp.p_blocks = block_size - _dtp.p_start + 1;
                        _dtp.p_size = _dtp.p_blocks * 512;

                        /* override the hd info in the global disk list */
                        for ( l_node = list->head->_next;
                            l_node != list->tail; l_node = l_node->_next ) {
                            disk = (sarah_disk_t *) l_node->value;
                            if ( strcmp(disk->name, _dtp.name) == 0 ) {
                                memcpy(disk->wwn, _dtp.wwn, strlen(_dtp.wwn) + 1);
                                disk->p_start  = _dtp.p_start;
                                disk->p_blocks = _dtp.p_blocks;
                                disk->p_size   = _dtp.p_size;
                            }
                        }
                    }
                }

            }   /* End while */
        }
    }

    /* check and fetch the disk partition information */
    /* lsblk -a -f -i -b -l -o NAME,FSTYPE,MOUNTPOINT,TYPE,UUID */
    if ( (mask & SARAH_DISK_FILL_PT) != 0 ) {
        // if ( (mtab_fd = fopen("/etc/mtab", "r")) != NULL ) {
        if ( (mtab_fd = popen("lsblk -f -l -o NAME,FSTYPE,MOUNTPOINT", "r")) != NULL ) {
            if ( fgets(buffer, sizeof(buffer), mtab_fd) == NULL ) {
                // clear the header line
            }

            while ( fgets(buffer, sizeof(buffer), mtab_fd) != NULL ) {
                /* Fill the device,format and mount path*/
                memset(device, 0x00, sizeof(device));
                memset(format, 0x00, sizeof(format));
                memset(mount, 0x00, sizeof(mount));
                sscanf(buffer, "%63s %15s %155s", device, format, mount);
                if ( strlen(format) < 2 ) {
                    continue;
                }

                // printf("line=%s", buffer);
                // printf("device=%s, format=%s, mount=%s\n", device, format, mount);
                memset(_dtp.format, 0x00, sizeof(_dtp.format));
                memcpy(_dtp.format, format, strlen(format));
                if ( filter == NULL || filter(&_dtp, SARAH_DISK_FILL_PT) == 0 ) {
                    for ( l_node = list->head->_next;
                        l_node != list->tail; l_node = l_node->_next ) {
                        disk = (sarah_disk_t *) l_node->value;
                        if ( strncmp(disk->name, "/dev/", 5) == 0
                            && strcmp(disk->name + 5, device) == 0 ) {
                            memcpy(disk->format, format, strlen(format));
                            if ( strlen(mount) > 0 ) {
                                memcpy(disk->mount, mount, strlen(mount));
                            }

                            /*
                             * @Note well, some partition tools will directly
                             * override the device name during partition, so here
                             * we mark its part_no to 0 to distinguish the standard
                             * partition device.
                            */
                            if ( disk->part_no == -1 && strlen(format) > 1 ) {
                                disk->part_no = 0;
                            }

                            break;
                        }
                    }
                }
            }
        }
    }

    /* check and fetch the partition storage information */
    if ( (mask & SARAH_DISK_FILL_ST) != 0 ) {
        for ( l_node = list->head->_next;
            l_node != list->tail; l_node = l_node->_next ) {
            disk = (sarah_disk_t *) l_node->value;
            if ( strlen(disk->mount) < 1
                || statvfs(disk->mount, &dir_stat) < 0 ) {
                continue;
            }

            disk->inode = dir_stat.f_files;
            disk->ifree = dir_stat.f_ffree;
            disk->size  = dir_stat.f_blocks * dir_stat.f_bsize;
            disk->free  = dir_stat.f_bfree * dir_stat.f_bsize;
        }
    }

failed:
    sarah_disk_destroy(&_dtp);
    // if ( disk_map != NULL ) {
    //     free_cel_hashmap(disk_map, NULL);
    // }

    if ( dirp != NULL ) {
        closedir(dirp);
    }

    if ( fdisk_fd != NULL ) {
        pclose(fdisk_fd);
        fdisk_fd = NULL;
    }

    if ( mtab_fd != NULL ) {
        pclose(mtab_fd);
        mtab_fd = NULL;
    }

    return errno;
}

SARAH_API void sarah_print_disk(const sarah_disk_t *d)
{
    printf(
        "hd_info: wwn=%s, serial_no=%s, model=%s\n", d->wwn, d->serial_no, d->model
    );

    printf(
        "pt_info: name=%s, part_no=%d, format=%s, mount=%s, \
start=%ld, blocks=%lld, size=%lld\n",
        d->name, d->part_no, d->format, d->mount,
        d->p_start, d->p_blocks, d->p_size
    );

    printf(
        "st_info: inode=%llu, ifree=%llu, size=%llu, free=%llu\n",
        d->inode, d->ifree, d->size, d->free
    );
}

/**
 * do the partition for the specified disk
 *  with the specified partitions define.
 *	this function implemented by shell script
 * @author Rock
 * @param   disk
 * @param   partitions
 * @param   part_num
 * @return  int 0 for succeed or failed
*/
SARAH_API int sarah_disk_partition_shell(
		char *disk, sarah_disk_partition_t **partitions, int part_num)
{

	FILE *fp = NULL;
	int errno = 0, i, t;
	char mklabel_cmd[100];
	char mkpart_cmd[200];
	char partition_name[80];
	char buffer[100];
    int unit = 2097152;
    int timeout = 10;
    char tmpfile[30] = {"/tmp/smartdisk.txt"};

    if (part_num == 0 || partitions == NULL) {
        errno = 1;
        printf("[Error]: Invalid arguments with errno=%d\n", errno);
		goto failed;
    }

	memset(mklabel_cmd, 0x00, sizeof(mklabel_cmd));
	sprintf(mklabel_cmd, "parted -s %s mklabel gpt > %s 2>&1", disk, tmpfile);

	// create disk partition label
	if ( (fp = popen(mklabel_cmd, "r")) == NULL ) {
		errno = 2;
		printf("[Error]: Fail to make disk partition label with errno=%d\n", errno);
		goto failed;
	}
	pclose(fp);

    memset(buffer, 0x00, sizeof(buffer));
    if ((fp = fopen(tmpfile, "r")) != NULL
	    && fgets(buffer, sizeof(buffer), fp) != NULL
	    && strstr(buffer, "Error:") != NULL) {

            errno = 3;
    		printf("[Error]: Fail to make disk partition label with errno=%d\n", errno);
    		goto failed;

	}
	fclose(fp);
	// remove temp file
	remove(tmpfile);

	// create disk partition
	for (i = 0; i < part_num; i++) {
		memset(mkpart_cmd, 0x00, sizeof(mkpart_cmd));
		if (partitions[i]->start == 0 && partitions[i]->blocks == -1) {
			sprintf(mkpart_cmd, "parted -s %s mkpart sarahos_%d ext4 2048s 100%%", disk, i);
		} else if (partitions[i]->start == 0 && partitions[i]->blocks != -1) {
			sprintf(mkpart_cmd, "parted -s %s mkpart sarahos_%d ext4 1049k %luG",
                disk,
                i,
                partitions[i]->blocks / unit
            );
		} else if (partitions[i]->start != 0 && partitions[i]->blocks == -1) {
			sprintf(mkpart_cmd, "parted -s %s mkpart sarahos_%d ext4 %luG 100%%",
			   disk,
			   i,
			   partitions[i]->start / unit
			);
		} else {
			sprintf(mkpart_cmd, "parted -s %s mkpart sarahos_%d ext4 %luG %luG",
    	        disk,
    	        i,
    	        partitions[i]->start / unit,
    	        (partitions[i]->start + partitions[i]->blocks) / unit
			);
		}

		if ((fp = popen(mkpart_cmd, "r")) == NULL) {
			errno = 4;
			printf("[Error]: Fail to execute bash script errno=%d\n", errno);
			goto failed;
		}
		while ( fgets(buffer, sizeof(buffer), fp) != NULL );
		memset(partition_name, 0x00, sizeof(partition_name));
		sprintf(partition_name, "%s%d", disk, i+1);
		// check if the partition is created successfully
        t = 0;
        while (access(partition_name, F_OK) == -1) {

            if (t >= timeout) {
                errno = 5;
    			printf("[Error]: Fail to make disk partition label with errno=%d\n", errno);
    			goto failed;
            }
            t++;
            sleep(1);
        }

		// 格式化磁盘
		if ((errno = sarah_disk_format_shell(partition_name)) != 0) {
			errno = 6;
			printf("[Error]: Fail to format partition with errno=%d\n", errno);
			goto failed;
		}
		pclose(fp);
	}

failed:
    if (fp != NULL) {
        pclose(fp);
    }
	return errno;
}

/**
 * make partition format by shell
 * @author Rock
 * @param disk
 * @return
 */
SARAH_API int sarah_disk_format_shell(char *partition)
{
	FILE *fp;
	char format_cmd[80];
	char buffer[10];
	char tmpfile[30] = {"/tmp/smartdisk.txt"};
	int success = 1;

	sprintf(format_cmd, "mkfs.ext4 %s > %s", partition, tmpfile);
	fp = popen(format_cmd, "r");
	pclose(fp);

	memset(buffer, 0x00, sizeof(buffer));
	if ((fp = fopen(tmpfile, "r")) != NULL
	    && fread(buffer, sizeof(buffer)-1, 1, fp) == 1
	    && strlen(buffer) > 0) {

		success = 0;

	}
	fclose(fp);
	// remove temp file
	remove(tmpfile);
	return success;
}

SARAH_API sarah_disk_partition_t *new_disk_partition_entry()
{
    sarah_disk_partition_t *partition = (sarah_disk_partition_t *) 
        sarah_malloc(sizeof(sarah_disk_partition_t));
    if ( partition == NULL ) {
        SARAH_ALLOCATE_ERROR("new_disk_partition_entry", sizeof(sarah_disk_partition_t));
    }

    sarah_disk_partition_init(partition);
    return partition;
}

SARAH_API void free_disk_partition_entry(void **ptr)
{
    if ( *ptr != NULL ) {
        sarah_free(*ptr);
        *ptr = NULL;
    }
}

SARAH_API int sarah_disk_partition_init(sarah_disk_partition_t *partition)
{
    memset(partition, 0x00, sizeof(sarah_disk_partition_t));
    return 0;
}

SARAH_API void sarah_disk_partition_destroy(sarah_disk_partition_t *partition)
{
    // do nothing here
}

SARAH_API void sarah_print_disk_partition(sarah_disk_partition_t *partition)
{
    printf(
        "partition: start=%lu, blocks=%lu, format=%s\n",
        partition->start, partition->blocks, partition->format
    );
}
