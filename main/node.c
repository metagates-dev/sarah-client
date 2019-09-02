/**
 * sarah node interface implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>
#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"


#define sarah_node_size 52
SARAH_STATIC int _read_unsigned_int(char *, int);
SARAH_STATIC void _write_unsigned_int(char *, int, unsigned int);
SARAH_STATIC int _write_buffer(sarah_node_t *, int, char *, unsigned int);
SARAH_STATIC void _gen_ts_random_node_id(char *);
SARAH_STATIC void _gen_ts_rmmac_node_id(char *, unsigned char hd_addr[6]);


/**
 * local file binary format
 *
 * +------------+-----------+--------------+----------+
 * | created_at | interval  | last_boot_at | node_id  |
 * +------------+-----------+--------------+----------+
 *   4 bytes      4 bytes        4 bytes    40 bytes
 *
 * All in local byte order - little-endian byte order
 *
 * unique id
 * %8s-%8s-%4s-%4s-%12s
*/

SARAH_STATIC int _read_unsigned_int(char *buffer, int offset)
{
    return (
        ((buffer[offset  ]) & 0x000000FF) |
        ((buffer[offset+1] <<  8) & 0x0000FF00) |
        ((buffer[offset+2] << 16) & 0x00FF0000) |
        ((buffer[offset+3] << 24) & 0xFF000000)
    );
}

SARAH_STATIC void _write_unsigned_int(char *buffer, int offset, unsigned int val)
{
    buffer[offset  ] = (val & 0xFF);
    buffer[offset+1] = ((val >>  8) & 0xFF);
    buffer[offset+2] = ((val >> 16) & 0xFF);
    buffer[offset+3] = ((val >> 24) & 0xFF);
}

SARAH_STATIC void _gen_ts_random_node_id(char *buff)
{
    struct timeval tv;

    // get the local unix timestamp
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec + tv.tv_usec);
    sprintf(
        buff, 
        "%8x-%08x-%04x-%04x-%04x%08x", 
        (unsigned int) tv.tv_sec,
        (unsigned int) tv.tv_usec * 1000000,
        rand() % 0xFFFF,
        ((rand() % 0xFF) << 8) | 0x00,
        rand() % 0xFFFF,
        rand() % 0xFFFFFFFF
    );
}

SARAH_STATIC void _gen_ts_rmmac_node_id(char *buff, unsigned char mac[6])
{
    struct timeval tv;

    // get the local unix timestamp
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec + tv.tv_usec);
    sprintf(
        buff, 
        "%8x-%08x-%04x-%04x-%02x%02x%02x%02x%02x%02x", 
        (unsigned int) tv.tv_sec,
        (unsigned int) tv.tv_usec * 1000000,
        rand() % 0xFFFF,
        ((rand() % 0xFF) << 8) | 0x01,
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}


SARAH_API void sarah_gen_node_id(char *buff)
{
    cel_link_t list;
    sarah_net_stat_t *net = NULL;
    unsigned char hd_addr[6] = {'\0'};
    int hd_addr_status = 0;

    // initialize the link list and get the network statistics
    cel_link_init(&list);
    sarah_get_net_stat(&list);
    while ( (net = cel_link_remove_first(&list)) != NULL ) {
        if ( strcmp(net->device, "lo") == 0 ) {
            // Ignore the virtual local device
        } else if ( hd_addr_status == 1 ) {
            // we've already get the first hd_addr
        } else if ( sarah_get_hardware_addr(hd_addr, net->device) == 0 ) {
            hd_addr_status = 1;
        }

        free_net_stat_entry((void **)&net);
    }

    // @Note about memory clean up
    // 1. Destroy the runtime cel link list
    // 2. remove all the automatically generated network entry
    cel_link_destroy(&list, NULL);

    // check the hardware address fetch status
    // and do the auto mode switch
    if ( hd_addr_status == 1 ) {
        _gen_ts_rmmac_node_id(buff, hd_addr);
    } else {
        _gen_ts_random_node_id(buff);
    }
}


/* 
 * initialize the node from a specified node local file
 * the specifield file will be created if it not exists yet..
 *
 * @param   node
 * @param   base_dir
*/

#define sarah_node_db_file "node.db"
#define sarah_node_conf_file "config.json"
SARAH_API int sarah_node_init(sarah_node_t *node, char *base_dir)
{
    int fno, rno;
    char buff[sarah_node_size];
    unsigned int size = sarah_node_size;
    struct timeval tv;

    cel_strbuff_t strbuff;
    FILE *fd;
    char file_buff[1024], *line;
    cJSON *json_result, *cursor;

    rno = 0;
    memset(node, 0x00, sizeof(sarah_node_t));
    node->is_new = 0;
    node->interval = sarah_node_interval;
    node->base_dir = NULL;
    node->db_file = NULL;
    node->config_file = NULL;

    node->config.build = NULL;
    node->config.app_key = NULL;
    node->config.disk_mount_basedir = NULL;
    node->config.disk_mount_prefix = NULL;
    node->config.disk_mount_algorithm = NULL;

    if ( base_dir == NULL ) {
        return 1;
    }

    
    node->base_dir = strdup(base_dir);

    // try to open the local file 
    // and load the data to initialize the node
    // node->db_file = base_dir + node.db
    node->db_file = (char *) sarah_malloc(strlen(base_dir) + strlen(sarah_node_db_file) + 1);
    if ( node->db_file == NULL ) {
        rno = 2;
        goto failed;
    }

    sprintf(node->db_file, "%s%s", base_dir, sarah_node_db_file);
    gettimeofday(&tv, NULL);

    // file already exists
    if ( access(node->db_file, F_OK) == 0 ) {
        fno = open(node->db_file, O_RDONLY);
        if ( fno > -1 ) {
            rno = 2; 
            if ( flock(fno, LOCK_EX) == 0 && read(fno, buff, size) == size ) {
                rno = 0;
                node->created_at = _read_unsigned_int(buff, 0);
                node->interval = _read_unsigned_int(buff, 4);
                node->last_boot_at = _read_unsigned_int(buff, 8);

                // the actual space for the node_id is greater than the
                // it really need, so left a whitespace for it.
                memcpy(node->node_id, buff + 12, sarah_node_id_size - 1);
            }

            flock(fno, LOCK_UN);
            close(fno);
        }
    } else {
        node->is_new = 1;
        // strcpy(node->node_id, "12345678-01293873-eeue-hhgj-3827huy83912");
        // _gen_ts_random_node_id(node->node_id, &tv);
        sarah_gen_node_id(node->node_id);
        node->created_at = tv.tv_sec;
        node->interval = sarah_node_interval;
        if ( sarah_node_flush(node) != 0 ) {
            rno = 4;
            goto failed;
        }
    }

    // set or update the last boot at timestamp
    // _gen_node_id(node->node_id, &tv);
    sarah_node_set_last_boot_at(node, tv.tv_sec, node->is_new ? 0 : 1);



    /* Check and read the config.json file */
    node->config_file = (char *) sarah_malloc(strlen(base_dir) + strlen(sarah_node_conf_file) + 1);
    if ( node->config_file == NULL ) {
        rno = 5;
        goto failed;
    }

    sprintf(node->config_file, "%s%s", base_dir, sarah_node_conf_file);
    if ( (fd = fopen(node->config_file, "r")) == NULL ) {
        rno = 6;
        goto failed;
    }

    cel_strbuff_init(&strbuff, 128, NULL);
    while ( (line = sarah_get_line(file_buff, fd)) != NULL ) {
        if ( line[0] == '#' ) {
            continue;
        }

        cel_strbuff_append(&strbuff, line, 1);
    }

    fclose(fd);
    if ( strbuff.size < 2 ) {
        rno = 7;
        goto failed;
    }

    // Parse the JSON configuration file
    json_result = cJSON_Parse(strbuff.buffer);
    if ( json_result == NULL ) {
        rno = 8;
        goto failed;
    }

    cursor = cJSON_GetObjectItem(json_result, "build");
    if ( cursor != NULL && cursor->type == cJSON_String ) {
        node->config.build = strdup(cursor->valuestring);
    }

    cursor = cJSON_GetObjectItem(json_result, "app_key");
    if ( cursor != NULL && cursor->type == cJSON_String ) {
        node->config.app_key = strdup(cursor->valuestring);
    }

    cursor = cJSON_GetObjectItem(json_result, "disk_auto_partition");
    if ( cursor != NULL ) {
        if ( cursor->type == cJSON_String ) {
            cel_strtolower(
                cursor->valuestring, 
                strlen(cursor->valuestring)
            );
            node->config.disk_auto_partition = 
                strcmp(cursor->valuestring, "true") == 0 ? 1 : 0;
        } else if ( cursor->type == cJSON_Number ) {
            node->config.disk_auto_partition = cursor->valueint > 0 ? 1 : 0;
        }
    }

    cursor = cJSON_GetObjectItem(json_result, "disk_mount_basedir");
    if ( cursor != NULL && cursor->type == cJSON_String ) {
        node->config.disk_mount_basedir = strdup(cursor->valuestring);
    }

    cursor = cJSON_GetObjectItem(json_result, "disk_mount_prefix");
    if ( cursor != NULL && cursor->type == cJSON_String ) {
        node->config.disk_mount_prefix = strdup(cursor->valuestring);
    }

    cursor = cJSON_GetObjectItem(json_result, "disk_mount_algorithm");
    if ( cursor != NULL && cursor->type == cJSON_String ) {
        node->config.disk_mount_algorithm = strdup(cursor->valuestring);
    }

    cJSON_Delete(json_result);
    cel_strbuff_destroy(&strbuff);



    // @Note added at 2018/12/18
    // memset(&node->kernel_info, 0x00, sizeof(sarah_utsname_t));
    if ( uname(&node->kernel_info) != 0 ) {
        // @TODO: do error log here
    }

failed:
    if ( rno > 0 ) {
        if ( node->db_file != NULL ) {
            sarah_free(node->db_file);
            node->db_file = NULL;
        }

        if ( node->config_file != NULL ) {
            sarah_free(node->config_file);
            node->config_file = NULL;
        }
    }

    if ( strbuff.buffer != NULL ) {
        cel_strbuff_destroy(&strbuff);
    }
    
    return rno;
}


/* 
 * it will not remove the local file by default
 * except set the code to 1 then it will clean up the disk file
 *
 * @param   node
 * @param   code
*/
SARAH_API void sarah_node_destroy(sarah_node_t *node, int code)
{
    if ( node->base_dir != NULL ) {
        free(node->base_dir);
        node->base_dir = NULL;
    }

    if ( node->db_file != NULL ) {
        // check and remove the local storage file
        // if ( code == 1 ) {
        //     remove(node->db_file);
        // }

        sarah_free(node->db_file);
        node->db_file = NULL;
    }

    if ( node->config_file != NULL ) {
        sarah_free(node->config_file);
        node->config_file = NULL;
    }

    if ( node->config.build != NULL ) {
        sarah_free(node->config.build);
        node->config.build = NULL;
    }

    if ( node->config.app_key != NULL ) {
        sarah_free(node->config.app_key);
        node->config.app_key = NULL;
    }

    if ( node->config.disk_mount_basedir != NULL ) {
        sarah_free(node->config.disk_mount_basedir);
        node->config.disk_mount_basedir = NULL;
    }

    if ( node->config.disk_mount_prefix != NULL ) {
        sarah_free(node->config.disk_mount_prefix);
        node->config.disk_mount_prefix = NULL;
    }

    if ( node->config.disk_mount_algorithm != NULL ) {
        sarah_free(node->config.disk_mount_algorithm);
        node->config.disk_mount_algorithm = NULL;
    }
}

// flush the node to a specified local file
SARAH_API int sarah_node_flush(sarah_node_t *node)
{
    int fno, rno;
    char buff[sarah_node_size];
    unsigned int size = sarah_node_size;

    if ( node->db_file == NULL ) {
        return 0;
    }

    fno = open(node->db_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if ( fno == -1 ) {
        return 1;
    }

    // pack the data
    _write_unsigned_int(buff, 0, node->created_at);
    _write_unsigned_int(buff, 4, node->interval);
    _write_unsigned_int(buff, 8, node->last_boot_at);
    memcpy(buff + 12, node->node_id, sarah_node_id_size - 1);

    rno = 2;
    lseek(fno, 0, SEEK_SET);     // always start from the begin
    if ( flock(fno, LOCK_EX) == 0 && write(fno, buff, size) == size ) {
        rno = 0;
    }

    flock(fno, LOCK_UN);
    close(fno);
    return rno;
}

SARAH_STATIC int _write_buffer(
    sarah_node_t *node, int offset, char *buff, unsigned int size)
{
    int fno, rno;

    if ( node->db_file == NULL ) {
        return 0;
    }

    fno = open(node->db_file, O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if ( fno == -1 ) {
        return 1;
    }

    rno = 2;
    lseek(fno, offset, SEEK_SET);
    if ( flock(fno, LOCK_EX) == 0 && write(fno, buff, size) == size ) {
        rno = 0;
    }

    flock(fno, LOCK_UN);
    close(fno);
    return rno;
}

SARAH_API int sarah_node_set_created_at(
    sarah_node_t *node, unsigned int created_at, int syn)
{
    char buff[4];
    node->created_at = created_at;
    _write_unsigned_int(buff, 0, created_at);
    return syn == 1 ? _write_buffer(node, 0, buff, 4) : 0;
}

SARAH_API int sarah_node_set_interval(
    sarah_node_t *node, unsigned int interval, int syn)
{
    char buff[4];
    node->interval = interval;
    _write_unsigned_int(buff, 0, interval);
    return syn == 1 ? _write_buffer(node, 4, buff, 4) : 0;
}


SARAH_API int sarah_node_set_last_boot_at(
    sarah_node_t *node, unsigned int last_boot_at, int syn)
{
    char buff[4];
    node->last_boot_at = last_boot_at;
    _write_unsigned_int(buff, 0, last_boot_at);
    return syn == 1 ? _write_buffer(node, 8, buff, 4) : 0;
}

SARAH_API void sarah_print_node(sarah_node_t *node)
{
    printf(
        "id=%s, store=%s, new=%d, created_at=%u, last_boot_at=%u, interval=%u",
        node->node_id, node->db_file, node->is_new,
        node->created_at, node->last_boot_at, node->interval
    );

    printf(
        ", last_stat_begin=%u, lat_stat_end=%u, config.build=%s, config.app_key=%s\n", 
        node->last_stat_begin, node->last_stat_end,
        node->config.build, node->config.app_key
    );
}
