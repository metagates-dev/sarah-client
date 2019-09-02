/**
 * sarah arraylist extension implementations.
 * @Note: read more about the production use in linklist implementation module.
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include "sarah_utils.h"
#include "cel_array.h"

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
#include <lua/lobject.h>
#include <lua/lstate.h>
#include <lua/lapi.h>
#include <lua/lgc.h>

#define L_METATABLE_NAME "_ext_arraylist"
#define L_ITERATOR_NAME "_ext_arraylist_iterator"
#define getTValue(L, idx) (L->ci->func + idx)
#define endOfArray(it) (it->idx >= it->list->size)
#define lua_idxcheck(idx, arg) \
{ if ( idx < 0 ) {              \
    idx = self->size + idx + 1; \
  }     \
  if ( idx < 1 ) {      \
    luaL_argerror(L, arg, "index should be start from 1"); \
  } else {              \
    idx--;              \
  }                     \
}

static int iterator_new(cel_array_t *, lua_State *);

/**
 * interator interface for arraylist
 * API design:
 * local it = list:iterator();
 * while ( it:hasNext() ) do
 *      local v = it:next();
 * end
*/
typedef struct arraylist_iterator_entry {
    cel_array_t *list;
    unsigned int idx;
} arraylist_iterator_t;


typedef union _value_entry {
  void *p;         /* light userdata */
  int b;           /* booleans */
  lua_CFunction f; /* light C functions */
  lua_Integer i;   /* integer numbers */
  lua_Number n;    /* float numbers */
} Value_t;

typedef struct _tvalue_entry {
    Value_t value_;
    int tt_;
} mval;

static mval *mval_new()
{
    mval *v = sarah_malloc(sizeof(mval));
    if ( v == NULL ) {
        return NULL;
    }

    memset(v, 0x00, sizeof(mval));
    return v;
}

static void mval_free(void *ptr)
{
    mval *val = (mval *) ptr;
    if ( val != NULL ) {
        sarah_free(val);
    }
}


/*
 * create and pack the lua TValue to the mval
*/
static mval *pack(TValue *o)
{
    mval *v = NULL;
    switch ( ttnov(o) ) {
    case LUA_TNUMBER:
        v = mval_new();
        if ( ttisinteger(o) ) {
            v->value_.i = ivalue(o);
            v->tt_ = LUA_TNUMINT;
        } else {
            v->value_.n = nvalue(o);
            v->tt_ = LUA_TNUMBER;
        }
        break;
    case LUA_TBOOLEAN:
        v = mval_new();
        v->value_.b = bvalue(o);
        v->tt_ = LUA_TBOOLEAN;
        break;
    case LUA_TLIGHTUSERDATA:
        v = mval_new();
        v->value_.p = pvalue(o);
        v->tt_ = LUA_TLIGHTUSERDATA;
        break;
    case LUA_TSTRING:
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TTHREAD: 
    case LUA_TUSERDATA: 
        v = mval_new();
        v->value_.p = gcvalue(o);
        v->tt_ = ttnov(o);
        break;
    }

    return v;
}

static void push_mval(lua_State *L, mval *val)
{
    switch ( val->tt_ ) {
    case LUA_TNUMBER:
        lua_pushnumber(L, val->value_.n);
        break;
    case LUA_TNUMINT:
        lua_pushinteger(L, val->value_.i);
        break;
    case LUA_TBOOLEAN:
        lua_pushboolean(L, val->value_.b);
        break;
    case LUA_TLIGHTUSERDATA:
        lua_pushlightuserdata(L, val->value_.p);
        break;
    // string
    case LUA_TSTRING:
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TTHREAD: 
    case LUA_TUSERDATA: 
        lua_lock(L);
        setgcovalue(L, L->top, (GCObject *) val->value_.p);
        settt_(L->top, val->tt_);
        api_incr_top(L);
        // luaC_checkGC(L);
        lua_unlock(L);
        break;
    }
}


static int lua_arraylist_new(lua_State *L)
{
    cel_array_t *self;

    /* Create the user data and push it onto the stack */
    self = (cel_array_t *) lua_newuserdata(L, sizeof(cel_array_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_METATABLE_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the array list */
    if ( cel_array_init(self, 16) != 1 ) {
        luaL_error(L, "Fail to initialize the arraylist");
    }

    return 1;
}

static int lua_arraylist_destroy(lua_State *L)
{
    cel_array_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    cel_array_destroy(self, mval_free);

    return 0;
}


static int lua_arraylist_insert(lua_State *L)
{
    cel_array_t *self;
    int idx;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 3, 3, "Object index and value expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);
    val = pack(getTValue(L, 3));

    cel_array_insert(self, idx, val);
    return 0;
}

static int lua_arraylist_add_first(lua_State *L)
{
    cel_array_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and value expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    val = pack(getTValue(L, 2));

    cel_array_insert(self, 0, val);
    return 0;
}

static int lua_arraylist_add_last(lua_State *L)
{
    cel_array_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and value expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    val = pack(getTValue(L, 2));

    cel_array_add(self, val);
    return 0;
}


static int lua_arraylist_remove(lua_State *L)
{
    cel_array_t *self;
    int idx;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and index expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);

    val = (mval *) cel_array_del(self, idx);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
        mval_free(val);
    }

    return 1;
}

static int lua_arraylist_remove_first(lua_State *L)
{
    cel_array_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    val = (mval *) cel_array_del(self, 0);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
        mval_free(val);
    }

    return 1;
}

static int lua_arraylist_remove_last(lua_State *L)
{
    cel_array_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    val = (mval *) cel_array_del(self, self->size - 1);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
        mval_free(val);
    }

    return 1;
}

static int lua_arraylist_get(lua_State *L)
{
    cel_array_t *self;
    int idx;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and index expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);

    val = (mval *) cel_array_get(self, idx);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
    }

    return 1;
}

static int lua_arraylist_set(lua_State *L)
{
    cel_array_t *self;
    int idx;
    mval *old, *new;

    luaL_argcheck(L, lua_gettop(L) == 3, 3, "Object index and value expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);
    new = pack(getTValue(L, 3));

    old = (mval *) cel_array_set(self, idx, new);
    if ( old == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, old);
        mval_free(old);
    }

    return 1;
}

static int lua_arraylist_size(lua_State *L)
{
    cel_array_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "Object expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    lua_pushinteger(L, self->size);
    return 1;
}

/**
 * create and return a new iterator for the current arraylist
*/
static int lua_arraylist_iterator(lua_State *L)
{
    cel_array_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_array_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    /* create and initialize the iterator */
    iterator_new(self, L);

    return 1;
}

static int iterator_new(cel_array_t *list, lua_State *L)
{
    arraylist_iterator_t *it;

    /* Create the user data and push it onto the stack */
    it = (arraylist_iterator_t *) lua_newuserdata(L, sizeof(arraylist_iterator_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_ITERATOR_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    it->list = list;
    it->idx = 0;
    return 1;
}

static int iterator_destroy(lua_State *L)
{
    // DO nothing here right now
    return 0;
}

static int iterator_has_next(lua_State *L)
{
    arraylist_iterator_t *it;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "iterator expected");
    it = (arraylist_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);

    lua_pushboolean(L, endOfArray(it) ? 0 : 1);
    return 1;
}

static int iterator_next(lua_State *L)
{
    arraylist_iterator_t *it;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "iterator expected");
    it = (arraylist_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);

    if ( endOfArray(it) ) {
        lua_pushnil(L);
    } else {
        push_mval(L, (mval *) cel_array_get(it->list, it->idx));
        it->idx++;
    }

    return 1;
}

static int iterator_rewind(lua_State *L)
{
    arraylist_iterator_t *it;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "iterator expected");
    it = (arraylist_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);

    it->idx = 0;
    return 0;
}


/* Lua module method array */
static const struct luaL_Reg arraylist_methods[] = {
    { "new",            lua_arraylist_new },
    { "insert",         lua_arraylist_insert },
    { "add",            lua_arraylist_add_last },
    { "addFirst",       lua_arraylist_add_first },
    { "addLast",        lua_arraylist_add_last },
    { "remove",         lua_arraylist_remove },
    { "removeFirst",    lua_arraylist_remove_first },
    { "removeLast",     lua_arraylist_remove_last },
    { "get",            lua_arraylist_get },
    { "set",            lua_arraylist_set },
    { "size",           lua_arraylist_size },
    { "iterator",       lua_arraylist_iterator },
    { "__len",          lua_arraylist_size },
    { "__gc",           lua_arraylist_destroy },
    { NULL, NULL }
};

/** module function array */
static const struct luaL_Reg iterator_methods[] = {
    { "hasNext",    iterator_has_next },
    { "next",       iterator_next },
    { "rewind",     iterator_rewind },
    { "__gc",       iterator_destroy },
    { NULL, NULL }
};


/**
 * register the current library to specified stack
 *
 * @param   L
 * @return  integer
*/
int luaopen_arraylist(lua_State *L)
{
    /* Register the iterator metatable */
    luaL_newmetatable(L, L_ITERATOR_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, iterator_methods, 0);

    /* Register the arraylist metatable */
    luaL_newmetatable(L, L_METATABLE_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, arraylist_methods, 0);

    return 1;
}
