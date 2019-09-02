/**
 * sarah lua executor internal module
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include "sarah_utils.h"
#include "april.h"


/** +- LUA executor object implementation and register */
#define G_EXECUTOR_PTR "G_EXECUTOR_PTR"

#define assert_global_executor(self) \
{ if ( lua_getglobal(L, G_EXECUTOR_PTR) != LUA_TLIGHTUSERDATA ) {   \
        luaL_argerror(L, 1, "internal args error"); \
  } self = lua_touserdata(L, -1); }

/**
 * interface to get the node information
 * {
 *  "id": "node id",
 *  "register_id": "global register uid",
 *  "system": "linux",
 *  "app_key": "app_key",
 *  "created_at": created unix timestamp,
 *  "last_boot_at": last boot unix timestamp,
 * }
*/
static int lua_executor_get_node(lua_State *L)
{
    sarah_executor_t *self;

    assert_global_executor(self);
    lua_newtable(L);
    // version
    lua_pushnumber(L, sarah_version());
    lua_setfield(L, -2, "version");

    // node id
    lua_pushstring(L, self->node->node_id);
    lua_setfield(L, -2, "id");

    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "reg_id");

    // system, =Linux
    lua_pushliteral(L, "Linux");
    lua_setfield(L, -2, "system");

    // system machine
    lua_pushstring(L, self->node->kernel_info.machine);
    lua_setfield(L, -2, "machine");

    // account app_key
    lua_pushstring(L, self->node->config.app_key);
    lua_setfield(L, -2, "app_key");

    // node create unix timestamp
    lua_pushinteger(L, self->node->created_at);
    lua_setfield(L, -2, "created_at");

    // node last boot unix timestamp
    lua_pushinteger(L, self->node->last_boot_at);
    lua_setfield(L, -2, "last_boot_at");

    return 1;
}

static int lua_executor_set_maxdatalen(lua_State *L)
{
    sarah_executor_t *self;
    int max_len, args = lua_gettop(L);

    luaL_argcheck(L, args >= 1, args, "max_len expected");
    assert_global_executor(self);
    max_len = (int) luaL_checkinteger(L, args);

    sarah_executor_set_maxdatalen(self, max_len);
    return 0;
}

static int lua_executor_append_data(lua_State *L)
{
    sarah_executor_t *self;
    char *data;
    int errno, args = lua_gettop(L);

    luaL_argcheck(L, args >= 1, args, "data expected");
    assert_global_executor(self);
    data = (char *) luaL_checkstring(L, args);

    errno = sarah_executor_data_append(self, data);
    lua_pushboolean(L, errno == 0 ? 1 : 0);

    return 1;
}

static int lua_executor_insert_data(lua_State *L)
{
    sarah_executor_t *self;
    int idx, errno, args = lua_gettop(L);
    char *data;

    luaL_argcheck(L, args >= 2, args, "index and data expected");
    assert_global_executor(self);
    data = (char *) luaL_checkstring(L, args);      // get the data to be inserted
    idx  = (int) luaL_checkinteger(L, args - 1);
    if ( idx < 0 ) {
        idx = sarah_executor_get_datalen(self) + idx + 1;
    }
    
    if ( idx < 1 ) {
        luaL_argerror(L, args - 1, "index should be start from 1");
    } else {
        idx--;
    }

    errno = sarah_executor_data_insert(self, idx, data);
    lua_pushboolean(L, errno == 0 ? 1 : 0);

    return 1;
}

static int lua_executor_clear_data(lua_State *L)
{
    sarah_executor_t *self;

    assert_global_executor(self);
    sarah_executor_data_clear(self);

    return 0;
}

static int lua_executor_set_status(lua_State *L)
{
    sarah_executor_t *self;
    int status, args = lua_gettop(L);
    float progress;

    luaL_argcheck(L, args >= 2, args, "status and progress expected");
    assert_global_executor(self);
    status = luaL_checkinteger(L, args - 1);
    progress = (float) luaL_checknumber(L, args);

    sarah_executor_set_status(self, status, progress);

    return 0;
}

static int lua_executor_get_datalen(lua_State *L)
{
    sarah_executor_t *self;

    assert_global_executor(self);
    lua_pushinteger(L, sarah_executor_get_datalen(self));

    return 1;
}

static int lua_executor_get_data(lua_State *L)
{
    sarah_executor_t *self;

    assert_global_executor(self);
    lua_pushstring(L, sarah_executor_get_data(self));

    return 1;
}

static int lua_executor_set_errno(lua_State *L)
{
    sarah_executor_t *self;
    int errno, args = lua_gettop(L);

    luaL_argcheck(L, args >= 1, args, "errno expected");
    assert_global_executor(self);
    errno = luaL_checkinteger(L, args);

    sarah_executor_set_errno(self, errno);

    return 0;
}

static int lua_executor_get_errno(lua_State *L)
{
    sarah_executor_t *self;

    assert_global_executor(self);
    lua_pushinteger(L, sarah_executor_get_errno(self));

    return 1;
}

static int lua_executor_set_attach(lua_State *L)
{
    sarah_executor_t *self;
    int attach, args = lua_gettop(L);

    luaL_argcheck(L, args >= 1, args, "attach expected");
    assert_global_executor(self);
    attach = luaL_checkinteger(L, args);

    sarah_executor_set_attach(self, attach);

    return 0;
}

static int lua_executor_get_attach(lua_State *L)
{
    sarah_executor_t *self;

    assert_global_executor(self);
    lua_pushinteger(L, sarah_executor_get_attach(self));

    return 1;
}

static int lua_executor_sync_status(lua_State *L)
{
    sarah_executor_t *self;

    assert_global_executor(self);
    lua_pushboolean(L, sarah_executor_sync_status(self) == 0 ? 1 : 0);

    return 0;
}

static int lua_executor_tostring(lua_State *L)
{
    sarah_executor_t *self;
    char buffer[41] = {'\0'};
    luaL_Buffer _buffer;

    assert_global_executor(self);
    luaL_buffinit(L, &_buffer);

    luaL_addstring(&_buffer, "status=");
    sprintf(buffer, "%d", self->status);
    luaL_addstring(&_buffer, buffer);

    luaL_addstring(&_buffer, ", progress=");
    sprintf(buffer, "%f", self->progress);
    luaL_addstring(&_buffer, buffer);

    luaL_addstring(&_buffer, ", errno=");
    sprintf(buffer, "%d", self->errno);
    luaL_addstring(&_buffer, buffer);

    luaL_addstring(&_buffer, ", max_datalen=");
    sprintf(buffer, "%d", self->max_datalen);
    luaL_addstring(&_buffer, buffer);

    luaL_addstring(&_buffer, ", errstr=");
    luaL_addstring(&_buffer, self->error_buffer.buffer);

    luaL_addstring(&_buffer, ", data=");
    luaL_addstring(&_buffer, self->data_buffer.buffer);

    // lua_pushstring(L, buff);
    luaL_pushresult(&_buffer);

    return 1;
}


/** module method array */
static const struct luaL_Reg executor_methods[] = {
    { "setMaxDataLen",  lua_executor_set_maxdatalen },
    { "appendData",     lua_executor_append_data },
    { "insertData",     lua_executor_insert_data },
    { "clearData",      lua_executor_clear_data },
    { "setErrno",       lua_executor_set_errno },
    { "getErrno",       lua_executor_get_errno },
    { "setAttach",      lua_executor_set_attach },
    { "getAttach",      lua_executor_get_attach },
    { "setStatus",      lua_executor_set_status },
    { "getNode",        lua_executor_get_node },
    { "getDataLen",     lua_executor_get_datalen },
    { "getData",        lua_executor_get_data },
    { "syncStatus",     lua_executor_sync_status },
    { "__tostring",     lua_executor_tostring },
    { NULL, NULL },
};


/**
 * register the current executor to specified stack
 *
 * @param   L
 * @return  integer
*/
int luaopen_executor(lua_State *L)
{
    /* Create a metatale and push it onto the stack */
    luaL_newmetatable(L, "april_executor");

    /**
     * set the methods of the metatable that could and should be
     * accessed via object:func in lua block
    */
    luaL_setfuncs(L, executor_methods, 0);

    /* Register the metatable as a global */
    if ( lua_getglobal(L, G_EXECUTOR_PTR) != LUA_TLIGHTUSERDATA ) {
        luaL_argerror(L, 1, "Failed to initialize the global executor");
    }

    lua_pop(L, 1);
    lua_setglobal(L, "executor");

    return 0;
}
