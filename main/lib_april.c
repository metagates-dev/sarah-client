/**
 * April std library with some basic functions implemented.
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <dirent.h>

#include <curl/curl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/statvfs.h>

#include "cJSON.h"
#include "cJSON_Utils.h"
#include "april.h"
#include "sarah_utils.h"


static int lua_json_encode(lua_State *L)
{
    char *json_str = NULL;
    cJSON *json = NULL;
    int top_len = 0, pretty = 0;

    top_len = lua_gettop(L);
    if ( top_len == 1 ) {
        // luaL_argcheck(L, 1, 1, "table expected");
        luaL_checktype(L, 1, LUA_TTABLE);
        pretty = 0;     // default to false
    } else {
        luaL_argcheck(L, top_len == 2, 2, "table and pretty expected");
        luaL_checktype(L, 1, LUA_TTABLE);
        pretty = lua_toboolean(L, 2);
    }


    // loop the table to create the JSON object
    json = table_is_array(L, 1) ? cJSON_CreateArray() : cJSON_CreateObject();
    table_to_json(json, L, 1);

    /*
     * push the JSON string to the stack
     * and delete the super JSON object
    */
    json_str = (pretty == 1) ? cJSON_Print(json) : cJSON_PrintUnformatted(json);
    if ( json_str == NULL ) {
        lua_pushnil(L);
        lua_pushboolean(L, 0);
    } else {
        lua_pushstring(L, json_str);
        lua_pushboolean(L, 1);
        free(json_str);
    }

    cJSON_Delete(json);
    return 2;
}

static int lua_json_decode(lua_State *L)
{
    const char *json_str  = NULL;
    cJSON *json = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "json string expected");
    json_str = luaL_checkstring(L, 1);

    json = cJSON_Parse(json_str);

    /* Fail to parse the json */
    if ( json == NULL ) {
        lua_pushstring(L, cJSON_GetErrorPtr());
        lua_pushboolean(L, 0);
        return 2;
    }

    lua_newtable(L);
    if ( json->type == cJSON_Array ) {
        cjson_to_table(json->child, cJSON_Array, L);
    } else {
        cjson_to_table(json->child, cJSON_Object, L);
    }

    lua_pushboolean(L, 1);
    cJSON_Delete(json);

    return 2;
}




/** +- LUA HTTP function implementation and register */

static size_t __http_recv_callback(void *buffer, size_t size, size_t nmemb, void *ptr)
{
    luaL_Buffer *ret_buffer = (luaL_Buffer *) ptr;
    luaL_addlstring(ret_buffer, buffer, size * nmemb);
    return size * nmemb;
}

/**
 * http get request with basic lua syntax define:
 * string, boolean http_get("api url", header_map)
*/
static int lua_http_get(lua_State *L)
{
    char *url = NULL;
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    int state_top, errno;
    long http_code;
    luaL_Buffer ret_buffer;

    state_top = lua_gettop(L);
    if ( state_top == 1 ) {
        // luaL_argcheck(L, state_top == 1, 1, "url expected");
        url = (char *) luaL_checkstring(L, 1);
    } else {
        luaL_argcheck(L, state_top == 2, 2, "url and header table expected");
        url = (char *) luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);
    }

    errno = 0;
    curl = curl_easy_init();
    if ( curl == NULL ) {
        errno = 1;
        goto failed;
    }

    // Initialize the lua string buffer
    luaL_buffinit(L, &ret_buffer);

    // check and initialize the http header
    if ( state_top > 1 ) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            // k_type = lua_type(L, -2);
            // if ( k_type == LUA_TSTRING ) {
            headers = curl_slist_append(headers, lua_tostring(L, -1));
            // }
            lua_pop(L, 1);
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    if ( headers != NULL ) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);          // Follow http redirect
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);               // max redirect times
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 30000);          // 30 seconds
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);    // 1 hour cache for DNS
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __http_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret_buffer);

    // Perform the http request
    if ( curl_easy_perform(curl) != CURLE_OK ) {
        errno = 2;
        goto failed;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if ( http_code != 200 ) {
        errno = 2;
        goto failed;
    }

failed:
    if ( headers != NULL ) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if ( errno == 0 || errno == 2 ) {
        luaL_pushresult(&ret_buffer);
    } else {
        lua_pushnil(L);
    }

    lua_pushboolean(L, errno > 0 ? 0 : 1);

    return 2;
}

/**
 * http post request with basic lua syntax define:
 * string, boolean http_post("api_url", args_map, header_map)
*/
static int lua_http_post(lua_State *L)
{
    char *url = NULL, *key = NULL, *val = NULL;
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    cel_strbuff_t *strbuff = NULL;
    int state_top, errno, k_type, v_type, bval;
    long http_code;
    luaL_Buffer ret_buffer;

    state_top = lua_gettop(L);
    if ( state_top == 1 ) {
        // luaL_argcheck(L, state_top == 1, 1, "url expected");
        url = (char *) luaL_checkstring(L, 1);
    } else if ( state_top == 2 ) {
        // luaL_argcheck(L, state_top == 2, 2, "url and header table expected");
        url = (char *) luaL_checkstring(L, 1);  // args table
        luaL_checktype(L, 2, LUA_TTABLE);
    } else {
        luaL_argcheck(L, state_top == 3, 3, "url, args table and header table expected");
        url = (char *) luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);       // args table
        luaL_checktype(L, 2, LUA_TTABLE);       // header table
    }


    errno = 0;
    curl = curl_easy_init();
    if ( curl == NULL ) {
        errno = 1;
        goto failed;
    }

    // Initialize the lua string buffer
    luaL_buffinit(L, &ret_buffer);

    // check the arguments and build the std post fields
    strbuff = new_cel_strbuff_opacity(16);
    if ( state_top > 1 ) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            key = NULL;
            val = NULL;

            k_type = lua_type(L, -2);
            v_type = lua_type(L, -1);

            switch ( k_type ) {
            case LUA_TNUMBER:
                key = (char *) lua_pushfstring(L, "%I", lua_tointeger(L, -2));
                lua_pop(L, 1);
                break;
            case LUA_TSTRING:
                key = (char *) lua_tostring(L, -2);
                break;
            }

            switch ( v_type ) {
            case LUA_TNUMBER:
                if ( lua_isinteger(L, -1) ) {
                    val = (char *) lua_pushfstring(L, "%I", lua_tointeger(L, -1));
                } else {
                    val = (char *) lua_pushfstring(L, "%f", lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
                break;
            case LUA_TBOOLEAN:
                bval = lua_toboolean(L, -1);
                val = bval == 1 ? "true" : "false";
                break;
            case LUA_TSTRING:
                val = (char *) lua_tostring(L, -1);
                break;
            }

            if ( strbuff->size > 0 ) {
                cel_strbuff_append_char(strbuff, '&', 1);
            }

            // push the key=value arguments pair
            cel_strbuff_append(strbuff, key, 1);
            cel_strbuff_append_char(strbuff, '=', 1);
            cel_strbuff_append(strbuff, val, 1);

            lua_pop(L, 1);
        }
    }

    // check and initialize the http header
    if ( state_top > 2 ) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            k_type = lua_type(L, -2);
            // if ( k_type == LUA_TSTRING ) {
            headers = curl_slist_append(headers, lua_tostring(L, -1));
            // }
            lua_pop(L, 1);
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    if ( headers != NULL ) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(curl, CURLOPT_POST, 1);
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);          // Follow http redirect
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);               // max redirect times
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 30000);          // 30 seconds
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);    // 1 hour cache for DNS
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strbuff->buffer);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __http_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret_buffer);

    // Perform the http request
    if ( curl_easy_perform(curl) != CURLE_OK ) {
        errno = 2;
        goto failed;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if ( http_code != 200 ) {
        errno = 2;
        goto failed;
    }

failed:
    if ( strbuff != NULL ) {
        free_cel_strbuff(&strbuff);
    }

    if ( headers != NULL ) {
        curl_slist_free_all(headers);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if ( errno == 0 || errno == 2 ) {
        luaL_pushresult(&ret_buffer);
    } else {
        lua_pushnil(L);
    }

    lua_pushboolean(L, errno > 0 ? 0 : 1);

    return 2;
}


/*
 * lua function download a specified resource to a local file
 *  througth http protocol.
*/
static int lua_http_download(lua_State *L)
{
    char *url, *file;
    int override, args, errno;

    args = lua_gettop(L);
    if ( args < 2 ) {
        luaL_argcheck(L, lua_gettop(L) == 2, 2, "url and local file path expected");
    }

    url  = (char *) luaL_checkstring(L, 1);
    file = (char *) luaL_checkstring(L, 2);
    if ( args > 2 ) {
        override = lua_toboolean(L, 3);
    } else {
        override = 0;   /* Default not to override anything */
    }

    /* Push the final result status */
    errno = sarah_http_download(url, file, override);
    lua_pushboolean(L, errno == 0 ? 1 : 0);
    lua_pushinteger(L, errno);

    return 2;
}


/**
 * quick interface to get the content of a specified fil
 * string, boolean file_get_contents(string file_path)
 * @author Rock
*/
static int lua_file_get_contents(lua_State *L)
{
    FILE *fp;
    char *file_path = NULL, buf[1024] = {'\0'};
    int _lock = -1;
    luaL_Buffer ret_buffer;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file path expected");
    file_path = (char *)luaL_checkstring(L, 1);

    // fail to open the file
    if ( NULL == (fp = fopen(file_path, "rb")) ) {
        goto failed;
    }

    if ( (_lock = flock(fp->_fileno, LOCK_SH)) == -1 ) {
        goto failed;
    }

    luaL_buffinit(L, &ret_buffer);
    while ( 1 ) {
        if ( fread(buf, 1, sizeof(buf)-1, fp) < 1 ) {
            break;
        }

        luaL_addstring(&ret_buffer, buf);
    }

    flock(fp->_fileno, LOCK_UN);
    fclose(fp);

    luaL_pushresult(&ret_buffer);
    return 1;

failed:
    if ( fp != NULL ) {
        if ( _lock > -1 ) {
            flock(fp->_fileno, LOCK_UN);
        }

        fclose(fp);
        fp = NULL;
    }

    lua_pushnil(L);
    return 1;
}


#define SARAH_FILE_RESET    (0x01 << 0)
#define SARAH_FILE_APPEND   (0x01 << 1)
#define SARAH_FILE_LOCK_EX  (0x01 << 2)
#define SARAH_FILE_LOCK_SH  (0x01 << 3)

/**
 * quick interface to put the content into a specified file
 *
 * boolean, errno = file_put_contents(string file_path, string file_content, int flag)
*/
static int lua_file_put_contents(lua_State *L)
{
    FILE *fp;
    char *file_path = NULL, *data = NULL;
    int args_num, _type, errno = 0, _lock = -1;
    int flag = SARAH_FILE_RESET | SARAH_FILE_LOCK_SH;

    args_num = lua_gettop(L);
    luaL_argcheck(L, args_num >= 2, 2, "file path and content expected");
    file_path = (char *) luaL_checkstring(L, 1);
    if ( args_num > 2 ) {
        flag = luaL_checkinteger(L, 3);
    }

    /* check the data type and to the basic auto convertion */
    _type = lua_type(L, 2);
    switch ( _type ) {
    case LUA_TNUMBER:
        if ( lua_isinteger(L, 2) ) {
            data = (char *) lua_pushfstring(L, "%I", lua_tointeger(L, 2));
        } else {
            data = (char *) lua_pushfstring(L, "%f", lua_tonumber(L, -1));
        }
        lua_pop(L, 1);
        break;
    case LUA_TBOOLEAN:
        data = lua_toboolean(L, 2) == 1 ? "true" : "false";
        break;
    case LUA_TSTRING:
        data = (char *) lua_tostring(L, 2);
        break;
    default:
        lua_pushliteral(L, "Cannot convert the table to string");
        lua_error(L);
        break;
    }

    // check and open the file
    fp = fopen(file_path, (flag & SARAH_FILE_APPEND) != 0 ? "a+b" : "wb");
    if ( NULL == fp ) {
        errno = 1;
        goto failed;
    }

    // check if need to lock the file
    if ( (flag & SARAH_FILE_LOCK_EX) > 0 ) {
        if ( (_lock = flock(fp->_fileno, LOCK_EX)) == -1 ) {
            errno = 2;
            goto failed;
        }
    } else if ( (flag & SARAH_FILE_LOCK_SH) > 0 ) {
        if ( (_lock = flock(fp->_fileno, LOCK_SH)) == -1 ) {
            errno = 2;
            goto failed;
        }
    }

    // Write data to file
    if ( 1 != fwrite(data, strlen(data), 1, fp) ) {
        errno = 3;
        goto failed;
    }


failed:
    if ( NULL != fp ) {
        if ( _lock > -1 ) {
            flock(fp->_fileno, LOCK_UN);
        }

        fclose(fp);
        fp = NULL;
    }

    lua_pushboolean(L, errno > 0 ? 0 : 1);
    lua_pushinteger(L, errno);
    return 2;
}

/**
 * quick interface to check if the file is exists
 * boolean file_exists(string file_path)
 * @author Rock
 *
*/
static int lua_file_exists(lua_State *L)
{
    char *file_path = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file path expected");
    file_path = (char *)luaL_checkstring(L, 1);

    if ( -1 != access(file_path, F_OK) ) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
    }

    return 1;
}

/**
 * make the specified directory path
 *
 * @param   path
 * @param   mode_mask
 * @return  bool
*/
static int lua_mkdir(lua_State *L)
{
    char *path = NULL;
    int top, errno, mask = 0755;

    top = lua_gettop(L);
    if ( top < 1 ) {
        luaL_argcheck(L, 0, 1, "as least dir path expected");
    } else if ( top == 1 ) {
        path = (char *) luaL_checkstring(L, 1);
    } else {
        path = (char *) luaL_checkstring(L, 1);
        mask = luaL_checkinteger(L, 2);
    }

    errno = sarah_mkdir(path, mask);
    lua_pushboolean(L, errno == 0 ? 1 : 0);

    return 1;
}

/**
 * change file mode
 *
 * @param   file
 * @param   mode_mask
 * @return  bool
*/
static int lua_chmod(lua_State *L)
{
    char *file_path = NULL;
    int errno, mode_mask;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "File path and mode mask expected");
    file_path = (char *) luaL_checkstring(L, 1);
    mode_mask = luaL_checkinteger(L, 2);

    errno = chmod(file_path, mode_mask);
    lua_pushboolean(L, errno == 0 ? 1 : 0);

    return 1;
}

/**
 * posix kill function for lua
*/
static int lua_kill(lua_State *L)
{
    int pid, signal;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "process id and signal expected");
    pid = luaL_checkinteger(L, 1);
    signal = luaL_checkinteger(L, 2);

    lua_pushboolean(L, kill(pid, signal) == 0 ? 1 : 0);

    return 1;
}

/**
 * fork for lua
*/
static int lua_fork(lua_State *L)
{
    lua_pushinteger(L, fork());
    return 1;
}

/**
 * wrapper for waitpid
 * usage: waitpid(pid, options);
 * pid >  0: wait only for the specified process with id = pid
 * pid = -1: wait for any of a child process
 * pid =  0: wait only for the process with the same group
 * pid < -1: wait only for the process with the same group and its Id = abs(pid)
*/
static int lua_waitpid(lua_State *L)
{
    int pid, r_pid;
    int status, options;
    
    luaL_argcheck(L, lua_gettop(L) == 2, 2, "pid and options expected");
    pid = luaL_checkinteger(L, 1);
    options = luaL_checkinteger(L, 2);

    r_pid = waitpid(pid, &status, options);
    lua_pushinteger(L, r_pid);
    lua_pushinteger(L, status);

    return 2;
}

/** 
 * exec functions for lua
 *
 * IF the exec function execute failed and this function 
 * will return or it will not return.
*/
static int lua_exec(lua_State *L)
{
    int args, i;
    char *path, **argv, *val;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "binary path and argv expected");
    path = (char *) luaL_checkstring(L, 1);
    args = lua_rawlen(L, 2);
    argv = (char **) sarah_calloc(args + 2, sizeof(char *));
    if ( argv == NULL ) {
        lua_pushboolean(L, 0);
        return 1;
    }

    argv[0] = path;
    i = 1;
    lua_pushnil(L);  /* first key */
    while ( lua_next(L, 2) != 0 ) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        val = NULL;
        switch ( lua_type(L, -1) ) {
        case LUA_TNUMBER:
            if ( lua_isinteger(L, -1) ) {
                val = (char *) lua_pushfstring(L, "%I", lua_tointeger(L, -1));
            } else {
                val = (char *) lua_pushfstring(L, "%f", lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            break;
        case LUA_TBOOLEAN:
            val = lua_toboolean(L, -1) == 1 ? "true" : "false";
            break;
        case LUA_TSTRING:
            val = (char *) lua_tostring(L, -1);
            break;
        }

        if ( val != NULL ) {
            argv[i++] = val;
        }

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }

    // make sure it end with NULL
    argv[args + 1] = NULL;

    // try to do the exec by invoking execv 
    execv(path, argv);

    // Well, this block will be invoked only if failed
    sarah_free(argv);
    lua_pushboolean(L, 0);
    return 1;
}

/**
 * wrapper for getpid
*/
static int lua_getpid(lua_State *L)
{
    lua_pushinteger(L, getpid());
    return 1;
}

/**
 * wrapper for getppid
*/
static int lua_getppid(lua_State *L)
{
    lua_pushinteger(L, getppid());
    return 1;
}

/**
 * wrapper for getuid
*/
static int lua_getuid(lua_State *L)
{
    lua_pushinteger(L, getuid());
    return 1;
}

/**
 * wrapper for setuid
*/
static int lua_setuid(lua_State *L)
{
    uid_t uid;
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "valid uid expected");
    uid = luaL_checkinteger(L, 1);

    lua_pushinteger(L, setuid(uid) == 0 ? 1 : 0);
    return 1;
}

/**
 * wrapper for geteuid
*/
static int lua_geteuid(lua_State *L)
{
    lua_pushinteger(L, geteuid());
    return 1;
}

/**
 * wrapper for seteuid
*/
static int lua_seteuid(lua_State *L)
{
    uid_t uid;
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "valid uid expected");
    uid = luaL_checkinteger(L, 1);

    lua_pushinteger(L, seteuid(uid) == 0 ? 1 : 0);
    return 1;
}

/**
 * wrapper for getgid
*/
static int lua_getgid(lua_State *L)
{
    lua_pushinteger(L, getgid());
    return 1;
}

/**
 * wrapper for setgid
*/
static int lua_setgid(lua_State *L)
{
    gid_t gid;
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "valid gid expected");
    gid = luaL_checkinteger(L, 1);

    lua_pushinteger(L, setgid(gid) == 0 ? 1 : 0);
    return 1;
}

/**
 * wrapper for getegid
*/
static int lua_getegid(lua_State *L)
{
    lua_pushinteger(L, getegid());
    return 1;
}

/**
 * wrapper for setegid
*/
static int lua_setegid(lua_State *L)
{
    gid_t gid;
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "valid gid expected");
    gid = luaL_checkinteger(L, 1);

    lua_pushinteger(L, setegid(gid) == 0 ? 1 : 0);
    return 1;
}

/* wrapper for getenv */
static int lua_getenv(lua_State *L)
{
    char *envvar = NULL, *envval = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "envvar expected");
    envvar = (char *)luaL_checkstring(L, 1);

    if( (envval = getenv(envvar)) == NULL ) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, envval);
    }

    return 1;
}

/* wrapper for setenv */
static int lua_setenv(lua_State *L)
{
    char *envvar = NULL, *envval = NULL;
    int override = 0, args_num = lua_gettop(L);

    luaL_argcheck(L, args_num >= 2, 2, "envvar envval expected");
    envvar = (char *)luaL_checkstring(L, 1);
    envval = (char *)luaL_checkstring(L, 2);
    if ( args_num > 2 ) {
        override = luaL_checkinteger(L, 3);
    }

    lua_pushboolean(L, setenv(envvar, envval, override) == 0 ? 1 : 0);
    return 1;
}

/**
 * std print function to instead of lua's print
*/
static int lua_print(lua_State *L)
{
    int i, t, argc = lua_gettop(L);
    char *_ptr = NULL, _str[41] = {'\0'};
    cJSON *json = NULL;

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
        printf("%s", _ptr == NULL ? _str : _ptr);
        if ( t == LUA_TTABLE ) {
            free(_ptr);
        }
    }

    return 0;
}

/*
 * sleep function
 * usage: sleep(seconds)
*/
static int lua_sleep(lua_State *L)
{
    int sec;
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "seconds to sleep");
    sec = (int) luaL_checkinteger(L, 1);
    sleep(sec);
    return 0;
}

static int lua_time(lua_State *L)
{
    struct timeval tv_s;
    gettimeofday(&tv_s, NULL);
    lua_pushinteger(L, tv_s.tv_sec);
    return 1;
}

static int lua_md5(lua_State *L)
{
    char *str;
    cel_md5_t md5_context;
    unsigned char digest[16] = {'\0'};
    char md5_str[33] = {'\0'};

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string to md5 expected");
    str = (char *)luaL_checkstring(L, 1);

    cel_md5_init(&md5_context);
    cel_md5_update(&md5_context, (uchar_t *)str, strlen(str));
    cel_md5_final(&md5_context, digest);
    cel_md5_print(digest, md5_str);
    lua_pushstring(L, md5_str);

    return 1;
}

static int lua_md5_file(lua_State *L)
{
    int errno;
    char *path;
    unsigned char digest[16] = {'\0'};
    char md5_str[33] = {'\0'};
    
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file path to md5 expected");
    path = (char *)luaL_checkstring(L, 1);

    errno = cel_md5_file(path, digest);
    lua_pushstring(L, md5_str);
    if ( errno == 0 ) {
        cel_md5_print(digest, md5_str);
        lua_pushstring(L, md5_str);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int lua_ltrim(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string to ltrim expected");
    str = (char *)luaL_checkstring(L, 1);

    cel_left_trim(str);
    lua_pushstring(L, str);

    return 1;
}

static int lua_rtrim(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string to rtrim expected");
    str = (char *)luaL_checkstring(L, 1);

    cel_right_trim(str);
    lua_pushstring(L, str);

    return 1;
}

static int lua_trim(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string to trim expected");
    str = (char *)luaL_checkstring(L, 1);

    cel_right_trim(str);
    cel_left_trim(str);
    lua_pushstring(L, str);

    return 1;
}

static int lua_strlen(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *)luaL_checkstring(L, 1);

    lua_pushinteger(L, strlen(str));
    return 1;
}

static int lua_strtolower(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *)luaL_checkstring(L, 1);

    cel_strtolower(str, strlen(str));
    lua_pushstring(L, str);
    return 1;
}

static int lua_strtoupper(lua_State *L)
{
    char *str;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string expected");
    str = (char *)luaL_checkstring(L, 1);

    cel_strtoupper(str, strlen(str));
    lua_pushstring(L, str);
    return 1;
}

static int lua_strcmp(lua_State *L)
{
    char *str1, *str2;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "str1 and str2 expected");
    str1 = (char *)luaL_checkstring(L, 1);
    str2 = (char *)luaL_checkstring(L, 2);

    lua_pushinteger(L, strcmp(str1, str2));
    return 1;
}

static int lua_strncmp(lua_State *L)
{
    char *str1, *str2;
    int len;

    luaL_argcheck(L, lua_gettop(L) == 3, 3, "str1, str2 and length expected");
    str1 = (char *)luaL_checkstring(L, 1);
    str2 = (char *)luaL_checkstring(L, 2);
    len  = luaL_checkinteger(L, 3);

    lua_pushinteger(L, strncmp(str1, str2, len));
    return 1;
}


/**
 * @Note: all the operation of the string position
 * will be the same like lua, it starts from 1.
*/
static int lua_charat(lua_State *L)
{
    char *str;
    int idx;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "str and index expected");
    str = (char *)luaL_checkstring(L, 1);
    idx = luaL_checkinteger(L, 2);

    if ( idx < 1 || idx > strlen(str) ) {
        lua_pushnil(L);
    } else {
        lua_pushfstring(L, "%c", str[idx - 1]);
    }

    return 1;
}

static int lua_strchr(lua_State *L)
{
    char *str, *chr, *pos;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "str and char expected");
    str = (char *)luaL_checkstring(L, 1);
    chr = (char *)luaL_checkstring(L, 2);
    
    if ( (pos = strchr(str, chr[0])) == NULL ) {
        lua_pushinteger(L, -1);
    } else {
        lua_pushinteger(L, pos - str + 1);
    }

    return 1;
}

static int lua_strstr(lua_State *L)
{
    char *str, *fnd, *pos;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "str and str expected");
    str = (char *)luaL_checkstring(L, 1);
    fnd = (char *)luaL_checkstring(L, 2);
    
    if ( (pos = strstr(str, fnd)) == NULL ) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, pos);
    }

    return 1;
}

static int lua_strpos(lua_State *L)
{
    char *str, *fnd, *pos;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "str and str expected");
    str = (char *)luaL_checkstring(L, 1);
    fnd = (char *)luaL_checkstring(L, 2);
    
    if ( (pos = strstr(str, fnd)) == NULL ) {
        lua_pushinteger(L, -1);
    } else {
        lua_pushinteger(L, pos - str + 1);
    }

    return 1;
}

static int lua_sha1(lua_State *L)
{
    int i;
    char *str, buffer[41] = {'\0'};
    unsigned char digest[20];

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "str to hash expected");
    str = (char *)luaL_checkstring(L, 1);
    if ( mbedtls_sha1_ret((unsigned char *)str, strlen(str), digest) != 0 ) {
        lua_pushnil(L);
    } else {
        for ( i = 0; i < 20; i++ ) {
            sprintf(buffer + i * 2, "%02x", digest[i]);
        }

        lua_pushstring(L, buffer);
    }

    return 1;
}

static int lua_sha256(lua_State *L)
{
    int i;
    char *str, buffer[65] = {'\0'};
    unsigned char digest[32];

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "str to hash expected");
    str = (char *)luaL_checkstring(L, 1);
    if ( mbedtls_sha256_ret((unsigned char *)str, strlen(str), digest, 0) != 0 ) {
        lua_pushnil(L);
    } else {
        for ( i = 0; i < 32; i++ ) {
            sprintf(buffer + i * 2, "%02x", digest[i]);
        }

        lua_pushstring(L, buffer);
    }

    return 1;
}

static int lua_sha512(lua_State *L)
{
    int i;
    char *str, buffer[129] = {'\0'};
    unsigned char digest[64];

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "str to hash expected");
    str = (char *)luaL_checkstring(L, 1);
    if ( mbedtls_sha512_ret((unsigned char *)str, strlen(str), digest, 0) != 0 ) {
        lua_pushnil(L);
    } else {
        for ( i = 0; i < 64; i++ ) {
            sprintf(buffer + i * 2, "%02x", digest[i]);
        }

        lua_pushstring(L, buffer);
    }

    return 1;
}

/*
 * string base64 encode
 * usage: string base64_encode(string)
*/
static int lua_base64_encode(lua_State *L)
{
    char *dst, *str;
    size_t olen, slen;

    luaL_argcheck(L, lua_gettop(L) == 1, 1,  "string expected");
    str = (char *) luaL_checkstring(L, 1);
    slen = strlen(str);

    /* get the data that would have been written. */
    mbedtls_base64_encode(NULL, 0, &olen, (unsigned char *) str, slen);
    if ( (dst = (char *) sarah_malloc(olen + 1)) == NULL ) {
        lua_pushnil(L);
        return 1;
    }

    dst[olen] = '\0';
    if ( mbedtls_base64_encode( (unsigned char *) dst, 
        olen, &olen, (unsigned char *) str, slen) == 0 ) {
        lua_pushstring(L, dst);
    } else {
        lua_pushnil(L);
    }

    sarah_free(dst);
    return 1;
}

/**
 * string base64 decode
 * usage: string base64_decode(string)
*/
static int lua_base64_decode(lua_State *L)
{
    char *dst, *str;
    size_t olen, slen;

    luaL_argcheck(L, lua_gettop(L) == 1, 1,  "string expected");
    str = (char *) luaL_checkstring(L, 1);
    slen = strlen(str);

    /* get the data that would have been written. */
    mbedtls_base64_decode(NULL, 0, &olen, (unsigned char *) str, slen);
    if ( (dst = (char *) sarah_malloc(olen + 1)) == NULL ) {
        lua_pushnil(L);
        return 1;
    }

    dst[olen] = '\0';
    if ( mbedtls_base64_decode((unsigned char *) dst, 
        olen, &olen, (unsigned char *) str, slen) == 0 ) {
        lua_pushstring(L, dst);
    } else {
        lua_pushnil(L);
    }

    sarah_free(dst);
    return 1;
}


static int lua_basename(lua_State *L)
{
    char *path, *_name = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string path expected");
    path = (char *)luaL_checkstring(L, 1);

    _name = basename(path);
    if ( _name == NULL ) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, _name);
    }

    return 1;
}

static int lua_dirname(lua_State *L)
{
    char *path, *_dir = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "string path expected");
    path = (char *)luaL_checkstring(L, 1);

    _dir = dirname(path);
    if ( _dir == NULL ) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, _dir);
    }

    return 1;
}

/**
 * struct statvfs {
 *  unsigned long  f_bsize;    // file system block size
 *  unsigned long  f_frsize;   // fragment size
 *  fsblkcnt_t     f_blocks;   // size of fs in f_frsize units
 *  fsblkcnt_t     f_bfree;    // free blocks
 *  fsblkcnt_t     f_bavail;   // free blocks for unprivileged users
 *  fsfilcnt_t     f_files;    // inodes
 *  fsfilcnt_t     f_ffree;    // free inodes
 *  fsfilcnt_t     f_favail;   // free inodes for unprivileged users
 *  unsigned long  f_fsid;     // file system ID
 *  unsigned long  f_flag;     // mount flags
 *  unsigned long  f_namemax;  // maximum filename length
 *};
*/
static int lua_statvfs(lua_State *L)
{
    char *path;
    struct statvfs dir_stat;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "dir path to statvfs expected");
    path = (char *)luaL_checkstring(L, 1);

    if ( statvfs(path, &dir_stat) < 0 ) {
        lua_pushnil(L);
    } else {
        lua_newtable(L);
        lua_pushinteger(L, dir_stat.f_bsize);
        lua_setfield(L, -2, "f_bsize");
        lua_pushinteger(L, dir_stat.f_frsize);
        lua_setfield(L, -2, "f_frsize");
        lua_pushinteger(L, dir_stat.f_blocks);
        lua_setfield(L, -2, "f_blocks");
        lua_pushinteger(L, dir_stat.f_bfree);
        lua_setfield(L, -2, "f_bfree");
        lua_pushinteger(L, dir_stat.f_bavail);
        lua_setfield(L, -2, "f_bavail");
        lua_pushinteger(L, dir_stat.f_files);
        lua_setfield(L, -2, "f_files");
        lua_pushinteger(L, dir_stat.f_ffree);
        lua_setfield(L, -2, "f_ffree");
        lua_pushinteger(L, dir_stat.f_favail);
        lua_setfield(L, -2, "f_favail");
        lua_pushinteger(L, dir_stat.f_fsid);
        lua_setfield(L, -2, "f_fsid");
        lua_pushinteger(L, dir_stat.f_flag);
        lua_setfield(L, -2, "f_flag");
        lua_pushinteger(L, dir_stat.f_namemax);
        lua_setfield(L, -2, "f_namemax");
    }

    return 1;
}

/*
 * struct stat 
 * { 
 *    dev_t     st_dev;     // 文件所在设备的标识
 *    ino_t     st_ino;     // 文件结点号
 *    mode_t    st_mode;    // 文件保护模式
 *    nlink_t   st_nlink;   // 硬连接数
 *    uid_t     st_uid;     // 文件用户标识
 *    gid_t     st_gid;     // 文件用户组标识
 *    dev_t     st_rdev;    // 文件所表示的特殊设备文件的设备标识
 *    off_t     st_size;    // 总大小，字节为单位
 *    blksize_t st_blksize; // 文件系统的块大小
 *    blkcnt_t  st_blocks;  // 分配给文件的块的数量，512字节为单元
 *    time_t    st_atime;   // 最后访问时间
 *    time_t    st_mtime;   // 最后修改时间
 *    time_t    st_ctime;   // 最后状态改变时间
 * };
*/
static int lua_stat(lua_State *L)
{
    char *path;
    struct stat file_stat;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "dir path to statvfs expected");
    path = (char *)luaL_checkstring(L, 1);

    if ( stat(path, &file_stat) < 0 ) {
        lua_pushnil(L);
    } else {
        lua_newtable(L);
        lua_pushinteger(L, file_stat.st_dev);
        lua_setfield(L, -2, "st_dev");
        lua_pushinteger(L, file_stat.st_ino);
        lua_setfield(L, -2, "st_ino");
        lua_pushinteger(L, file_stat.st_mode);
        lua_setfield(L, -2, "st_mode");
        lua_pushinteger(L, file_stat.st_nlink);
        lua_setfield(L, -2, "st_nlink");
        lua_pushinteger(L, file_stat.st_uid);
        lua_setfield(L, -2, "st_uid");
        lua_pushinteger(L, file_stat.st_gid);
        lua_setfield(L, -2, "st_gid");
        lua_pushinteger(L, file_stat.st_rdev);
        lua_setfield(L, -2, "st_rdev");
        lua_pushinteger(L, file_stat.st_size);
        lua_setfield(L, -2, "st_size");
        lua_pushinteger(L, file_stat.st_blksize);
        lua_setfield(L, -2, "st_blksize");
        lua_pushinteger(L, file_stat.st_blocks);
        lua_setfield(L, -2, "st_blocks");
        lua_pushinteger(L, file_stat.st_atime);
        lua_setfield(L, -2, "st_atime");
        lua_pushinteger(L, file_stat.st_mtime);
        lua_setfield(L, -2, "st_mtime");
        lua_pushinteger(L, file_stat.st_ctime);
        lua_setfield(L, -2, "st_ctime");
    }

    return 1;
}

static int lua_lstat(lua_State *L)
{
    char *path;
    struct stat file_stat;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "dir path to statvfs expected");
    path = (char *)luaL_checkstring(L, 1);

    if ( stat(path, &file_stat) < 0 ) {
        lua_pushnil(L);
    } else {
        lua_newtable(L);
        lua_pushinteger(L, file_stat.st_dev);
        lua_setfield(L, -2, "st_dev");
        lua_pushinteger(L, file_stat.st_ino);
        lua_setfield(L, -2, "st_ino");
        lua_pushinteger(L, file_stat.st_mode);
        lua_setfield(L, -2, "st_mode");
        lua_pushinteger(L, file_stat.st_nlink);
        lua_setfield(L, -2, "st_nlink");
        lua_pushinteger(L, file_stat.st_uid);
        lua_setfield(L, -2, "st_uid");
        lua_pushinteger(L, file_stat.st_gid);
        lua_setfield(L, -2, "st_gid");
        lua_pushinteger(L, file_stat.st_rdev);
        lua_setfield(L, -2, "st_rdev");
        lua_pushinteger(L, file_stat.st_size);
        lua_setfield(L, -2, "st_size");
        lua_pushinteger(L, file_stat.st_blksize);
        lua_setfield(L, -2, "st_blksize");
        lua_pushinteger(L, file_stat.st_blocks);
        lua_setfield(L, -2, "st_blocks");
        lua_pushinteger(L, file_stat.st_atime);
        lua_setfield(L, -2, "st_atime");
        lua_pushinteger(L, file_stat.st_mtime);
        lua_setfield(L, -2, "st_mtime");
        lua_pushinteger(L, file_stat.st_ctime);
        lua_setfield(L, -2, "st_ctime");
    }

    return 1;
}

/**
 * lua package import function
 * 1, With remote pacakge download support
 * 2, local cache support
*/
static int lua_import(lua_State *L)
{
    int i, len, errno;
    char *modname = NULL, modurl[256] = {'\0'}, modfile[256] = {'\0'};

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "as least one package module expected");
    modname = (char *)luaL_checkstring(L, 1);

    /* Basic arguments checking */
    len = strlen(modname);
    if ( len >= 256 ) {
        lua_pushstring(L, "the length of the modname path should be less than 256");
        errno = 1;
        goto failed;
    }

    /* Pre-process the module name */
    // memcpy(modcopy, modname, len);
    for ( i = 0; i < len; i++ ) {
        if ( modname[i] == '.' ) {
            modname[i] = '_';
        }
    }

    /* 
     * Check the local cache of the package.
     * IF it were not cached try to download it and cache it locally.
    */
    snprintf(
        modfile, 
        sizeof(modfile) - 1, 
        "%s__loader/%s_init.lua", 
        APRIL_PACKAGE_BASE_DIR, modname
    );
    if ( access(modfile, F_OK) == -1 ) {
        memcpy(modurl, modfile, strlen(modfile));
        if ( sarah_mkdir(dirname(modurl), 0755) > 0 ) {
            lua_pushfstring(L, "Fail to make the base directory");
            errno = 2;
            goto failed;
        }

        memset(modurl, 0x00, sizeof(modurl));
        sprintf(modurl, "%s__loader/%s_init.lua", APRIL_PACKAGE_BASE_URL, modname);
        if ( sarah_http_download(modurl, modfile, 1) > 0 ) {
            lua_pushfstring(L, "Fail to load the module '%s' ", modname);
            errno = 3;

            /*
             * In this case the download init file should be unavailable .
             * remove it we must.
            */
            if ( remove(modfile) == -1) {
                // @TODO: do error log here maybe
            }

            goto failed;
        }

        chmod(modfile, 0644);
    }

    /* Load the library source code */
    errno = luaL_loadfile(L, modfile);
    switch ( errno ) {
    case LUA_OK:
        break;
    case LUA_ERRFILE:
    case LUA_ERRSYNTAX:
    case LUA_ERRMEM:
    case LUA_ERRGCMM:
        errno += 10;
        goto failed;
        break;
    }

    errno = lua_pcall(L, 0, LUA_MULTRET, 0);
    switch ( errno ) {
    case LUA_OK:
        break;
    case LUA_ERRRUN:
    case LUA_ERRMEM:
    case LUA_ERRERR:
    case LUA_ERRGCMM:
        errno += 10;
        goto failed;
        break;
    }

failed:
    if ( errno > 0 ) {
        lua_error(L);
        lua_pushnil(L);
    }

    return 1;
}


/* Beginning of file module */

/**
 * open the specified file with specified mode
 *
 * @param   path
 * @param   mode
 * {
 *  "r" : read only, file should be exists
 *  "w" : write only, create new file if not exists, clear the original content
 *  "a" : append only, create new file if not exists
 *  "r+": read & write, file should be exists
 *  "w+": write & r+, create new file and clear the original content
 *  "a+": append & r+, create new file and append content
 *  For write only:
 *  "t" : "Text file",
 *  "b" : "Binary file"
 * }
 * "rb", "wt", "ab", "r+b", "w+t", "a+t"
 * "rb+", "wt+", "ab+"
 * @return  mixed (nil | resource)
*/
static int lua_fopen(lua_State *L)
{
    FILE *fp = NULL;
    char *file_path = NULL, *open_mode = NULL;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "file path and open mode expected");
    file_path = (char *)luaL_checkstring(L, 1);
    open_mode = (char *)luaL_checkstring(L, 2);

    if ( (fp = fopen(file_path, open_mode)) == NULL ) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushlightuserdata(L, fp);
    return 1;
}

static int lua_fclose(lua_State *L)
{
    FILE *fp = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file handler expected");
    fp = (FILE *) lua_touserdata(L, 1);
    if ( fp == NULL || fclose(fp) != 0 ) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_flock(lua_State *L)
{
    FILE *fp = NULL;
    int lock_type;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "file handler expected");
    fp = (FILE *) lua_touserdata(L, 1);
    lock_type = lua_tointeger(L, 2);

    if ( fp == NULL ) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, flock(fp->_fileno, lock_type) == -1 ? 0 : 1);
    return 1;
}

static int lua_fgets(lua_State *L)
{
    FILE *fp = NULL;
    int length = 1024, _free = 0, args_num = lua_gettop(L);
    char *buff, buffer[1024] = {'\0'};

    luaL_argcheck(L, args_num > 0, 1, "file handler expected");
    fp = (FILE *) lua_touserdata(L, 1);
    if ( args_num > 1 ) {
        length = lua_tointeger(L, 2);
    }

    if ( length <= sizeof(buffer) ) {
        buff = buffer;
    } else {
        buff = (char *) sarah_malloc(length);
        memset(buff, 0x00, length);
        if ( buff == NULL ) {
            lua_pushnil(L);
            return 1;
        }

        _free = 1;
    }

    if ( fgets(buff, length, fp) == NULL ) {
        if ( _free == 1 ) {
            sarah_free(buff);
        }

        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, buff);
    if ( _free == 1 ) {
        sarah_free(buff);
        buff = NULL;
    }

    return 1;
}

static int lua_fseek(lua_State *L)
{
    FILE *fp = NULL;
    long int offset;
    int whence = SEEK_SET, args_num = lua_gettop(L);

    luaL_argcheck(L, args_num > 1, 1, "file handler expected");
    fp = (FILE *) lua_touserdata(L, 1);
    offset = lua_tointeger(L, 2);
    if ( args_num > 2 ) {
        whence = lua_tointeger(L, 3);
    }

    lua_pushboolean(L, fseek(fp, offset, whence) == 0 ? 1 : 0);
    return 1;
}

static int lua_ftell(lua_State *L)
{
    FILE *fp = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file handler expected");
    fp = (FILE *) lua_touserdata(L, 1);

    lua_pushinteger(L, ftell(fp));
    return 1;
}

static int lua_rewind(lua_State *L)
{
    FILE *fp = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file handler expected");
    fp = (FILE *) lua_touserdata(L, 1);

    rewind(fp);
    return 0;
}

static int lua_fread(lua_State *L)
{
    FILE *fp = NULL;
    int length = 1024, _free = 0, args_num = lua_gettop(L);
    char *buff, buffer[1024] = {'\0'};

    luaL_argcheck(L, args_num > 0, 1, "file handler expected");
    fp = (FILE *) lua_touserdata(L, 1);
    if ( args_num > 1 ) {
        length = lua_tointeger(L, 2);
    }

    if ( length <= sizeof(buffer) ) {
        buff = buffer;
    } else {
        buff = (char *) sarah_malloc(length);
        memset(buff, 0x00, length);
        if ( buff == NULL ) {
            lua_pushnil(L);
            return 1;
        }

        _free = 1;
    }

    if ( fread(buff, 1, length - 1, fp) < 1 ) {
        if ( _free == 1 ) {
            sarah_free(buff);
        }

        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, buff);
    if ( _free == 1 ) {
        sarah_free(buff);
        buff = NULL;
    }

    return 1;
}

static int lua_fwrite(lua_State *L)
{
    FILE *fp = NULL;
    char *str = NULL;
    int len = 0, dst_len, args_num = lua_gettop(L);

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "file handler and string expected");
    fp = (FILE *) lua_touserdata(L, 1);
    str = (char *) lua_tostring(L, 2);
    len = strlen(str);
    if ( args_num > 2 ) {
        dst_len = lua_tointeger(L, 3);
        len = (dst_len < len) ? dst_len : len;
    }

    if ( fwrite(str, len, 1, fp) == len ) {
        lua_pushinteger(L, len);
    } else {
        lua_pushinteger(L, -1);
    }

    return 1;
}

static int lua_is_dir(lua_State *L)
{
    char *path = NULL;
    struct stat file_stat;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file path expected");
    path = (char *) lua_tostring(L, 1);

    if ( stat(path, &file_stat) < 0 ) {
        lua_pushnil(L);
    } else if ( S_ISDIR(file_stat.st_mode) ) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
    }

    return 1;
}

static int lua_opendir(lua_State *L)
{
    DIR *dirp  = NULL;
    char *path = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "file path expected");
    path = (char *) lua_tostring(L, 1);

    if ( (dirp = opendir(path)) == NULL ) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushlightuserdata(L, dirp);
    return 1;
}

static int lua_readdir(lua_State *L)
{
    DIR *dirp  = NULL;
    struct dirent *direntp = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "dir handler expected");
    dirp = (DIR *) lua_touserdata(L, 1);

    if ( (direntp = readdir(dirp)) == NULL ) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, direntp->d_name);
    }

    return 1;
}

static int lua_closedir(lua_State *L)
{
    DIR *dirp  = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "dir handler expected");
    dirp = (DIR *) lua_touserdata(L, 1);

    lua_pushboolean(L, closedir(dirp) == 0 ? 1 : 0);
    return 0;
}
/* End of file module */




/* Type checking functions */
static int lua_is_nil(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isnil(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_none(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isnone(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_noneornil(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isnoneornil(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_bool(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isboolean(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_string(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isstring(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_number(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isnumber(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_integer(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isinteger(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_table(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_istable(L, 1) ? 1 : 0);
    return 1;
}

/**
 * check if the specified table is an array or not.
 * Array means table but all with numeric index.
*/
static int lua_is_array(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, (lua_istable(L, 1) && table_is_array(L, 1)) ? 1 : 0);
    return 1;
}

static int lua_is_function(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isfunction(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_cfunction(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_iscfunction(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_userdata(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isuserdata(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_thread(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_isthread(L, 1) ? 1 : 0);
    return 1;
}

static int lua_is_lightuserdata(lua_State *L)
{
    luaL_argcheck(L, lua_gettop(L) == 1, 1, "var expected");
    lua_pushboolean(L, lua_islightuserdata(L, 1) ? 1 : 0);
    return 1;
}
/* End of type checking */

/*
 * string key integer value entry
*/
struct lua_skiv_const_entry {
    char *name;
    int value;
};
typedef struct lua_skiv_const_entry lua_skiv_const_t;
static lua_skiv_const_t _internal_skiv_const_array[] = {
    {"FILE_APPEND",  SARAH_FILE_APPEND},
    {"FILE_RESET",   SARAH_FILE_RESET},
    {"FILE_LOCK_EX", SARAH_FILE_LOCK_EX},
    {"FILE_LOCK_SH", SARAH_FILE_LOCK_SH},

    // lock
    {"LOCK_SH", LOCK_SH},
    {"LOCK_EX", LOCK_EX},
    {"LOCK_UN", LOCK_UN},
    {"LOCK_NB", LOCK_NB},

    // seek
    {"SEEK_SET", SEEK_SET},
    {"SEEK_CUR", SEEK_CUR},
    {"SEEK_END", SEEK_END},

    // task status
    {"TASK_STATUS_ERROR",     SARAH_TASK_STATUS_ERR},
    {"TASK_STATUS_READY",     SARAH_TASK_STATUS_READY},
    {"TASK_STATUS_EXECUTING", SARAH_TASK_STATUS_EXECUTING},
    {"TASK_STATUS_COMPLETED", SARAH_TASK_STATUS_COMPLETED},

    // asyn/syn status
    {"WNOHANG",   WNOHANG},
    {"WUNTRACED", WUNTRACED},

    // file type if constants
    {"S_IFBLK", S_IFBLK},
    {"S_IFDIR", S_IFDIR},
    {"S_IFCHR", S_IFCHR},
    {"S_IFIFO", S_IFIFO},
    {"S_IFREG", S_IFREG},
    {"S_IFLNK", S_IFLNK},

    // file mode constants
    {"S_ISUID", S_ISUID},
    {"S_ISGID", S_ISGID},
    {"S_ISVTX", S_ISVTX},

    {"S_IRWXU", S_IRWXU},
    {"S_IXUSR", S_IXUSR},
    {"S_IRUSR", S_IRUSR},
    {"S_IWUSR", S_IWUSR},

    {"S_IRWXG", S_IRWXG},
    {"S_IXGRP", S_IXGRP},
    {"S_IRGRP", S_IRGRP},
    {"S_IWGRP", S_IWGRP},

    {"S_IRWXO", S_IRWXO},
    {"S_IRWXO", S_IXOTH},
    {"S_IROTH", S_IROTH},
    {"S_IWOTH", S_IWOTH},

    // signal
    {"SIGHUP",  SIGHUP },
    {"SIGINT",  SIGINT },
    {"SIGQUIT", SIGQUIT},
    {"SIGILL",  SIGILL },
    {"SIGTRAP", SIGTRAP},
    {"SIGBUS",  SIGBUS },
    {"SIGFPE",  SIGFPE },
    {"SIGKILL", SIGKILL},
    {"SIGUSR1", SIGUSR1},
    {"SIGSEGV", SIGSEGV},
    {"SIGUSR2", SIGUSR2},
    {"SIGPIPE", SIGPIPE},
    {"SIGALRM", SIGALRM},
    {"SIGTERM", SIGTERM},
    {"SIGCHLD", SIGCHLD},
    {"SIGCONT", SIGCONT},
    {"SIGSTOP", SIGSTOP},
    {"SIGTSTP", SIGTSTP},

    // The end make sure it end with this
    {NULL,      0}
};



/*
 * string key and string value entry
*/
struct lua_sksv_const_entry {
    char *name;
    char *value;
};
typedef struct lua_sksv_const_entry lua_sksv_const_t;
static lua_sksv_const_t _internal_sksv_const_array[] = {
    {"_LIB_DIR", APRIL_PACKAGE_BASE_DIR},
    {"_LIB_URL", APRIL_PACKAGE_BASE_URL},
    {"_MIRROR_URL",    APP_MIRROR_BASE_URL},
    {"_SARAH_LIB_DIR", SARAH_LIB_BASE_DIR},
    {NULL, NULL}
};



struct lua_function_entry {
    char *name;
    lua_CFunction value;
};
typedef struct lua_function_entry lua_function_t;
static lua_function_t _internal_function_array[] = {
    // IO functions
    {"print",    lua_print},
    {"import",   lua_import},

    // json function
    {"json_encode", lua_json_encode},
    {"json_decode", lua_json_decode},

    // http function
    {"http_get",  lua_http_get },
    {"http_post", lua_http_post},
    {"http_download", lua_http_download},

    // file handler function
    {"file_put_contents", lua_file_put_contents},
    {"file_get_contents", lua_file_get_contents},
    {"file_exists",       lua_file_exists},
    {"mkdir",    lua_mkdir},
    {"fopen",    lua_fopen},
    {"fclose",   lua_fclose},
    {"flock",    lua_flock},
    {"fgets",    lua_fgets},
    {"fseek",    lua_fseek},
    {"ftell",    lua_ftell},
    {"rewind",   lua_rewind},
    {"fread",    lua_fread},
    {"fwrite",   lua_fwrite},
    {"is_dir",   lua_is_dir},
    {"opendir",  lua_opendir},
    {"readdir",  lua_readdir},
    {"closedir", lua_closedir},

    // process about
    {"chmod",    lua_chmod},
    {"kill",     lua_kill},
    {"fork",     lua_fork},
    {"waitpid",  lua_waitpid},
    {"exec",     lua_exec},
    {"getpid",   lua_getpid},
    {"getppid",  lua_getppid},
    {"getuid",   lua_getuid},
    {"setuid",   lua_setuid},
    {"geteuid",  lua_geteuid},
    {"seteuid",  lua_seteuid},
    {"getgid",   lua_getgid},
    {"setgid",   lua_setgid},
    {"getegid",  lua_getegid},
    {"setegid",  lua_setegid},
    {"setenv",   lua_setenv},
    {"getenv",   lua_getenv},

    // string functions
    {"md5",             lua_md5},
    {"md5_file",        lua_md5_file},
    {"ltrim",           lua_ltrim},
    {"rtrim",           lua_rtrim},
    {"trim",            lua_trim},
    {"strlen",          lua_strlen},
    {"strtolower",      lua_strtolower},
    {"strtoupper",      lua_strtoupper},
    {"strcmp",          lua_strcmp},
    {"strncmp",         lua_strncmp},
    {"charat",          lua_charat},
    {"strchr",          lua_strchr},
    {"strstr",          lua_strstr},
    {"strpos",          lua_strpos},
    {"sha1",            lua_sha1},
    {"sha256",          lua_sha256},
    {"sha512",          lua_sha512},
    {"base64_encode",   lua_base64_encode},
    {"base64_decode",   lua_base64_decode},

    // type checking
    {"is_nil",           lua_is_nil},
    {"is_none",          lua_is_none},
    {"is_noneornil",     lua_is_noneornil},
    {"is_bool",          lua_is_bool},
    {"is_string",        lua_is_string},
    {"is_number",        lua_is_number},
    {"is_integer",       lua_is_integer},
    {"is_table",         lua_is_table},
    {"is_array",         lua_is_array},
    {"is_function",      lua_is_function},
    {"is_cfunction",     lua_is_cfunction},
    {"is_userdata",      lua_is_userdata},
    {"is_thread",        lua_is_thread},
    {"is_lightuserdata", lua_is_lightuserdata},

    // Utils functions
    {"sleep",    lua_sleep},
    {"time",     lua_time},
    {"basename", lua_basename},
    {"dirname",  lua_dirname},
    {"statvfs",  lua_statvfs},
    {"stat",     lua_stat},
    {"lstat",    lua_lstat},

    {NULL, NULL}
};


/**
 * internal method to register all the available libs
 * to the current lua state.
 *
 * @param   executor
 * @param   L
*/
int luaopen_april(lua_State *L)
{
    register int i;
    lua_skiv_const_t *_iconst;
    lua_sksv_const_t *_sconst;
    lua_function_t *_func;

    /* 1, register the global constant */
    for ( i = 0; _internal_skiv_const_array[i].name != NULL; i++ ) {
        _iconst = &_internal_skiv_const_array[i];
        lua_pushinteger(L, _iconst->value);
        lua_setglobal(L, _iconst->name);
    }

    for ( i = 0; _internal_sksv_const_array[i].name != NULL; i++ )  {
        _sconst = &_internal_sksv_const_array[i];
        lua_pushstring(L, _sconst->value);
        lua_setglobal(L, _sconst->name);
    }

    /* 2, Register the functions */
    for ( i = 0; _internal_function_array[i].name != NULL; i++ ) {
        _func = &_internal_function_array[i];
        lua_register(L, _func->name, _func->value);
    }

    return 0;
}
