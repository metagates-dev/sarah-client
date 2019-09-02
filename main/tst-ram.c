/**
 * ram interface test
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    sarah_ram_stat_t ram;
    sarah_get_ram_stat(&ram);
    sarah_print_ram_stat(&ram);
    return 0;
}
