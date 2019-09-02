/**
 * Ip2region lua c binding
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
 * @date    2018/10/04
*/

#include "ip2region.h"
#include <stdio.h>
#include <stdlib.h>

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>


#define L_METATABLE_NAME "_ext_ip2region"

/** create a new ip2region object with a specified dbFile */
static int lua_ip2region_new(lua_State *L)
{
    ip2region_entry *self;
    const char *dbFile;

    /* Check the arguments are valid */
    dbFile = luaL_checkstring(L, 1);
    if ( dbFile == NULL ) {
        luaL_error(L, "dbFile cannot be empty");
    }

    /* Create the user data and push it onto the stack */
    self = (ip2region_entry *) lua_newuserdata(L, sizeof(ip2region_entry));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_METATABLE_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the entry */
    if ( ip2region_create(self, dbFile) != 1 ) {
        luaL_error(L, "Fail to Initialize the ip2region with specified dbFile");
    }

    return 1;
}

/** destroy the specified ip2region instance */
static int lua_ip2region_destroy(lua_State *L)
{
    ip2region_entry *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (ip2region_entry *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    ip2region_destroy(self);

    return 0;
}



#define get_search_args(L, entry, ip) \
do {    \
    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and ip address expected"); \
    entry = (ip2region_entry *) luaL_checkudata(L, 1, L_METATABLE_NAME); \
    ip = luaL_checkstring(L, 2);  \
} while (0); \


#define set_search_result(L, data) \
do { \
    lua_newtable(L); \
    lua_pushinteger(L, data.city_id); \
    lua_setfield(L, -2, "city_id"); \
    lua_pushfstring(L, "%s", data.region); \
    lua_setfield(L, -2, "region"); \
} while (0); \


/** ip2region_memory_search wrapper */
static int lua_ip2region_memory_search(lua_State *L)
{
    ip2region_entry *self;
    const char *addr;
    datablock_entry data;

    get_search_args(L, self, addr);

    /* do the memory search */
    if ( ip2region_memory_search(self, ip2long(addr), &data) == 0 ) {
        lua_pushnil(L);
        return 1;
    }
    
    set_search_result(L, data);

    return 1;
}

/** ip2region_binary_search wrapper */
static int lua_ip2region_binary_search(lua_State *L)
{
    ip2region_entry *self;
    const char *addr;
    datablock_entry data;

    /* Check and get the search parameters */
    get_search_args(L, self, addr);

    /* Do the binary search */
    if ( ip2region_binary_search(self, ip2long(addr), &data) == 0 ) {
        lua_pushnil(L);
        return 1;
    }

    /* Set the return value */
    set_search_result(L, data);

    return 1;
}

/** ip2region_btree_search wrapper */
static int lua_ip2region_btree_search(lua_State *L)
{
    ip2region_entry *self;
    const char *addr;
    datablock_entry data;

    /* Check and get the search parameters */
    get_search_args(L, self, addr);

    /* Do the btree search */
    if ( ip2region_btree_search(self, ip2long(addr), &data) == 0 ) {
        lua_pushnil(L);
        return 1;
    }

    /* Set the return value */
    set_search_result(L, data);

    return 1;
}


/** ip2long wrapper */
static int lua_ip2long(lua_State *L)
{
    int argc;
    const char *addr;
    uint_t ipval;

    argc = lua_gettop(L);
    if ( argc == 1 ) {
        addr = luaL_checkstring(L, 1);
    } else {
        luaL_checkudata(L, 1, L_METATABLE_NAME);
        addr = luaL_checkstring(L, 2);
    }

    if ( (ipval = ip2long(addr)) == 0 ) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushinteger(L, (ipval & 0x7FFFFFFF));

    return 1;
}

static int lua_ip2region_tostring(lua_State *L)
{
    ip2region_entry *self;
    self = (ip2region_entry *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    /* Push the string to return to lua */
    lua_pushfstring(L, 
        "dbFile=%s, headerLen=%d, fristIndexPtr=%d, lastIndexPtr=%d, totalBlocks=%d",
        self->dbFile, self->headerLen, self->firstIndexPtr,
        self->lastIndexPtr, self->totalBlocks
    );

    return 1;
}


/** module method array */
static const struct luaL_Reg ip2region_methods[] = {
    // { "new",            lua_ip2region_new },
    // { "ip2long",        lua_ip2long },
    { "memorySearch",   lua_ip2region_memory_search },
    { "binarySearch",   lua_ip2region_binary_search },
    { "btreeSearch",    lua_ip2region_btree_search },
    { "close",          lua_ip2region_destroy },
    { "__gc",           lua_ip2region_destroy },
    { "__tostring",     lua_ip2region_tostring },
    { NULL, NULL },
};

/** module function array */
static const struct luaL_Reg ip2region_functions[] = {
    { "new",        lua_ip2region_new },
    { "ip2long",    lua_ip2long },
    { NULL, NULL }
};

/** module open function interface */
int luaopen_ip2region(lua_State *L)
{
    /* Create a metatable and push it onto the stack */
    luaL_newmetatable(L, L_METATABLE_NAME);
    /* Duplicate the metatable on the stack */
    lua_pushvalue(L, -1);

    /* Pop the first metatable off the stack
     * and assign it to the __index of the second one.
     * so we set the metatable to the table itself.
    */
    lua_setfield(L, -2, "__index");


    /* Set the methods fo the metatable that could and should be 
     * access via object:func in lua block 
     */
    luaL_setfuncs(L, ip2region_methods, 0);
    luaL_setfuncs(L, ip2region_functions, 0);

    /* Finally register the object.func functions
     * into the table witch at the top of the stack */
    // luaL_newlib(L, ip2region_functions);

    return 1;
}
