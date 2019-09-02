/**
 * Lua executor implementation with the executor interface implemented
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include "sarah.h"
#include "sarah_utils.h"
#include "april.h"

#define G_EXECUTOR_PTR "G_EXECUTOR_PTR"

/*
 * Lua print redirecting function
 * we will print the content to the executor buffer by default
*/
static int lua_echo(lua_State *L)
{
    sarah_executor_t *self = NULL;
    int i, t, argc = lua_gettop(L);
    char *_ptr = NULL, _str[41] = {'\0'};
    cJSON *json = NULL;

    luaL_argcheck(L, argc > 0, 1, "What do you want to echo?");
    if ( lua_getglobal(L, G_EXECUTOR_PTR) == LUA_TLIGHTUSERDATA ) {
        self = lua_touserdata(L, -1);
    }

    for ( i = 1; i <= argc; i++ ) {
        _ptr = NULL;
        t = lua_type(L, i);
        switch ( t ) {
        case LUA_TSTRING:
            _ptr = (char *) lua_tostring(L, i);
            break;
        case LUA_TNUMBER:
            memset(_str, 0x00, sizeof(_str));
            if ( lua_isinteger(L, i) ) {
                sprintf(_str, "%d", (int) lua_tonumber(L, i));
            } else {
                sprintf(_str, "%lf", lua_tonumber(L, i));
            }
            break;
        case LUA_TBOOLEAN:
            _ptr = lua_toboolean(L, i) == 1 ? "true" : "false";
            break;
        case LUA_TNIL:
            _ptr = "nil";
            break;
        case LUA_TTABLE:
            json = table_is_array(L, i) ? cJSON_CreateArray() : cJSON_CreateObject();
            table_to_json(json, L, i);
            _ptr = cJSON_PrintUnformatted(json);
            cJSON_Delete(json);
            break;
        case LUA_TUSERDATA:
            /* push onto the stack the function to invoke */
            if ( lua_getfield(L, i, "__tostring") != LUA_TFUNCTION ) {
                _ptr = "Object with no __string method defined";
            } else {
                lua_pushvalue(L, i);    /* the first self argument */
                lua_pcall(L, 1, 1, 0);  /* Invoke the self.__tostring interface */
                _ptr = (char *) lua_tostring(L, -1); /* no matter the error or result */
                lua_pop(L, 1);  /* Pop the function result */
            }
            break;
        default:
            luaL_argerror(L, i, "Unsupport arg type to print");
            break;
        }

        /* do the actual print */
        if ( self == NULL ) {
            printf("%s", _ptr == NULL ? _str : _ptr);
        } else {
            sarah_executor_data_append(self, _ptr == NULL ? _str : _ptr);
        }

        if ( t == LUA_TTABLE ) {
            free(_ptr);
        }
    }

    return 0;
}

/**
 * internal method to register all the available libs
 * to the current lua state.
 *
 * @param   executor
 * @param   L
*/
SARAH_STATIC void init_lua_script(sarah_executor_t *executor, lua_State *L)
{
    /* Opens all standard lua libraries into the given state */
    luaL_openlibs(L);

    /* Register the april std library */
    luaopen_april(L);

    /* Register the executor object directly */
    lua_pushlightuserdata(L, executor);
    lua_setglobal(L, G_EXECUTOR_PTR);
    luaopen_executor(L);

    /* Register the extra functions */
    lua_register(L, "echo", lua_echo);

    /* april std config */
    april_std_config(L);
}


/**
 * Lua Basic string executor
 *
 * @param   executor
 * @param   str
 * @param   argv
 * @return  0 for success and none 0 for failed
*/
SARAH_API int script_lua_dostring(sarah_executor_t *executor, char *str, char *argv)
{
    int status, errno = 0;

    lua_State *L = luaL_newstate();
    if ( L == NULL ) {
        return -1;
    }

    /* Initialize the lua script */
    init_lua_script(executor, L);

    /* Register the global argv var for arguments */
    if ( argv == NULL ) {
        lua_pushliteral(L, "");
    } else {
	    lua_pushstring(L, argv);
    }
	lua_setglobal(L, "argv");

    status = luaL_loadstring(L, str);
    switch ( status ) {
    case LUA_OK:
        break;
    case LUA_ERRSYNTAX:
        errno = SARAH_EXECUTOR_ERRSYNTAX;
        goto error;
    case LUA_ERRMEM:
        errno = SARAH_EXECUTOR_ERRMEM;
        goto error;
    case LUA_ERRGCMM:
        errno = SARAH_EXECUTOR_ERRGC;
        goto error;
    }

    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    switch ( status ) {
    case LUA_OK:
        break;
    case LUA_ERRRUN:
        errno = SARAH_EXECUTOR_ERRRUN;
        goto error;
    case LUA_ERRMEM:
        errno = SARAH_EXECUTOR_ERRMEM;
        goto error;
    // Will never happen, Since we did not set the error handler
    case LUA_ERRERR:
        errno = SARAH_EXECUTOR_ERRERR;
        goto error;
    case LUA_ERRGCMM:
        errno = SARAH_EXECUTOR_ERRGC;
        goto error;
    }

error:
    if ( errno > 0 ) {
        sarah_executor_error_append(executor, (char *) lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_close(L);
    return errno;
}

/**
 * load and execute a lua file
 *
 * @param   executor
 * @param   str
 * @param   argv
 * @return  int 0 for succeed and none 0 for failed
*/
SARAH_API int script_lua_dofile(sarah_executor_t *executor, char *file, char *argv)
{
    int status, errno = 0;

    lua_State *L = luaL_newstate();
    if ( L == NULL ) {
        return -1;
    }

    /* Initialize the lua script */
    init_lua_script(executor, L);

    /* Register the global argv var for arguments */
    if ( argv == NULL ) {
        lua_pushliteral(L, "");
    } else {
	    lua_pushstring(L, argv);
    }
	lua_setglobal(L, "argv");

    status = luaL_loadfile(L, file);
    switch ( status ) {
    case LUA_OK:
        break;
    case LUA_ERRFILE:
        errno = SARAH_EXECUTOR_ERRFILE;
        goto error;
    case LUA_ERRSYNTAX:
        errno = SARAH_EXECUTOR_ERRSYNTAX;
        goto error;
    case LUA_ERRMEM:
        errno = SARAH_EXECUTOR_ERRMEM;
        goto error;
    case LUA_ERRGCMM:
        errno = SARAH_EXECUTOR_ERRGC;
        goto error;
    }

    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    switch ( status ) {
    case LUA_OK:
        break;
    case LUA_ERRRUN:
        errno = SARAH_EXECUTOR_ERRRUN;
        goto error;
    case LUA_ERRMEM:
        errno = SARAH_EXECUTOR_ERRMEM;
        goto error;
    // Will never happen, Since we did not set the error handler
    case LUA_ERRERR:
        errno = SARAH_EXECUTOR_ERRERR;
        goto error;
    case LUA_ERRGCMM:
        errno = SARAH_EXECUTOR_ERRGC;
        goto error;
    }

error:
    if ( errno > 0 ) {
        sarah_executor_error_append(executor, (char *) lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_close(L);
    return 0;
}
