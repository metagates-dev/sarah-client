/**
 * sarah hash extension implementations
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include "sarah_utils.h"

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

#define L_METATABLE_NAME "_ext_hashfunc"

static int lua_bkdr_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_bkdr_hash(str));
    return 1;
}

static int lua_fnv_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_fnv_hash(str));
    return 1;
}

static int lua_fnvla_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_fnv1a_hash(str));
    return 1;
}

static int lua_ap_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_ap_hash(str));
    return 1;
}

static int lua_djp_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_djp_hash(str));
    return 1;
}

static int lua_djp2_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_djp2_hash(str));
    return 1;
}

static int lua_js_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_js_hash(str));
    return 1;
}

static int lua_sdms_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_sdms_hash(str));
    return 1;
}

static int lua_rs_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_rs_hash(str));
    return 1;
}

static int lua_dek_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_dek_hash(str));
    return 1;
}

static int lua_elf_hash(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_elf_hash(str));
    return 1;
}

static int lua_bobJenkins_hash32(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);

    lua_pushinteger(L, cel_bobJenkins32_hash(str));
    return 1;
}

static int lua_murmur_hash32(lua_State *L)
{
    char *str;
    unsigned int seed = 0x01, args_num = lua_gettop(L);

    luaL_argcheck(L, args_num >= 1, 1, "string expected");
    str = (char *) luaL_checkstring(L, 1);
    if ( args_num > 1 ) {
        seed = (unsigned int ) luaL_checkinteger(L, 2);
    }

    lua_pushinteger(L, cel_murmur_hash32(str, strlen(str), seed));
    return 1;
}

static int lua_all_functions(lua_State *L)
{
    lua_pushstring(
        L, 
        "all hash functions: \n"
        "uint32_t hashfunc.bkdr(string)\n"
        "uint32_t hashfunc.fnv(string)\n"
        "uint32_t hashfunc.fnvla(string)\n"
        "uint32_t hashfunc.ap(string)\n"
        "uint32_t hashfunc.djp(string)\n"
        "uint32_t hashfunc.djp2(string)\n"
        "uint32_t hashfunc.js(string)\n"
        "uint32_t hashfunc.sdms(string)\n"
        "uint32_t hashfunc.rs(string)\n"
        "uint32_t hashfunc.dek(string)\n"
        "uint32_t hashfunc.elf(string)\n"
        "uint32_t hashfunc.bobJenkins32(string)\n"
        "uint32_t hashfunc.murmur32(string)\n"
    );

    return 1;
}

/* Lua module method array */
static const struct luaL_Reg hash_methods[] = {
    { "bkdr",   lua_bkdr_hash },
    { "fnv",    lua_fnv_hash },
    { "fnvla",  lua_fnvla_hash },
    { "ap",     lua_ap_hash },
    { "djp",    lua_djp_hash },
    { "djp2",   lua_djp2_hash },
    { "js",     lua_js_hash },
    { "sdms",   lua_sdms_hash },
    { "rs",     lua_rs_hash },
    { "dek",    lua_dek_hash },
    { "elf",    lua_elf_hash },
    { "bobJenkins32",   lua_bobJenkins_hash32 },
    { "murmur32",       lua_murmur_hash32 },
    { "all",            lua_all_functions },
    { "__tostring",     lua_all_functions },
    { NULL, NULL }
};


/**
 * register the current library to specified stack
 *
 * @param   L
 * @return  integer
*/
int luaopen_hashfunc(lua_State *L)
{
    /* Create a metatale and push it onto the stack */
    luaL_newmetatable(L, L_METATABLE_NAME);

    /**
     * set the methods of the metatable that could and should be
     * accessed via object.func in lua block
    */
    luaL_setfuncs(L, hash_methods, 0);

    return 1;
}
