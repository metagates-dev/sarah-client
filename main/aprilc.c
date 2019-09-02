/**
 * april script language
 * 
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sarah.h"
#include "april.h"

int main(int argc, char *argv[])
{
    int status;
    char *file;
    lua_State *L = NULL;

    if ( argc < 2 ) {
        printf("Usage: %s script path\n", argv[0]);
        return 0;
    }

    /* Check and make the april base lib dir */
    if ( sarah_mkdir(APRIL_PACKAGE_BASE_DIR, 0755) != 0 ) {
        printf("[Error]: Failed to make the lib base dir=%s\n", APRIL_PACKAGE_BASE_DIR);
        return 0;
    }

    file = argv[1];

    /* Invoke the april lua engine to execute the file */
    L = luaL_newstate();
    if ( L == NULL ) {
        return -1;
    }

    /* Opens all standard lua libraries into the given state */
    luaL_openlibs(L);

    /* Register the april std library */
    luaopen_april(L);

    /* april std config */
    april_std_config(L);

    status = luaL_loadfile(L, file);
    if ( status != LUA_OK ) {
        goto error;
    }

    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if ( status != LUA_OK ) {
        goto error;
    }

    lua_close(L);
    return 0;

error:
    printf("%s\n", (char *) lua_tostring(L, -1));
    lua_pop(L, 1);
    lua_close(L);

    return 1;
}
