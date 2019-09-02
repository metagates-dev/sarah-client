/**
 * hd info fetch test program
 *
 * Compile:
 * gcc -g -Wall tst-hd.c hardware.c cel_string.c
*/

#include <stdio.h>
#include <stdlib.h>
#include "sarah.h"

int main(int argc, char **argv)
{
    sarah_hd_bios_t bios;
    sarah_hd_system_t system;
    sarah_hd_board_t board;

    sarah_hd_bios_init(&bios);
    sarah_hd_system_init(&system);
    sarah_hd_board_init(&board);

    sarah_get_hd_info(&bios, &system, &board);

    printf("<<<Result: \n");
    sarah_print_hd_bios(&bios);
    sarah_print_hd_system(&system);
    sarah_print_hd_board(&board);

    return 0;
}
