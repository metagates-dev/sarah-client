/**
 * System hardware information fetch interface implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

SARAH_STATIC int _parse_bios_info(sarah_hd_bios_t *, FILE *, char *, size_t);
SARAH_STATIC int _parse_system_info(sarah_hd_system_t *, FILE *, char *, size_t);
SARAH_STATIC int _pase_board_info(sarah_hd_board_t *, FILE *, char *, size_t);


SARAH_API int sarah_hd_bios_init(sarah_hd_bios_t *bios)
{
    memset(bios, 0x00, sizeof(sarah_hd_bios_t));
    return 0;
}

SARAH_API void sarah_hd_bios_destroy(sarah_hd_bios_t *bios)
{
}

SARAH_API void sarah_print_hd_bios(sarah_hd_bios_t *bios)
{
    printf(
        "vendor=%s, version=%s, release_date=%s, runtime_size=%u, rom_size=%u\n",
        bios->vendor, bios->version, bios->release_date, 
        bios->runtime_size, bios->rom_size
    );
}


SARAH_API int sarah_hd_system_init(sarah_hd_system_t *system)
{
    memset(system, 0x00, sizeof(sarah_hd_system_t));
    return 0;
}

SARAH_API void sarah_hd_system_destroy(sarah_hd_system_t *system)
{
}

SARAH_API void sarah_print_hd_system(sarah_hd_system_t *system)
{
    printf("manufacturer=%s, product_name=%s, version=%s\
, serial_no=%s, uuid=%s\n",
        system->manufacturer, system->product_name, system->version,
        system->serial_no, system->uuid
    );
}


SARAH_API int sarah_hd_board_init(sarah_hd_board_t *board)
{
    memset(board, 0x00, sizeof(sarah_hd_board_t));
    return 0;
}

SARAH_API void sarah_hd_board_destroy(sarah_hd_board_t *board)
{
}

SARAH_API void sarah_print_hd_board(sarah_hd_board_t *board)
{
    printf(
        "manufacturer=%s, product_name=%s, version=%s, serial_no=%s\n",
        board->manufacturer, board->product_name, board->version, board->serial_no
    );
}

SARAH_STATIC int _parse_bios_info(
    sarah_hd_bios_t *bios, FILE *fd, char *buffer, size_t size)
{
    register int val_len;
    char key[48] = {'\0'}, val[256] = {'\0'}, unit[3] = {'\0'};
    while ( fgets(buffer, size, fd) != NULL ) {
        // printf("buffer=%s, len=%ld\n", buffer, strlen(buffer));
        if ( strlen(buffer) < 2 ) {
            break;
        }

        if ( strchr(buffer, ':') == NULL ) {
            continue;
        }

        sscanf(buffer, "%32[^:]:%*[ ]%255[^\n]", key, val);
        cel_left_trim(key);
        cel_right_trim(val);
        // printf("key=%s, value=%s\n", key, val);

        val_len = strlen(val);
        if ( strcmp(key, "Vendor") == 0 ) {
            memcpy(bios->vendor, val, val_len >= 40 ? 39 : val_len);
        } else if ( strcmp(key, "Version") == 0 ) {
            memcpy(bios->version, val, val_len >= 64 ? 63 : val_len);
        } else if ( strcmp(key, "Release Date") == 0 ) {
            memcpy(bios->release_date, val, val_len >= 12 ? 12 : val_len);
        } else if ( strcmp(key, "Runtime Size") == 0 ) {
            sscanf(val, "%u %s", &bios->runtime_size, unit);
            if ( unit[0] == 'k' || unit[0] == 'K') {
                bios->runtime_size = bios->runtime_size * 1024;
            } else if ( unit[0] == 'M' || unit[0] == 'm' ) {
                bios->runtime_size = bios->runtime_size * 1024 * 1024;
            }
        } else if ( strcmp(key, "ROM Size") == 0 ) {
            sscanf(val, "%u %s", &bios->rom_size, unit);
            if ( unit[0] == 'k' || unit[0] == 'K') {
                bios->rom_size = bios->rom_size * 1024;
            } else if ( unit[0] == 'M' || unit[0] == 'm' ) {
                bios->rom_size = bios->rom_size * 1024 * 1024;
            }
        }
    }

    return 0;
}

SARAH_STATIC int _parse_system_info(
    sarah_hd_system_t *system, FILE *fd, char *buffer, size_t size)
{
    register int val_len;
    char key[48] = {'\0'}, val[256] = {'\0'};
    while ( fgets(buffer, size, fd) != NULL ) {
        if ( strlen(buffer) < 2 ) {
            break;
        }

        if ( strchr(buffer, ':') == NULL ) {
            continue;
        }

        sscanf(buffer, "%32[^:]:%*[ ]%255[^\n]", key, val);
        cel_left_trim(key);
        cel_right_trim(val);
        // printf("key=%s, value=%s\n", key, val);

        val_len = strlen(val);
        if ( strcmp(key, "Manufacturer") == 0 ) {
            memcpy(system->manufacturer, val, val_len >= 40 ? 39 : val_len);
        } else if ( strcmp(key, "Product Name") == 0 ) {
            memcpy(system->product_name, val, val_len >= 24 ? 23 : val_len);
        } else if ( strcmp(key, "Version") == 0 ) {
            memcpy(system->version, val, val_len >= 24 ? 23 : val_len);
        } else if ( strcmp(key, "Serial Number") == 0 ) {
            memcpy(system->serial_no, val, val_len >= 40 ? 39 : val_len);
            cel_strtolower(system->serial_no, val_len);
        } else if ( strcmp(key, "UUID") == 0 ) {
            memcpy(system->uuid, val, val_len >= 40 ? 39 : val_len);
            cel_strtolower(system->uuid, val_len);
        }
    }

    return 0;
}

SARAH_STATIC int _pase_board_info(
    sarah_hd_board_t *board, FILE *fd, char *buffer, size_t size)
{
    register int val_len;
    char key[48] = {'\0'}, val[256] = {'\0'};
    while ( fgets(buffer, size, fd) != NULL ) {
        if ( strlen(buffer) < 2 ) {
            break;
        }

        if ( strchr(buffer, ':') == NULL ) {
            continue;
        }

        sscanf(buffer, "%32[^:]:%*[ ]%255[^\n]", key, val);
        cel_left_trim(key);
        cel_right_trim(val);
        // printf("key=%s, value=%s\n", key, val);

        val_len = strlen(val);
        if ( strcmp(key, "Manufacturer") == 0 ) {
            memcpy(board->manufacturer, val, val_len >= 40 ? 39 : val_len);
        } else if ( strcmp(key, "Product Name") == 0 ) {
            memcpy(board->product_name, val, val_len >= 24 ? 23 : val_len);
        } else if ( strcmp(key, "Version") == 0 ) {
            memcpy(board->version, val, val_len >= 24 ? 23 : val_len);
        } else if ( strcmp(key, "Serial Number") == 0 ) {
            memcpy(board->serial_no, val, val_len >= 40 ? 39 : val_len);
            cel_strtolower(board->serial_no, val_len);
        }
    }

    return 0;
}


/**
 * interface to fetch the hardware information
 * and fill the bios, system, board entry base on the decode information in DMI.
 *
 * @param   bios
 * @param   system
 * @param   board
 * @return  int 0 for succeed or the errno
*/
SARAH_API int sarah_get_hd_info(
    sarah_hd_bios_t *bios, sarah_hd_system_t *system, sarah_hd_board_t *board)
{
    FILE *fd = NULL;
    char buffer[256] = {'\0'};

    if ( (fd = popen("dmidecode", "r")) == NULL ) {
        return 1;
    }

    while ( fgets(buffer, sizeof(buffer), fd) != NULL ) {
        if ( strncmp(buffer, "BIOS Info", 9) == 0 ) {
            _parse_bios_info(bios, fd, buffer, sizeof(buffer));
        } else if ( strncmp(buffer, "System Info", 11) == 0 ) {
            _parse_system_info(system, fd, buffer, sizeof(buffer));
        } else if ( strncmp(buffer, "Base Board Info", 15) == 0 ) {
            _pase_board_info(board, fd, buffer, sizeof(buffer));
        }
    }

    /* 
     * check and merge the system information to the board information
     * CUZ some system base board is not filled 
     *  but it usually extended from the system information.
    */

    pclose(fd);
    return 0;
}
