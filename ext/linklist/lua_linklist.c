/**
 * sarah linklist extension implementations.
 *
 * @Note: READ this before u use this extension to the production env.
 * For the basic data type storage there will be no problems.
 * For the complex data type which in lua called GCObject storage still need
 *  more production testing, here is the reasons:
 *
 * 1, i currently directly copy and stored the address of the GCObject.
 * 2, Only passed some simple testing.
 * 3, Lack of experience of the implementation of the GC mechansim of lua.
 * 4, Lua use the Mark-Sweep GC implementation.
 * May comming problem: 
 * Will the copied GCObject be collected during long-time running production env ?
 * Yeah, that will be a disaster if this happend under a production env.
 *
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "linklist.h"
#include "sarah.h"
#include "sarah_utils.h"

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
#include <lua/lobject.h>
#include <lua/lstate.h>
#include <lua/lapi.h>
#include <lua/lgc.h>

#define L_METATABLE_NAME "_ext_linklist"
#define L_ITERATOR_NAME "_ext_linklist_iterator"
#define getTValue(L, idx) (L->ci->func + idx)
#define endOfLink(it) (it->cur->_next == NULL || it->cur->_next == it->link->tail)
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

static int iterator_new(cel_link_t *, lua_State *);

/**
 * interator interface for linklist
 * API design:
 * local it = list:iterator();
 * while ( it:hasNext() ) do
 *      local v = it:next();
 * end
*/
typedef struct linklist_iterator_entry {
    cel_link_t *link;
    cel_link_node_t *cur;
} linklist_iterator_t;


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

static int lua_linklist_new(lua_State *L)
{
    cel_link_t *self;

    /* Create the user data and push it onto the stack */
    self = (cel_link_t *) lua_newuserdata(L, sizeof(cel_link_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_METATABLE_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the link list */
    if ( cel_link_init(self) != 1 ) {
        luaL_error(L, "Fail to initialize the linklist");
    }

    return 1;
}

static int lua_linklist_destroy(lua_State *L)
{
    cel_link_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    cel_link_destroy(self, mval_free);

    return 0;
}


static int lua_linklist_insert(lua_State *L)
{
    cel_link_t *self;
    int idx;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 3, 3, "Object index and value expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);
    val = pack(getTValue(L, 3));

    cel_link_insert(self, idx, val);
    return 0;
}

static int lua_linklist_add_first(lua_State *L)
{
    cel_link_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and value expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    val = pack(getTValue(L, 2));

    cel_link_add_first(self, val);
    return 0;
}

static int lua_linklist_add_last(lua_State *L)
{
    cel_link_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and value expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    val = pack(getTValue(L, 2));

    cel_link_add_last(self, val);
    return 0;
}


static int lua_linklist_remove(lua_State *L)
{
    cel_link_t *self;
    int idx;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and index expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);

    val = (mval *) cel_link_remove(self, idx);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
        mval_free(val);
    }

    return 1;
}

static int lua_linklist_remove_first(lua_State *L)
{
    cel_link_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    val = (mval *) cel_link_remove_first(self);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
        mval_free(val);
    }

    return 1;
}

static int lua_linklist_remove_last(lua_State *L)
{
    cel_link_t *self;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    val = (mval *) cel_link_remove_last(self);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
        mval_free(val);
    }

    return 1;
}


static int lua_linklist_get(lua_State *L)
{
    cel_link_t *self;
    int idx;
    mval *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and index expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);

    val = (mval *) cel_link_get(self, idx);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, val);
    }

    return 1;
}

static int lua_linklist_set(lua_State *L)
{
    cel_link_t *self;
    int idx;
    mval *old, *new;

    luaL_argcheck(L, lua_gettop(L) == 3, 3, "Object index and value expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    idx = luaL_checkinteger(L, 2);
    lua_idxcheck(idx, 2);
    new = pack(getTValue(L, 3));

    old = (mval *) cel_link_set(self, idx, new);
    if ( old == NULL ) {
        lua_pushnil(L);
    } else {
        push_mval(L, old);
        mval_free(old);
    }

    return 1;
}

static int lua_linklist_size(lua_State *L)
{
    cel_link_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "Object expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    lua_pushinteger(L, self->size);
    return 1;
}

/**
 * create and return a new iterator for the current linklist
*/
static int lua_linklist_iterator(lua_State *L)
{
    cel_link_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (cel_link_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    /* create and initialize the iterator */
    iterator_new(self, L);

    return 1;
}

static int iterator_new(cel_link_t *link, lua_State *L)
{
    linklist_iterator_t *it;

    /* Create the user data and push it onto the stack */
    it = (linklist_iterator_t *) lua_newuserdata(L, sizeof(linklist_iterator_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_ITERATOR_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    it->link = link;
    it->cur = link->head;
    return 1;
}

static int iterator_destroy(lua_State *L)
{
    // DO nothing here right now
    return 0;
}

static int iterator_has_next(lua_State *L)
{
    linklist_iterator_t *it;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "iterator expected");
    it = (linklist_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);

    lua_pushboolean(L, endOfLink(it) ? 0 : 1);
    return 1;
}

static int iterator_next(lua_State *L)
{
    linklist_iterator_t *it;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "iterator expected");
    it = (linklist_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);

    if ( endOfLink(it) ) {
        lua_pushnil(L);
    } else {
        it->cur = it->cur->_next;
        push_mval(L, (mval *) it->cur->value);
    }

    return 1;
}

static int iterator_rewind(lua_State *L)
{
    linklist_iterator_t *it;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "iterator expected");
    it = (linklist_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);

    it->cur = it->link->head;
    return 0;
}


/* Lua module method array */
static const struct luaL_Reg linklist_methods[] = {
    { "new",            lua_linklist_new },
    { "insert",         lua_linklist_insert },
    { "add",            lua_linklist_add_last },
    { "addFirst",       lua_linklist_add_first },
    { "addLast",        lua_linklist_add_last },
    { "remove",         lua_linklist_remove },
    { "removeFirst",    lua_linklist_remove_first },
    { "removeLast",     lua_linklist_remove_last },
    { "get",            lua_linklist_get },
    { "set",            lua_linklist_set },
    { "size",           lua_linklist_size },
    { "iterator",       lua_linklist_iterator },
    { "__len",          lua_linklist_size },
    { "__gc",           lua_linklist_destroy },
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
int luaopen_linklist(lua_State *L)
{

    /* Register the iterator metatable */
    luaL_newmetatable(L, L_ITERATOR_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, iterator_methods, 0);

    /* Register the linklist metatable */
    luaL_newmetatable(L, L_METATABLE_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, linklist_methods, 0);

    return 1;
}
