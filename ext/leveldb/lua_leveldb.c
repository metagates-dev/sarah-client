/**
 * sarah leveldb extension implementation.
 * This implementation is based on LevelDB specified by https://github.com/google/leveldb.
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

#include <stdlib.h>
#include "leveldb/c.h"
#include "april.h"
#include "sarah.h"
#include "sarah_utils.h"

#define L_METATABLE_NAME "_ext_leveldb"
#define L_ITERATOR_NAME "_ext_leveldb_iterator"
#define L_READOPTIONS_NAME "_ext_leveldb_readoptions"
#define L_WRITEOPTIONS_NAME "_ext_leveldb_writeoptions"
#define L_WRITEBATCH_NAME "_ext_leveldb_writebatch"
#define assert_object_state(L, ptr) \
{if (ptr == NULL) {luaL_argerror(L, 1, "Invalid state(Is the object closed?)");}}

typedef struct lua_leveldb_readoptions_entry {
    leveldb_readoptions_t *options;
} lua_leveldb_readoptions_t;

typedef struct lua_leveldb_writeoptions_entry {
    leveldb_writeoptions_t *options;
} lua_leveldb_writeoptions_t;

typedef struct lua_leveldb_entry {
    char *name;
    leveldb_t *db;
    leveldb_options_t *options;
    lua_leveldb_writeoptions_t *writeOptions;
    lua_leveldb_readoptions_t *readOptions;
} lua_leveldb_t;

typedef struct lua_leveldb_iterator_entry {
    lua_leveldb_t *db;
    leveldb_iterator_t *it;
} lua_leveldb_iterator_t;

typedef struct lua_leveldb_writebatch_entry {
    leveldb_writebatch_t *batch;
} lua_leveldb_writebatch_t;


static int lua_leveldb_readoption_new(lua_State *L)
{
    lua_leveldb_readoptions_t *options;
    int args_num = lua_gettop(L);
    char *key = NULL;

    if ( args_num > 1 ) {
        luaL_argerror(L, 1, "Too much argv than expected");
    }

    /* Create a readoption object */
    options = (lua_leveldb_readoptions_t *) 
        lua_newuserdata(L, sizeof(lua_leveldb_readoptions_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_READOPTIONS_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the read options object */
    options->options = leveldb_readoptions_create();

    /* Check and apply the config over the options */
    if ( args_num > 0 ) {
        lua_pushnil(L);  /* first key */
        while ( lua_next(L, 1) != 0 ) {
            /* uses 'key' (at index -2) and 'value' (at index -1) */
            if ( lua_type(L, -2) != LUA_TSTRING ) {
                continue;
            }

            key = (char *) luaL_checkstring(L, -2);
            if ( strcmp(key, "verify_checksums") == 0 ) {
                if ( lua_isboolean(L, -1) ) {
                    leveldb_readoptions_set_verify_checksums(
                        options->options, (unsigned char)lua_toboolean(L, -1));
                }
            } else if ( strcmp(key, "fill_cache") == 0 ) {
                if ( lua_isboolean(L, -1) ) {
                    leveldb_readoptions_set_fill_cache(
                        options->options, (unsigned char)lua_toboolean(L, -1));
                }
            }

            /* removes 'value'; keeps 'key' for next iteration */
            lua_pop(L, 1);
        }
    }

    return 1;
}

static int lua_leveldb_readoptions_destroy(lua_State *L)
{
    lua_leveldb_readoptions_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "readOptions object expected");
    self = (lua_leveldb_readoptions_t *) luaL_checkudata(L, 1, L_READOPTIONS_NAME);

    /* Check and destroy the readoptions object */
    if ( self->options != NULL ) {
        leveldb_readoptions_destroy(self->options);
        self->options = NULL;
    }

    return 0;
}

static int lua_leveldb_readoptions_set_verify_checksums(lua_State *L)
{
    lua_leveldb_readoptions_t *self;
    int checksums = 0;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "readOptions object and boolean expected");
    self = (lua_leveldb_readoptions_t *) luaL_checkudata(L, 1, L_READOPTIONS_NAME);
    assert_object_state(L, self->options);
    checksums = lua_toboolean(L, 2);

    leveldb_readoptions_set_verify_checksums(self->options, checksums);
    return 0;
}

static int lua_leveldb_readoptions_set_fill_cache(lua_State *L)
{
    lua_leveldb_readoptions_t *self;
    int fillCache = 0;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "readOptions object and boolean expected");
    self = (lua_leveldb_readoptions_t *) luaL_checkudata(L, 1, L_READOPTIONS_NAME);
    assert_object_state(L, self->options);
    fillCache = lua_toboolean(L, 2);

    leveldb_readoptions_set_fill_cache(self->options, fillCache);
    return 0;
}


static int lua_leveldb_writeoption_new(lua_State *L)
{
    lua_leveldb_writeoptions_t *options;
    int args_num = lua_gettop(L);
    char *key = NULL;

    if ( args_num > 1 ) {
        luaL_argerror(L, 1, "Too much argv than expected");
    }

    /* Create a writeoptions object */
    options = (lua_leveldb_writeoptions_t *) 
        lua_newuserdata(L, sizeof(lua_leveldb_writeoptions_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_WRITEOPTIONS_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the write options object */
    options->options = leveldb_writeoptions_create();

    /* Check and apply the config over the options */
    if ( args_num > 0 ) {
        lua_pushnil(L);  /* first key */
        while ( lua_next(L, 1) != 0 ) {
            /* uses 'key' (at index -2) and 'value' (at index -1) */
            if ( lua_type(L, -2) != LUA_TSTRING ) {
                continue;
            }

            key = (char *) luaL_checkstring(L, -2);
            if ( strcmp(key, "fill_cache") == 0 ) {
                if ( lua_isboolean(L, -1) ) {
                    leveldb_writeoptions_set_sync(
                        options->options, (unsigned char)lua_toboolean(L, -1));
                }
            }

            /* removes 'value'; keeps 'key' for next iteration */
            lua_pop(L, 1);
        }
    }

    return 1;
}

static int lua_leveldb_writeoptions_destroy(lua_State *L)
{
    lua_leveldb_writeoptions_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "writeOption object expected");
    self = (lua_leveldb_writeoptions_t *) luaL_checkudata(L, 1, L_WRITEOPTIONS_NAME);

    /* Check and destroy the readoptions object */
    if ( self->options != NULL ) {
        leveldb_writeoptions_destroy(self->options);
        self->options = NULL;
    }

    return 0;
}

static int lua_leveldb_writeoptions_set_sync(lua_State *L)
{
    lua_leveldb_writeoptions_t *self;
    int sync = 0;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "writeOption object and boolean expected");
    self = (lua_leveldb_writeoptions_t *) luaL_checkudata(L, 1, L_WRITEOPTIONS_NAME);
    assert_object_state(L, self->options);
    sync = lua_toboolean(L, 2);

    leveldb_writeoptions_set_sync(self->options, sync);
    return 0;
}


/** Write batch class entry implementation */
static int lua_leveldb_writebatch_new(lua_State *L)
{
    lua_leveldb_writebatch_t *writebatch;

    /* Create a iterator object */
    writebatch = (lua_leveldb_writebatch_t *) 
        lua_newuserdata(L, sizeof(lua_leveldb_writebatch_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_WRITEBATCH_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the write options object */
    writebatch->batch = leveldb_writebatch_create();

    return 1;
}

static int lua_leveldb_writebatch_destroy(lua_State *L)
{
    lua_leveldb_writebatch_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "writebatch object expected");
    self = (lua_leveldb_writebatch_t *) luaL_checkudata(L, 1, L_WRITEBATCH_NAME);

    /* Check and destroy the readoptions object */
    if ( self->batch != NULL ) {
        leveldb_writebatch_destroy(self->batch);
        self->batch = NULL;
    }

    return 0;
}

static int lua_leveldb_writebatch_clear(lua_State *L)
{
    lua_leveldb_writebatch_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "writebatch object expected");
    self = (lua_leveldb_writebatch_t *) luaL_checkudata(L, 1, L_WRITEBATCH_NAME);
    assert_object_state(L, self->batch);

    leveldb_writebatch_clear(self->batch);
    return 0;
}

static int lua_leveldb_writebatch_put(lua_State *L)
{
    lua_leveldb_writebatch_t *self;
    char *key, *val = NULL, *json_str;
    size_t keylen, vallen;
    int _type;
    cJSON *json = NULL;

    luaL_argcheck(L, lua_gettop(L) >= 3, 3, "writebatch object key and value expected");
    self = (lua_leveldb_writebatch_t *) luaL_checkudata(L, 1, L_WRITEBATCH_NAME);
    assert_object_state(L, self->batch);
    key = (char *) luaL_checklstring(L, 2, &keylen);

    _type = lua_type(L, 3);
    switch ( _type ) {
    case LUA_TSTRING:
        val = (char *)lua_pushfstring(L, "s%s", lua_tolstring(L, 3, &vallen));
        vallen += 1;
        break;
    case LUA_TNUMBER:
        if ( lua_isinteger(L, 3) ) {
            val = (char *)lua_pushfstring(L, "i%I", lua_tointeger(L, 3));
        } else {
            val = (char *)lua_pushfstring(L, "d%f", lua_tonumber(L, 3));
        }
        vallen = strlen(val);
        lua_pop(L, 1);
        break;
    case LUA_TBOOLEAN:
        val = lua_toboolean(L, 3) ? "b1" : "b0";
        vallen = 2;
        break;
    case LUA_TTABLE:
        json = table_is_array(L, 3) ? cJSON_CreateArray() : cJSON_CreateObject();
        table_to_json(json, L, 3);
        json_str = cJSON_PrintUnformatted(json);
        cJSON_Delete(json);
        if ( json_str == NULL ) {
            luaL_error(L, "Failed to encode the table");
        }

        val = (char *)lua_pushfstring(L, "t%s", json_str);
        free(json_str);
        vallen = strlen(val);
        break;
    default:
        luaL_error(L, "Invalid data type=%s", lua_typename(L, 3));
        break;
    }

    leveldb_writebatch_put(self->batch, key, keylen, val, vallen);
    return 0;
}

static int lua_leveldb_writebatch_delete(lua_State *L)
{
    lua_leveldb_writebatch_t *self;
    char *key;
    size_t keylen;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "writebatch object and key expected");
    self = (lua_leveldb_writebatch_t *) luaL_checkudata(L, 1, L_WRITEBATCH_NAME);
    assert_object_state(L, self->batch);
    key = (char *) luaL_checklstring(L, 2, &keylen);

    leveldb_writebatch_delete(self->batch, key, keylen);
    return 0;
}

static int lua_leveldb_writebatch_append(lua_State *L)
{
    lua_leveldb_writebatch_t *self, *_src;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "writebatch object and source write batch object expected");
    self = (lua_leveldb_writebatch_t *) luaL_checkudata(L, 1, L_WRITEBATCH_NAME);
    assert_object_state(L, self->batch);
    _src = (lua_leveldb_writebatch_t *) luaL_checkudata(L, 2, L_WRITEBATCH_NAME);
    assert_object_state(L, _src->batch);

    leveldb_writebatch_append(self->batch, _src->batch);
    return 0;
}


/** Main leveldb class entnry implementation */
static int lua_leveldb_new(lua_State *L)
{
    lua_leveldb_t *self;
    char *name = NULL, *key = NULL, *errptr = NULL;
    int args_num = lua_gettop(L);

    luaL_argcheck(L, args_num >= 1, 1, "db base dir and optional config expected");
    name = (char *) luaL_checkstring(L, 1);

    /* Create the user data and push it onto the stack */
    self = (lua_leveldb_t *) lua_newuserdata(L, sizeof(lua_leveldb_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_METATABLE_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the entry */
    self->name = name;
    self->options = leveldb_options_create();
    self->writeOptions = NULL;
    self->readOptions = NULL;

    /* Check and apply the config over the options */
    if ( args_num > 1 ) {
        lua_pushnil(L);  /* first key */
        while ( lua_next(L, 2) != 0 ) {
            /* uses 'key' (at index -2) and 'value' (at index -1) */
            if ( lua_type(L, -2) != LUA_TSTRING ) {
                continue;
            }

            key = (char *) luaL_checkstring(L, -2);
            if ( strcmp(key, "create_if_missing") == 0 ) {
                if ( lua_isboolean(L, -1) ) {
                    leveldb_options_set_create_if_missing(
                        self->options, (unsigned char )lua_toboolean(L, -1));
                }
            } else if ( strcmp(key, "error_if_exists") == 0 ) {
                if ( lua_isboolean(L, -1) ) {
                    leveldb_options_set_error_if_exists(
                        self->options, (unsigned char)lua_toboolean(L, -1));
                }
            } else if ( strcmp(key, "paranoid_checks") == 0 ) {
                if ( lua_isboolean(L, -1) ) {
                    leveldb_options_set_paranoid_checks(
                        self->options, (unsigned char)lua_toboolean(L, -1));
                }
            } else if ( strcmp(key, "write_buffer_size") == 0 ) {
                if ( lua_isinteger(L, -1) ) {
                    leveldb_options_set_write_buffer_size(
                        self->options, lua_tointeger(L, -1));
                }
            } else if ( strcmp(key, "max_open_files") == 0 ) {
                if ( lua_isinteger(L, -1) ) {
                    leveldb_options_set_max_file_size(
                        self->options, lua_tointeger(L, -1));
                }
            } else if ( strcmp(key, "compression") == 0 ) {
                if ( lua_isinteger(L, -1) ) {
                    leveldb_options_set_compression(
                        self->options, lua_tointeger(L, -1));
                }
            } else if ( strcmp(key, "read_option") == 0 ) {
                self->readOptions = (lua_leveldb_readoptions_t *) luaL_checkudata(L, -1, L_READOPTIONS_NAME);
            } else if ( strcmp(key, "write_option") == 0 ) {
                self->writeOptions = (lua_leveldb_writeoptions_t *) luaL_checkudata(L, -1, L_WRITEOPTIONS_NAME);
            } else if ( strcmp(key, "bloomfilter_bits") == 0 ) {
            }

            /* removes 'value'; keeps 'key' for next iteration */
            lua_pop(L, 1);
        }
    }

    /* check and set the default read and write options */
    if ( self->writeOptions == NULL ) {
        self->writeOptions = (lua_leveldb_writeoptions_t *) 
            lua_newuserdata(L, sizeof(lua_leveldb_writeoptions_t));
        luaL_getmetatable(L, L_WRITEOPTIONS_NAME);
        lua_setmetatable(L, -2);
        self->writeOptions->options = leveldb_writeoptions_create();
        lua_pop(L, 1);
    }
    
    if ( self->readOptions == NULL ) {
        self->readOptions = (lua_leveldb_readoptions_t *) 
            lua_newuserdata(L, sizeof(lua_leveldb_readoptions_t));
        luaL_getmetatable(L, L_READOPTIONS_NAME);
        lua_setmetatable(L, -2);
        self->readOptions->options = leveldb_readoptions_create();
        lua_pop(L, 1);
    }

    /* Try to open the database */
    self->db = leveldb_open(self->options, self->name, &errptr);
    if ( errptr != NULL ) {
        luaL_error(L, "Failed to open database with errptr=%s", errptr);
        leveldb_free(errptr);
    }

    return 1;
}

static int lua_leveldb_destroy(lua_State *L)
{
    lua_leveldb_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    /* 
     * release the allactions .
     * Since Lua will manager the allocations of the read and write options
     * so here we don't have to do anything with them.
    */
    if ( self->options != NULL ) {
        leveldb_options_destroy(self->options);
        self->options = NULL;
    }

    if ( self->db != NULL ) {
        leveldb_close(self->db);
        self->db = NULL;
    }

    return 0;
}

static int lua_leveldb_set_read_option(lua_State *L)
{
    lua_leveldb_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "DB Object and readOption object expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);

    self->readOptions = (lua_leveldb_readoptions_t *) luaL_checkudata(L, 2, L_READOPTIONS_NAME);

    return 1;
}

static int lua_leveldb_set_write_option(lua_State *L)
{
    lua_leveldb_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "DB Object and writeOption object expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);

    self->writeOptions = (lua_leveldb_writeoptions_t *) luaL_checkudata(L, 2, L_WRITEOPTIONS_NAME);

    return 0;
}

/**
 * here we add boolean,integer,numeric,string,table support.
 * the orginal data will be encode like this:
 * +-----------+----------+
 * | data_type | raw data |
 * +-----------+----------+
 * | s:string  | dynamic  |
 * +-----------+----------+
 * | i:integer | dynamic  |
 * +-----------+----------+
 * | d:doulbe  | dynamic  |
 * +-----------+----------+
 * | b:boolean | dynamic  |
 * +-----------+----------+
 * | t:table   | dynamic  |
 * +-----------+----------+
*/
static int lua_leveldb_put(lua_State *L)
{
    lua_leveldb_t *self;
    char *key = NULL, *val = NULL, *json_str = NULL, *errptr = NULL;
    size_t keylen, vallen;
    int _type;
    cJSON *json = NULL;

    luaL_argcheck(L, lua_gettop(L) == 3, 3, "Object key and value expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);
    key = (char *) luaL_checklstring(L, 2, &keylen);

    _type = lua_type(L, 3);
    switch ( _type ) {
    case LUA_TSTRING:
        val = (char *)lua_pushfstring(L, "s%s", lua_tolstring(L, 3, &vallen));
        vallen += 1;
        break;
    case LUA_TNUMBER:
        if ( lua_isinteger(L, 3) ) {
            val = (char *)lua_pushfstring(L, "i%I", lua_tointeger(L, 3));
        } else {
            val = (char *)lua_pushfstring(L, "d%f", lua_tonumber(L, 3));
        }
        vallen = strlen(val);
        lua_pop(L, 1);
        break;
    case LUA_TBOOLEAN:
        val = lua_toboolean(L, 3) ? "b1" : "b0";
        vallen = 2;
        break;
    case LUA_TTABLE:
        json = table_is_array(L, 3) ? cJSON_CreateArray() : cJSON_CreateObject();
        table_to_json(json, L, 3);
        json_str = cJSON_PrintUnformatted(json);
        cJSON_Delete(json);
        if ( json_str == NULL ) {
            luaL_error(L, "Failed to encode the table");
        }

        val = (char *)lua_pushfstring(L, "t%s", json_str);
        free(json_str);
        vallen = strlen(val);
        break;
    default:
        luaL_error(L, "Invalid data type=%s", lua_typename(L, 3));
        break;
    }

    /* Put the data to the database */
    leveldb_put(self->db, self->writeOptions->options, key, keylen, val, vallen, &errptr);
    if ( errptr == NULL ) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
        leveldb_free(errptr);
    }

    return 1;
}

static int lua_leveldb_write(lua_State *L)
{
    lua_leveldb_t *self;
    lua_leveldb_writebatch_t *writebatch;
    char *errptr = NULL;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "Object and writebatch object expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);
    writebatch = (lua_leveldb_writebatch_t *) luaL_checkudata(L, 2, L_WRITEBATCH_NAME);

    leveldb_write(self->db, self->writeOptions->options, writebatch->batch, &errptr);
    if ( errptr == NULL ) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
        leveldb_free(errptr);
    }

    return 1;
}

/**
 * convert the binary value fetched from leveldb
 *  to a null terminated binary string.
 *
 * @param   val original raw data fetch from leveldb
 * @param   len the length of the value
*/
static void null_terminated_adjust(char *val, size_t len)
{
    register int i, e = len - 1;
    for ( i = 0; i < e; i++ ) {
        val[i] = val[i+1];
    }
    val[e] = '\0';
}

static int lua_leveldb_get(lua_State *L)
{
    lua_leveldb_t *self;
    char *key = NULL, *val = NULL, *errptr = NULL;
    size_t keylen, vallen;
    cJSON *json = NULL;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and key expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);
    key = (char *) luaL_checklstring(L, 2, &keylen);

    /* get the value of the specified key */
    val = leveldb_get(self->db, self->readOptions->options, key, keylen, &vallen, &errptr);
    if ( val == NULL || vallen < 2 ) {
        lua_pushnil(L);
    } else {
        switch ( val[0] ) {
        case 's':
            lua_pushlstring(L, val + 1, vallen - 1);
            break;
        case 'i':
            null_terminated_adjust(val, vallen);
            lua_pushinteger(L, atoi(val));
            break;
        case 'd':
            null_terminated_adjust(val, vallen);
            lua_pushnumber(L, strtod(val, NULL));
            break;
        case 'b':
            lua_pushboolean(L, val[1]=='1' ? 1 : 0);
            break;
        case 't':
            null_terminated_adjust(val, vallen);
            json = cJSON_Parse(val);
            if ( json == NULL ) {
                luaL_error(L, "Failed to decode the value of key `%s`", key);
            }

            /* Convert the JSON to table */
            lua_newtable(L);
            if ( json->type == cJSON_Array ) {
                cjson_to_table(json->child, cJSON_Array, L);
            } else {
                cjson_to_table(json->child, cJSON_Object, L);
            }

            cJSON_Delete(json);
            break;
        default:
            lua_pushlstring(L, val, vallen);
            break;
        }
    }

    if ( val != NULL ) leveldb_free(val);
    if ( errptr != NULL ) leveldb_free(errptr);

    return 1;
}

static int lua_leveldb_delete(lua_State *L)
{
    lua_leveldb_t *self;
    char *key = NULL, *errptr = NULL;
    size_t keylen;

    luaL_argcheck(L, lua_gettop(L) == 2, 2, "Object and key expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);
    key = (char *) luaL_checklstring(L, 2, &keylen);

    leveldb_delete(self->db, self->writeOptions->options, key, keylen, &errptr);
    if ( errptr == NULL ) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
        leveldb_free(errptr);
    }

    return 1;
}

static int lua_leveldb_repair_db(lua_State *L)
{
    lua_leveldb_t *self;
    char *errptr = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);

    leveldb_repair_db(self->options, self->name, &errptr);
    if ( errptr == NULL ) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
        leveldb_free(errptr);
    }

    return 1;
}

static int lua_leveldb_destroy_db(lua_State *L)
{
    lua_leveldb_t *self;
    char *errptr = NULL;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self->db);

    leveldb_destroy_db(self->options, self->name, &errptr);
    if ( errptr == NULL ) {
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
        leveldb_free(errptr);
    }

    return 1;
}
/* End of leveldb main class */


/** iterator class entry implementation */
static int lua_leveldb_iterator_new(lua_State *L)
{
    lua_leveldb_iterator_t *it;
    lua_leveldb_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Object expected");
    self = (lua_leveldb_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    /* Create a iterator object */
    it = (lua_leveldb_iterator_t *) 
        lua_newuserdata(L, sizeof(lua_leveldb_iterator_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_ITERATOR_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the iterator */
    it->db = self;
    it->it = leveldb_create_iterator(self->db, self->readOptions->options);
    leveldb_iter_seek_to_first(it->it);

    return 1;
}

static int lua_leveldb_iterator_destroy(lua_State *L)
{
    lua_leveldb_iterator_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);

    /* Check and destroy the iterator object */
    if ( self->it != NULL ) {
        leveldb_iter_destroy(self->it);
        self->it = NULL;
    }

    return 0;
}

static int lua_leveldb_iterator_valid(lua_State *L)
{
    lua_leveldb_iterator_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);

    lua_pushboolean(L, leveldb_iter_valid(self->it) ? 1 : 0);
    return 1;
}

static int lua_leveldb_iterator_seek_to_first(lua_State *L)
{
    lua_leveldb_iterator_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);
    
    leveldb_iter_seek_to_first(self->it);
    return 0;
}

static int lua_leveldb_iterator_seek_to_last(lua_State *L)
{
    lua_leveldb_iterator_t *self;

    luaL_argcheck(L, lua_gettop(L) == 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);
    
    leveldb_iter_seek_to_last(self->it);
    return 0;
}

static int lua_leveldb_iterator_seek(lua_State *L)
{
    lua_leveldb_iterator_t *self;
    char *key;
    size_t keylen;

    luaL_argcheck(L, lua_gettop(L) >= 2, 2, "Iterator object and key expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);
    key = (char *) luaL_checklstring(L, 2, &keylen);

    leveldb_iter_seek(self->it, key, keylen);
    return 0;
}

static int lua_leveldb_iterator_next(lua_State *L)
{
    lua_leveldb_iterator_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);

    leveldb_iter_next(self->it);
    return 0;
}

static int lua_leveldb_iterator_prev(lua_State *L)
{
    lua_leveldb_iterator_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);

    leveldb_iter_prev(self->it);
    return 0;
}

static int lua_leveldb_iterator_key(lua_State *L)
{
    lua_leveldb_iterator_t *self;
    char *key;
    size_t keylen;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);

    key = (char *)leveldb_iter_key(self->it, &keylen);
    if ( key == NULL ) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, key, keylen);
        // leveldb_free(key);
    }

    return 1;
}

static int lua_leveldb_iterator_value(lua_State *L)
{
    lua_leveldb_iterator_t *self;
    char *val = NULL;
    size_t vallen;
    cJSON *json;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);

    val = (char *)leveldb_iter_value(self->it, &vallen);
    if ( val == NULL ) {
        lua_pushnil(L);
    } else {
        switch ( val[0] ) {
        case 's':
            lua_pushlstring(L, val + 1, vallen - 1);
            break;
        case 'i':
            null_terminated_adjust(val, vallen);
            lua_pushinteger(L, atoi(val));
            break;
        case 'd':
            null_terminated_adjust(val, vallen);
            lua_pushnumber(L, strtod(val, NULL));
            break;
        case 'b':
            lua_pushboolean(L, val[1]=='1' ? 1 : 0);
            break;
        case 't':
            null_terminated_adjust(val, vallen);
            json = cJSON_Parse(val);
            if ( json == NULL ) {
                luaL_error(L, "Failed to decode the value in iterator");
            }

            /* Convert the JSON to table */
            lua_newtable(L);
            if ( json->type == cJSON_Array ) {
                cjson_to_table(json->child, cJSON_Array, L);
            } else {
                cjson_to_table(json->child, cJSON_Object, L);
            }

            cJSON_Delete(json);
            break;
        default:
            lua_pushlstring(L, val, vallen);
            break;
        }

        // leveldb_free(val);
    }

    return 1;
}

static int lua_leveldb_iterator_error(lua_State *L)
{
    lua_leveldb_iterator_t *self;
    char *errptr = NULL;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "Iterator object expected");
    self = (lua_leveldb_iterator_t *) luaL_checkudata(L, 1, L_ITERATOR_NAME);
    assert_object_state(L, self->it);

    leveldb_iter_get_error(self->it, &errptr);
    if ( errptr == NULL ) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, errptr);
        leveldb_free(errptr);
    }

    return 1;
}


/* Read options method array */
static const struct luaL_Reg leveldb_readoptions_methods[] = {
    { "setVerifyChecksums", lua_leveldb_readoptions_set_verify_checksums },
    { "setFillCache",       lua_leveldb_readoptions_set_fill_cache },
    { "destroy",            lua_leveldb_readoptions_destroy },
    { "__gc",               lua_leveldb_readoptions_destroy },
    { NULL, NULL }
};

/* Write options method array */
static const struct luaL_Reg leveldb_writeoptions_methods[] = {
    { "setSync",        lua_leveldb_writeoptions_set_sync },
    { "destroy",        lua_leveldb_writeoptions_destroy },
    { "__gc",           lua_leveldb_writeoptions_destroy },
    { NULL, NULL }
};

/* Write batch method array */
static const struct luaL_Reg leveldb_writebatch_methods[] = {
    { "put",        lua_leveldb_writebatch_put },
    { "delete",     lua_leveldb_writebatch_delete },
    { "clear",      lua_leveldb_writebatch_clear },
    { "append",     lua_leveldb_writebatch_append },
    { "destroy",    lua_leveldb_writebatch_destroy },
    { "__gc",       lua_leveldb_writebatch_destroy },
    { NULL, NULL }
};

/* Lua module method array */
static const struct luaL_Reg leveldb_methods[] = {
    { "new",                lua_leveldb_new },
    { "put",                lua_leveldb_put },
    { "write",              lua_leveldb_write },
    { "get",                lua_leveldb_get },
    { "delete",             lua_leveldb_delete },
    { "repair",             lua_leveldb_repair_db },
    { "destroy",            lua_leveldb_destroy_db },
    { "createWriteBatch",   lua_leveldb_writebatch_new },
    { "createReadOption",   lua_leveldb_readoption_new },
    { "createWriteOption",  lua_leveldb_writeoption_new },
    { "setReadOption",      lua_leveldb_set_read_option },
    { "setWriteOption",     lua_leveldb_set_write_option },
    { "iterator",           lua_leveldb_iterator_new },
    { "close",              lua_leveldb_destroy },
    { "__gc",               lua_leveldb_destroy },
    { NULL, NULL }
};

/* Iterator module method array */
static const struct luaL_Reg leveldb_iterator_methods[] = {
    { "valid",          lua_leveldb_iterator_valid },
    { "seekToFirst",    lua_leveldb_iterator_seek_to_first },
    { "seekToLast",     lua_leveldb_iterator_seek_to_last },
    { "seek",           lua_leveldb_iterator_seek },
    { "next",           lua_leveldb_iterator_next },
    { "prev",           lua_leveldb_iterator_prev },
    { "key",            lua_leveldb_iterator_key },
    { "value",          lua_leveldb_iterator_value },
    { "error",          lua_leveldb_iterator_error },
    { "destroy",        lua_leveldb_iterator_destroy },
    { "__gc",           lua_leveldb_iterator_destroy },
    { NULL, NULL }
};


/**
 * register the current library to specified stack
 *
 * @param   L
 * @return  integer
*/
int luaopen_leveldb(lua_State *L)
{
    /* 1, Register the readoptions metatable */
    luaL_newmetatable(L, L_READOPTIONS_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, leveldb_readoptions_methods, 0);

    /* 2, Register the writeoptions metatable */
    luaL_newmetatable(L, L_WRITEOPTIONS_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, leveldb_writeoptions_methods, 0);

    /* 2, Register the writebatch metatable */
    luaL_newmetatable(L, L_WRITEBATCH_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, leveldb_writebatch_methods, 0);

    /* 4, Register the leveldb iterator metatable */
    luaL_newmetatable(L, L_ITERATOR_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, leveldb_iterator_methods, 0);

    /* 5, Register the leveldb metatable */
    luaL_newmetatable(L, L_METATABLE_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    /* Register some constants */
    lua_pushinteger(L, leveldb_major_version());
    lua_setfield(L, -2, "MAJOR_VERSION");
    lua_pushinteger(L, leveldb_minor_version());
    lua_setfield(L, -2, "MINOR_VERSION");

    lua_pushinteger(L, leveldb_no_compression);
    lua_setfield(L, -2, "NO_COMPRESSION");
    lua_pushinteger(L, leveldb_snappy_compression);
    lua_setfield(L, -2, "SNAPPY_COMPRESSION");

    /* Register the methods */
    luaL_setfuncs(L, leveldb_methods, 0);

    return 1;
}
