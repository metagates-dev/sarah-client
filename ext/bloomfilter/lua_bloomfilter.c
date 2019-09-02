/**
 * sarah hash extension implementations
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "cel_bloomfilter.h"
#include "sarah.h"
#include "sarah_utils.h"

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

#define L_METATABLE_NAME "_ext_bloomfilter"

static int lua_bloomfilter_new(lua_State *L)
{
    cel_bloomfilter_t *self;
    int amount, bits;
    float false_rate;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Estimated amount and false rate expected");
    amount = luaL_checkinteger(L, 1);
    false_rate = luaL_checknumber(L, 2);

    self = (cel_bloomfilter_t *) lua_newuserdata(L, sizeof(cel_bloomfilter_t));
    luaL_getmetatable(L, L_METATABLE_NAME);
    lua_setmetatable(L, -2);

    /* initialize the bloomfilter */
    bits = cel_bloomfilter_optimal_bits(amount, false_rate);
    cel_bloomfilter_init(
        self, bits,
        cel_bloomfilter_optimal_hfuncs(amount, bits)
    );

    // printf("bits=%d, hfuncs=%d\n", bits, self->hsize);
    return 1;
}

static int lua_bloomfilter_destroy(lua_State *L)
{
    cel_bloomfilter_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "object expected");
    self = (cel_bloomfilter_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    cel_bloomfilter_destroy(self);

    return 0;
}

static int lua_bloomfilter_add(lua_State *L)
{
    cel_bloomfilter_t *self;
    char *str = NULL;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "object and string expected");
    self = (cel_bloomfilter_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    str = (char *) luaL_checkstring(L, 2);

    lua_pushboolean(L, cel_bloomfilter_add(self, str) == 1 ? 1 : 0);
    return 1;
}

static int lua_bloomfilter_exists(lua_State *L)
{
    cel_bloomfilter_t *self;
    char *str = NULL;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "object and string expected");
    self = (cel_bloomfilter_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    str = (char *) luaL_checkstring(L, 2);

    lua_pushboolean(L, cel_bloomfilter_exists(self, str) == 1 ? 1 : 0);
    return 1;
}

static int lua_bloomfilter_length(lua_State *L)
{
    cel_bloomfilter_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "object expected");
    self = (cel_bloomfilter_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    lua_pushinteger(L, self->length);
    return 1;
}

static int lua_bloomfilter_size(lua_State *L)
{
    cel_bloomfilter_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "object expected");
    self = (cel_bloomfilter_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    lua_pushinteger(L, self->size);
    return 1;
}

static int lua_bloomfilter_hsize(lua_State *L)
{
    cel_bloomfilter_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "object expected");
    self = (cel_bloomfilter_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    lua_pushinteger(L, self->hsize);
    return 1;
}

/** module method array */
static const struct luaL_Reg bloomfilter_methods[] = {
    { "add",        lua_bloomfilter_add },
    { "exists",     lua_bloomfilter_exists },
    { "length",     lua_bloomfilter_length },
    { "size",       lua_bloomfilter_size },
    { "hsize",      lua_bloomfilter_hsize },
    { "__len",      lua_bloomfilter_size },
    { "__gc",       lua_bloomfilter_destroy },
    { NULL, NULL }
};

/** module function array */
static const struct luaL_Reg bloomfilter_functions[] = {
    { "new",        lua_bloomfilter_new },
    { NULL, NULL }
};


/**
 * register the current library to specified stack
 *
 * @param   L
 * @return  integer
*/
int luaopen_bloomfilter(lua_State *L)
{
    /* Create a metatale and push it onto the stack */
    luaL_newmetatable(L, L_METATABLE_NAME);

    /* Duplicate the metatable on the stack */
    lua_pushvalue(L, -1);

    /* Pop the first metatable off the stack
     * and assign it to the __index of the second one.
     * so we set the metatable to the table itself.
    */
    lua_setfield(L, -2, "__index");

    /**
     * set the methods of the metatable that could and should be
     * accessed via object.func in lua block
    */
    luaL_setfuncs(L, bloomfilter_methods, 0);
    luaL_setfuncs(L, bloomfilter_functions, 0);

    return 1;
}
