/**
 * sarah mysql extension implementation.
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

#include "mysql/mysql.h"
#include "april.h"
#include "sarah.h"
#include "sarah_utils.h"


#ifdef WIN32
#include <winsock2.h>
#define NO_CLIENT_LONG_LONG
#endif

/* For compat with the old version Mysql client under 4.0 */
#if ( MYSQL_VERSION_ID < 40100 )
#define MYSQL_TYPE_VAR_STRING   FIELD_TYPE_VAR_STRING
#define MYSQL_TYPE_STRING       FIELD_TYPE_STRING
#define MYSQL_TYPE_DECIMAL      FIELD_TYPE_DECIMAL
#define MYSQL_TYPE_SHORT        FIELD_TYPE_SHORT
#define MYSQL_TYPE_LONG         FIELD_TYPE_LONG
#define MYSQL_TYPE_FLOAT        FIELD_TYPE_FLOAT
#define MYSQL_TYPE_DOUBLE       FIELD_TYPE_DOUBLE
#define MYSQL_TYPE_LONGLONG     FIELD_TYPE_LONGLONG
#define MYSQL_TYPE_INT24        FIELD_TYPE_INT24
#define MYSQL_TYPE_YEAR         FIELD_TYPE_YEAR
#define MYSQL_TYPE_TINY         FIELD_TYPE_TINY
#define MYSQL_TYPE_TINY_BLOB    FIELD_TYPE_TINY_BLOB
#define MYSQL_TYPE_MEDIUM_BLOB  FIELD_TYPE_MEDIUM_BLOB
#define MYSQL_TYPE_LONG_BLOB    FIELD_TYPE_LONG_BLOB
#define MYSQL_TYPE_BLOB         FIELD_TYPE_BLOB
#define MYSQL_TYPE_DATE         FIELD_TYPE_DATE
#define MYSQL_TYPE_NEWDATE      FIELD_TYPE_NEWDATE
#define MYSQL_TYPE_DATETIME     FIELD_TYPE_DATETIME
#define MYSQL_TYPE_TIME         FIELD_TYPE_TIME
#define MYSQL_TYPE_TIMESTAMP    FIELD_TYPE_TIMESTAMP
#define MYSQL_TYPE_ENUM         FIELD_TYPE_ENUM
#define MYSQL_TYPE_SET          FIELD_TYPE_SET
#define MYSQL_TYPE_NULL         FIELD_TYPE_NULL

#define mysql_commit(_) ((void)_)
#define mysql_rollback(_) ((void)_)
#define mysql_autocommit(_,__) ((void)_)
#endif


#define L_METATABLE_NAME "_ext_mysql"
#define L_CURSOR_NAME "_ext_mysql_cursor"

#define assert_object_state(L, ptr) \
{if (ptr == NULL) {luaL_argerror(L, 1, "Invalid state(Is the object closed?)");}}

#define lua_mysql_print(mysql)  \
{   \
    printf( \
        "host=%s, user=%s, passwd=%s, db=%s, "  \
        "port=%u, unix_socket=%s, client_flag=%d, charset=%s\n",    \
        mysql->host, mysql->user, mysql->passwd, mysql->db, \
        mysql->port, mysql->unix_socket, mysql->client_flag, mysql->charset \
    );  \
}

enum enum_april_types
{
    APRIL_TYPE_STRING,
    APRIL_TYPE_INTEGER,
    APRIL_TYPE_NUMBER,
    APRIL_TYPE_NOTSUPPORT
};

typedef struct lua_mysql_entry {
    char *host;
    char *user;
    char *passwd;
    char *db;
    unsigned int port;
    char *unix_socket;
    unsigned int client_flag;
    char *charset;
    MYSQL *my_conn;
    int debug;
} lua_mysql_t;

typedef struct lua_mysql_cursor_entry {
    unsigned int num_cols;
    MYSQL_FIELD *fields;
    long long int num_rows;
    MYSQL_RES *my_res;
} lua_mysql_cursor_t;


static int getAprilType(enum enum_field_types type) {

	switch (type) {
    case MYSQL_TYPE_VAR_STRING: case MYSQL_TYPE_STRING:
        return APRIL_TYPE_STRING;
    case MYSQL_TYPE_SHORT: case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24: case MYSQL_TYPE_YEAR: case MYSQL_TYPE_TINY:
        return APRIL_TYPE_INTEGER;
    case MYSQL_TYPE_DECIMAL: case MYSQL_TYPE_FLOAT: case MYSQL_TYPE_DOUBLE: 
        return APRIL_TYPE_NUMBER;
    case MYSQL_TYPE_TINY_BLOB: case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB: case MYSQL_TYPE_BLOB:
        return APRIL_TYPE_STRING;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIME:
        return APRIL_TYPE_STRING;
    case MYSQL_TYPE_TIMESTAMP:
        return APRIL_TYPE_INTEGER;
    case MYSQL_TYPE_ENUM: case MYSQL_TYPE_SET:
        return APRIL_TYPE_NOTSUPPORT;
    case MYSQL_TYPE_NULL:
        return APRIL_TYPE_STRING;
    default:
        return APRIL_TYPE_STRING;
	}
}

/*
 * Get the internal database type of the specified field.
*/
static char *getFieldType (enum enum_field_types type) {

	switch (type) {
    case MYSQL_TYPE_VAR_STRING: case MYSQL_TYPE_STRING:
        return "string";
    case MYSQL_TYPE_SHORT: case MYSQL_TYPE_LONG: case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24: case MYSQL_TYPE_YEAR: case MYSQL_TYPE_TINY:
        return "integer";
    case MYSQL_TYPE_DECIMAL: 
    case MYSQL_TYPE_FLOAT: case MYSQL_TYPE_DOUBLE:
        return "decimal";
    case MYSQL_TYPE_TINY_BLOB: case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB: case MYSQL_TYPE_BLOB:
        return "binary";
    case MYSQL_TYPE_DATE: case MYSQL_TYPE_NEWDATE:
        return "date";
    case MYSQL_TYPE_DATETIME:
        return "datetime";
    case MYSQL_TYPE_TIME:
        return "time";
    case MYSQL_TYPE_TIMESTAMP:
        return "timestamp";
    case MYSQL_TYPE_ENUM: case MYSQL_TYPE_SET:
        return "set";
    case MYSQL_TYPE_NULL:
        return "null";
    default:
        return "undefined";
	}
}


/* Mysql cursor object implementation */
static int lua_mysql_cursor_next(lua_State *L)
{
    lua_mysql_cursor_t *self;
    MYSQL_ROW row;
    unsigned long *lengths = NULL;
    unsigned int i, _type;
    char buffer[41] = {'\0'};

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql cursor object expected");
    self = (lua_mysql_cursor_t *) luaL_checkudata(L, 1, L_CURSOR_NAME);

    if ( self->fields == NULL ) {
        self->fields = mysql_fetch_fields(self->my_res);
    }

    /* keep fetch the next row until the last record */
    if ( (row = mysql_fetch_row(self->my_res)) == NULL ) {
        lua_pushnil(L);
        return 1;
    }

    /* Move the row to a table and return it as the final result */
    lengths = mysql_fetch_lengths(self->my_res);
    lua_newtable(L);
    for ( i = 0; i < self->num_cols; i++ ) {
        _type = getAprilType(self->fields[i].type);
        switch ( _type ) {
        case APRIL_TYPE_STRING:
            lua_pushlstring(L, row[i], lengths[i]);
            break;
        case APRIL_TYPE_INTEGER:
            sprintf(buffer, "%.*s", (int) lengths[i], row[i]);
            lua_pushinteger(L, strtol(buffer, NULL, 10));
            break;
        case APRIL_TYPE_NUMBER:
            sprintf(buffer, "%.*s", (int) lengths[i], row[i]);
            lua_pushnumber(L, strtod(buffer, NULL));
            break;
        // case APRIL_TYPE_NOTSUPPORT:
        default:
            luaL_error(L, "Unsupported field type for field %s\n", self->fields[i].name);
            break;
        }
        
        // Add the field and value to the table
        lua_setfield(L, -2, self->fields[i].name);
    }

    return 1;
}

static int lua_mysql_cursor_rows(lua_State *L)
{
    lua_mysql_cursor_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql cursor object expected");
    self = (lua_mysql_cursor_t *) luaL_checkudata(L, 1, L_CURSOR_NAME);

    if ( self->num_rows == -1 ) {
        self->num_rows = mysql_num_rows(self->my_res);
    }

    lua_pushinteger(L, self->num_rows);
    return 1;
}

static int lua_mysql_cursor_cols(lua_State *L)
{
    lua_mysql_cursor_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql cursor object expected");
    self = (lua_mysql_cursor_t *) luaL_checkudata(L, 1, L_CURSOR_NAME);

    lua_pushinteger(L, self->num_cols);
    return 1;
}

static int lua_mysql_cursor_fields(lua_State *L)
{
    lua_mysql_cursor_t *self;
    unsigned int i;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql cursor object expected");
    self = (lua_mysql_cursor_t *) luaL_checkudata(L, 1, L_CURSOR_NAME);

    if ( self->fields == NULL ) {
        self->fields = mysql_fetch_fields(self->my_res);
    }

    /* Make and push the field info to a table */
    lua_newtable(L);
    for ( i = 0; i < self->num_cols; i++ ) {
        lua_pushstring(L, getFieldType(self->fields[i].type));
        lua_setfield(L, -2, self->fields[i].name);
    }
    
    return 1;
}

static int lua_mysql_cursor_destroy(lua_State *L)
{
    lua_mysql_cursor_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql cursor object expected");
    self = (lua_mysql_cursor_t *) luaL_checkudata(L, 1, L_CURSOR_NAME);

    if ( self->my_res != NULL ) {
        mysql_free_result(self->my_res);
        self->num_rows = -1;
        self->num_cols = -1;
        self->fields = NULL;
        self->my_res = NULL;
    }

    return 0;
}


/* Mysql object implementation */
static int lua_mysql_new(lua_State *L)
{
    lua_mysql_t *self;

    /* Create a mysql object */
    self = (lua_mysql_t *) lua_newuserdata(L, sizeof(lua_mysql_t));
    /* Push the metatable onto the stack */
    luaL_getmetatable(L, L_METATABLE_NAME);
    /* Set the metatable on the userdata */
    lua_setmetatable(L, -2);

    /* Initialize the mysql */
    self->my_conn = mysql_init(NULL);
    if ( self->my_conn == NULL ) {
        luaL_error(L, "error to initialized the mysql: Out of memory?");
    }

    self->host   = "localhost";
    self->user   = NULL;
    self->passwd = NULL;
    self->db     = NULL;
    self->port   = 3306;
    self->unix_socket = NULL;
    self->client_flag = 0;
    self->charset = "utf8";
    self->debug  = 0;

    return 1;
}

static int lua_mysql_destroy(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);

    if ( self->my_conn != NULL ) {
        mysql_close(self->my_conn);
        mysql_library_end();
        self->my_conn = NULL;
    }

    return 0;
}

static int lua_mysql_connect(lua_State *L)
{
    lua_mysql_t *self;
    char *key = NULL;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    
    /* check and parse the connection arguments */
    if ( ! lua_istable(L, 2) ) {
        luaL_argerror(L, 2, "standard config table expected");
    }

    lua_pushnil(L);
    while ( lua_next(L, 2) != 0 ) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        if ( lua_type(L, -2) != LUA_TSTRING ) {
            continue;
        }

        key = (char *) luaL_checkstring(L, -2);
        if ( strcmp(key, "host") == 0 ) {
            self->host = (char *) lua_tostring(L, -1);
        } else if ( strcmp(key, "username") == 0 ) {
            self->user = (char *) lua_tostring(L, -1);
        } else if ( strcmp(key, "password") == 0 ) {
            self->passwd = (char *) lua_tostring(L, -1);
        } else if ( strcmp(key, "database") == 0 ) {
            self->db = (char *) lua_tostring(L, -1);
        } else if ( strcmp(key, "port") == 0 ) {
            self->port = (unsigned int) lua_tointeger(L, -1);
        } else if ( strcmp(key, "charset") == 0 ) {
            self->charset = (char *) lua_tostring(L, -1);
        } else if ( strcmp(key, "unix_socket") == 0 ) {
            self->unix_socket = (char *) lua_tostring(L, -1);
        } else if ( strcmp(key, "client_flag") == 0 ) {
            self->client_flag = (unsigned int) lua_tointeger(L, -1);
        }

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }

#ifdef SARAH_LINUX
    cel_strtolower(self->host, strlen(self->host));
    if ( strcmp(self->host, "localhost") == 0 
        || strcmp(self->host, "127.0.0.1") == 0 ) {
        if ( access("/var/run/mysqld/mysqld.sock", F_OK) == -1 ) {
            luaL_argerror(L, 1, "A valid unix_socket "
                "expected for local server connection");
        }

        self->unix_socket = "/var/run/mysqld/mysqld.sock";
    }
#endif

    /* Try to connect to the database and push the state as status */
    if ( mysql_real_connect(self->my_conn, 
        self->host, self->user, self->passwd, self->db, 
            self->port, self->unix_socket, self->client_flag) == NULL ) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, 1);
    }

    /* check and select the default database */
    if ( self->db != NULL 
        && mysql_select_db(self->my_conn, self->db) != 0 ) {
        luaL_error(L, "Fail to select the database to %s", self->db);
    }

    return 1;
}

static int lua_mysql_set_debug(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 2, 1, "mysql object boolean expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    self->debug = lua_toboolean(L, 2);

    return 0;
}

static int lua_mysql_ping(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);

    lua_pushboolean(L, mysql_ping(self->my_conn) == 0 ? 1 : 0);
    return 1;
}

static int lua_mysql_real_escape_string(lua_State *L)
{
    lua_mysql_t *self;
    char *from = NULL, *to = NULL;
    size_t size = 0, new_size = 0;

    luaL_argcheck(L, lua_gettop(L) >= 2, 1, "mysql object and string expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    from = (char *) luaL_checklstring(L, 2, &size);

    to = (char *) sarah_malloc(sizeof(char) * (2 * size + 1));
    if ( to == NULL ) {
        luaL_error(L, "Fail to do the allocate");
    }

    new_size = mysql_real_escape_string(self->my_conn, to, from, size);
    lua_pushlstring(L, to, new_size);
    sarah_free(to);

    return 1;
}

static int lua_mysql_select_db(lua_State *L)
{
    lua_mysql_t *self;
    char *db = NULL;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object and database name expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    db = (char *) luaL_checkstring(L, 2);

    lua_pushboolean(L, mysql_select_db(self->my_conn, db) == 0 ? 1 : 0);
    return 1;
}

static int lua_mysql_query(lua_State *L)
{
    lua_mysql_t *self;
    lua_mysql_cursor_t *cursor;
    char *sql = NULL;
    size_t sql_len = 0;
    MYSQL_RES *res = NULL;
    unsigned int status = 0, num_cols;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object and query sql expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    sql  = (char *) luaL_checklstring(L, 2, &sql_len);

    if ( self->debug ) {
        printf("[Info]: query#sql=%s\n", sql);
    }

    if ( mysql_real_query(self->my_conn, sql, sql_len) != 0 ) {
        lua_pushboolean(L, 0);
        return 1;
    }

    /* query succeed, process any data returned by it */
    res = mysql_store_result(self->my_conn);
    num_cols = mysql_field_count(self->my_conn);
    if ( res == NULL ) {
        /* returned nothing, should it have? */
        if ( num_cols == 0 ) {
            /* Query does not return data (it was not a SELECT) */
            status = 1;
        } else {
            /* It should have returned data, return nil */
            // luaL_error(L, "Error: %s\n", mysql_error(self->my_conn));
            status = 0;
        }

        lua_pushboolean(L, status);
        return 1;
    }

    /* create a new query cursor to fetch the query data */
    cursor = (lua_mysql_cursor_t *) lua_newuserdata(L, sizeof(lua_mysql_cursor_t));
    luaL_getmetatable(L, L_CURSOR_NAME);
    lua_setmetatable(L, -2);

    /* Initialize the cursor */
    cursor->num_rows = -1;
    cursor->num_cols = num_cols;
    cursor->fields   = NULL;
    cursor->my_res   = res;

    return 1;
}

static int lua_mysql_insert_id(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);

    lua_pushinteger(L, mysql_insert_id(self->my_conn));
    return 1;
}

static int lua_mysql_affected_rows(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);

    lua_pushinteger(L, mysql_affected_rows(self->my_conn));
    return 1;
}

static int lua_mysql_errno(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);

    lua_pushinteger(L, mysql_errno(self->my_conn));
    return 1;
}

static int lua_mysql_error(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);

    lua_pushstring(L, mysql_error(self->my_conn));
    return 1;
}

static int lua_mysql_add(lua_State *L)
{
    lua_mysql_t *self;
    char *tbl_name = NULL, *key = NULL, *val = NULL, *nval = NULL;
    cel_link_t val_list;
    int counter, _type, args_num;
    size_t vallen, new_len, sql_len;

    luaL_Buffer sql_buffer;
    const char *query_sql = NULL, *onDuplicateKey = NULL;


    args_num = lua_gettop(L);
    luaL_argcheck(L, args_num >= 3, 1, "mysql object, table name and data expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    tbl_name = (char *) luaL_checkstring(L, 2);
    if ( args_num > 3 ) {
        onDuplicateKey = lua_tostring(L, 4);
    }

    /* Parse the data and create the Query SQL for insertion */
    luaL_buffinit(L, &sql_buffer);
    luaL_addstring(&sql_buffer, "INSERT INTO ");
    luaL_addstring(&sql_buffer, tbl_name);
    luaL_addchar(&sql_buffer, '(');

    /* Append the field list */
    counter = 0;
    cel_link_init(&val_list);
    lua_pushnil(L);
    while ( lua_next(L, 3) != 0 ) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        if ( lua_type(L, -2) != LUA_TSTRING ) {
            luaL_error(L, "Invalid field "
                "type=%s, string expected\n", lua_typename(L, -2));
            continue;
        }

        key = (char *) lua_tostring(L, -2);
        if ( counter == 0 ) {
            luaL_addstring(&sql_buffer, key);
        } else {
            luaL_addchar(&sql_buffer, ',');
            luaL_addstring(&sql_buffer, key);
        }

        _type = lua_type(L, -1);
        switch ( _type ) {
        case LUA_TSTRING:
            val  = (char *)lua_tolstring(L, -1, &vallen);
            nval = sarah_malloc(sizeof(char) * (2 * vallen + 1));
            if ( nval == NULL ) {
                luaL_error(L, "Fail to do the allocate");
            }

            new_len = mysql_real_escape_string(self->my_conn, nval, val, vallen);
            lua_pushlstring(L, nval, new_len);
            sarah_free(nval);
            val = (char *)lua_tostring(L, -1);
            lua_pop(L, 1);
            break;
        case LUA_TNUMBER:
            if ( lua_isinteger(L, -1) ) {
                val = (char *)lua_pushfstring(L, "%I", lua_tointeger(L, -1));
            } else {
                val = (char *)lua_pushfstring(L, "%f", lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            break;
        case LUA_TBOOLEAN:
            val = lua_toboolean(L, -1) ? "1" : "0";
            break;
        default:
            luaL_error(L, "Invalid value type=%s for field=%s", 
                lua_typename(L, -1), (char *)lua_tostring(L, -2));
        }

        cel_link_add_last(&val_list, val);
        counter++;

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }

    /* Append the value list */
    counter = 0;
    luaL_addstring(&sql_buffer, ") VALUES(");
    while ( (val = cel_link_remove_first(&val_list)) != NULL ) {
        if ( counter == 0 ) {
            luaL_addchar(&sql_buffer, '\'');
            luaL_addstring(&sql_buffer, val);
            luaL_addchar(&sql_buffer, '\'');
        } else {
            luaL_addstring(&sql_buffer, ",\'");
            luaL_addstring(&sql_buffer, val);
            luaL_addchar(&sql_buffer, '\'');
        }

        counter++;
    }
    luaL_addchar(&sql_buffer, ')');
    cel_link_destroy(&val_list, NULL);

    /* Check and append the onDuplicateKey of the query SQL */
    if ( onDuplicateKey != NULL ) {
        luaL_addstring(&sql_buffer, " ON DUPLICATE KEY ");
        luaL_addstring(&sql_buffer, onDuplicateKey);
    }

    /* Get the final query SQL for insertion */
    luaL_pushresult(&sql_buffer);
    query_sql = lua_tolstring(L, -1, &sql_len);
    if ( self->debug ) {
        printf("[Info]: add#sql=%s\n", query_sql);
    }

    /* Do the query SQL real execution */
    lua_pushboolean(L, mysql_real_query(
        self->my_conn, query_sql, sql_len) == 0 ? 1 : 0);

    return 1;
}

static int lua_mysql_batch_add(lua_State *L)
{
    lua_mysql_t *self;
    char *tbl_name = NULL, *field = NULL;
    char *key = NULL, *val = NULL, *nval = NULL;
    cel_link_t field_list;
    int counter, _type, args_num, stack_top, index;
    size_t vallen, new_len, sql_len;

    luaL_Buffer sql_buffer;
    const char *query_sql = NULL, *onDuplicateKey = NULL;
    cel_link_node_t *l_node;


    args_num = lua_gettop(L);
    luaL_argcheck(L, args_num >= 3, 1, "mysql object, table name and data expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    tbl_name = (char *) luaL_checkstring(L, 2);
    if ( args_num > 3 ) {
        onDuplicateKey = lua_tostring(L, 4);
    }

    /* Parse the data and create the Query SQL for insertion */
    luaL_buffinit(L, &sql_buffer);
    luaL_addstring(&sql_buffer, "INSERT INTO ");
    luaL_addstring(&sql_buffer, tbl_name);
    luaL_addchar(&sql_buffer, '(');

    /* get the field list */
    _type = lua_geti(L, 3, 1);
    if ( _type != LUA_TTABLE ) {
        luaL_error(L, "Invalid field value at index 1 table expected\n");
    }

    counter = 0;
    cel_link_init(&field_list);
    stack_top = lua_gettop(L);
    lua_pushnil(L);
    while ( lua_next(L, stack_top) != 0 ) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        if ( lua_type(L, -2) != LUA_TSTRING ) {
            luaL_error(L, "Invalid field "
                "type=%s, string expected\n", lua_typename(L, -2));
            continue;
        }

        key = (char *) lua_tostring(L, -2);
        if ( counter > 0 ) {
            luaL_addchar(&sql_buffer, ',');
            luaL_addstring(&sql_buffer, key);
        } else {
            luaL_addstring(&sql_buffer, key);
        }

        counter++;
        cel_link_add_last(&field_list, key);

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }
    luaL_addchar(&sql_buffer, ')');


    /* Append the value list */
    index = 0;
    luaL_addstring(&sql_buffer, " VALUES");
    lua_pushnil(L);
    while ( lua_next(L, 3) != 0 ) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        /* Push the value and get the field value */
        lua_pushvalue(L, -1);
        if ( index == 0 ) {
            luaL_addchar(&sql_buffer, '(');
        } else {
            luaL_addstring(&sql_buffer, ",(");
        }

        counter = 0;
        stack_top = lua_gettop(L);
        for ( l_node = field_list.head->_next; 
            l_node != field_list.tail; l_node = l_node->_next ) {
            field = (char *) l_node->value;

            /* Get and check the field value type */
            _type = lua_getfield(L, stack_top, field);
            switch ( _type ) {
            case LUA_TSTRING:
                val  = (char *)lua_tolstring(L, -1, &vallen);
                nval = sarah_malloc(sizeof(char) * (2 * vallen + 1));
                if ( nval == NULL ) {
                    luaL_error(L, "Fail to do the allocate");
                }

                new_len = mysql_real_escape_string(self->my_conn, nval, val, vallen);
                lua_pushlstring(L, nval, new_len);
                sarah_free(nval);
                val = (char *)lua_tostring(L, -1);
                lua_pop(L, 1);
                break;
            case LUA_TNUMBER:
                if ( lua_isinteger(L, -1) ) {
                    val = (char *)lua_pushfstring(L, "%I", lua_tointeger(L, -1));
                } else {
                    val = (char *)lua_pushfstring(L, "%f", lua_tonumber(L, -1));
                }
                lua_pop(L, 1);
                break;
            case LUA_TBOOLEAN:
                val = lua_toboolean(L, -1) ? "1" : "0";
                break;
            default:
                luaL_error(L, "Invalid value type=%s for field=%s", 
                    lua_typename(L, -1), (char *)lua_tostring(L, -2));
            }

            if ( counter == 0 ) {
                luaL_addchar(&sql_buffer, '\'');
                luaL_addstring(&sql_buffer, val);
                luaL_addchar(&sql_buffer, '\'');
            } else {
                luaL_addstring(&sql_buffer, ",\'");
                luaL_addstring(&sql_buffer, val);
                luaL_addchar(&sql_buffer, '\'');
            }

            counter++;
            lua_pop(L, 1);
        }

        luaL_addchar(&sql_buffer, ')');
        index++;
        lua_pop(L, 1);


        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }

    while ( cel_link_remove_first(&field_list) != NULL ) ;
    cel_link_destroy(&field_list, NULL);


    /* Check and append the onDuplicateKey of the query SQL */
    if ( onDuplicateKey != NULL ) {
        luaL_addstring(&sql_buffer, " ON DUPLICATE KEY ");
        luaL_addstring(&sql_buffer, onDuplicateKey);
    }

    /* Get the final query SQL for insertion */
    luaL_pushresult(&sql_buffer);
    query_sql = lua_tolstring(L, -1, &sql_len);
    if ( self->debug ) {
        printf("[Info]: batchAdd#sql=%s\n", query_sql);
    }

    /* Do the query SQL real execution */
    lua_pushboolean(L, mysql_real_query(
        self->my_conn, query_sql, sql_len) == 0 ? 1 : 0);

    return 1;
}

static int lua_mysql_count(lua_State *L)
{
    lua_mysql_t *self;
    char *tbl_name = NULL, *query_where = NULL, *query_group = NULL;

    luaL_Buffer sql_buffer;
    const char *query_sql = NULL;
    int args_num = lua_gettop(L);

    size_t sql_len;
    MYSQL_RES *my_res = NULL;
    MYSQL_ROW row;
    unsigned long *lengths = NULL;
    char buffer[21] = {'\0'};


    luaL_argcheck(L, args_num >= 2, 1, "mysql object and query where expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    tbl_name = (char *) luaL_checkstring(L, 2);

    if ( args_num > 2 ) {
        if ( lua_type(L, 3) == LUA_TSTRING ) {
            query_where = (char *) lua_tostring(L, 3);
        }

        if ( args_num > 3 && lua_type(L, 4) == LUA_TSTRING ) {
            query_group = (char *) lua_tostring(L, 4);
        }
    }

    luaL_buffinit(L, &sql_buffer);
    luaL_addstring(&sql_buffer, "SELECT count(*) FROM ");
    luaL_addstring(&sql_buffer, tbl_name);

    /* Check and append the query where and group */
    if ( query_where != NULL ) {
        luaL_addstring(&sql_buffer, " WHERE ");
        luaL_addstring(&sql_buffer, query_where);
    }

    if ( query_group != NULL ) {
        luaL_addstring(&sql_buffer, " GROUP BY ");
        luaL_addstring(&sql_buffer, query_group);
    }

    /* Get the final query SQL for count statistics  */
    luaL_pushresult(&sql_buffer);
    query_sql = lua_tolstring(L, -1, &sql_len);
    if ( self->debug ) {
        printf("[Info]: count#sql=%s\n", query_sql);
    }

    /* Do the SQL execution and analysis */
    if ( mysql_real_query(self->my_conn, query_sql, sql_len) != 0 ) {
        lua_pushinteger(L, -1);
        return 1;
    }

    if ( (my_res = mysql_store_result(self->my_conn)) == NULL ) {
        goto failed;
    }

    if ( (row = mysql_fetch_row(my_res)) == NULL ) {
        goto failed;
    }

    lengths = mysql_fetch_lengths(my_res);
    sprintf(buffer, "%.*s", (int) lengths[0], row[0]);
    lua_pushinteger(L, strtol(buffer, NULL, 10));
    mysql_free_result(my_res);
    return 1;

failed:
    lua_pushinteger(L, -1);
    mysql_free_result(my_res);

    return 1;
}

static int lua_mysql_get_row(lua_State *L)
{
    lua_mysql_t *self;
    int args_num = lua_gettop(L), num_cols = 0;
    unsigned int i, _type;

    const char *query_sql = NULL;
    size_t sql_len;
    MYSQL_RES *my_res = NULL;
    MYSQL_ROW row = NULL;
    unsigned long *lengths = NULL;
    MYSQL_FIELD *fields = NULL;
    char buffer[41] = {'\0'};


    luaL_argcheck(L, args_num >= 2, 1, "mysql object, query SQL expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    query_sql = (char *) luaL_checklstring(L, 2, &sql_len);

    if ( self->debug ) {
        printf("[Info]: getRow#sql=%s\n", query_sql);
    }

    /* Do the SQL execution and analysis */
    if ( mysql_real_query(self->my_conn, query_sql, sql_len) != 0 ) {
        goto failed;
    }

    my_res   = mysql_store_result(self->my_conn);
    num_cols = mysql_field_count(self->my_conn);
    if ( my_res == NULL ) {
        goto failed;
    }

    if ( (row = mysql_fetch_row(my_res)) == NULL ) {
        goto failed;
    }

    fields  = mysql_fetch_fields(my_res);
    lengths = mysql_fetch_lengths(my_res);

    lua_newtable(L);
    for ( i = 0; i < num_cols; i++ ) {
        _type = getAprilType(fields[i].type);
        switch ( _type ) {
        case APRIL_TYPE_STRING:
            lua_pushlstring(L, row[i], lengths[i]);
            break;
        case APRIL_TYPE_INTEGER:
            sprintf(buffer, "%.*s", (int) lengths[i], row[i]);
            lua_pushinteger(L, strtol(buffer, NULL, 10));
            break;
        case APRIL_TYPE_NUMBER:
            sprintf(buffer, "%.*s", (int) lengths[i], row[i]);
            lua_pushnumber(L, strtod(buffer, NULL));
            break;
        default:
            luaL_error(L, "Unsupported field type for field %s\n", fields[i].name);
            break;
        }
        
        // Add the field and value to the table
        lua_setfield(L, -2, fields[i].name);
    }

    mysql_free_result(my_res);
    return 1;

failed:
    lua_pushnil(L);
    mysql_free_result(my_res);

    return 1;
}

static int lua_mysql_get_list(lua_State *L)
{
    lua_mysql_t *self;
    int args_num = lua_gettop(L), num_cols = 0;
    unsigned int i, r, _type;

    const char *query_sql = NULL;
    size_t sql_len;
    MYSQL_RES *my_res = NULL;
    MYSQL_ROW row = NULL;
    unsigned long *lengths = NULL;
    MYSQL_FIELD *fields = NULL;
    char buffer[41] = {'\0'};


    luaL_argcheck(L, args_num >= 2, 1, "mysql object, query SQL expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    query_sql = (char *) luaL_checklstring(L, 2, &sql_len);

    if ( self->debug ) {
        printf("[Info]: getList#sql=%s\n", query_sql);
    }

    /* Do the SQL execution and analysis */
    if ( mysql_real_query(self->my_conn, query_sql, sql_len) != 0 ) {
        goto failed;
    }

    my_res   = mysql_store_result(self->my_conn);
    num_cols = mysql_field_count(self->my_conn);
    if ( my_res == NULL ) {
        goto failed;
    }

    fields = mysql_fetch_fields(my_res);
    lua_newtable(L);
    for ( r = 1; (row = mysql_fetch_row(my_res)) != NULL; r++ ) {
        lengths = mysql_fetch_lengths(my_res);
        /* Check and make the row table */
        lua_newtable(L);
        for ( i = 0; i < num_cols; i++ ) {
            _type = getAprilType(fields[i].type);
            switch ( _type ) {
            case APRIL_TYPE_STRING:
                lua_pushlstring(L, row[i], lengths[i]);
                break;
            case APRIL_TYPE_INTEGER:
                sprintf(buffer, "%.*s", (int) lengths[i], row[i]);
                lua_pushinteger(L, strtol(buffer, NULL, 10));
                break;
            case APRIL_TYPE_NUMBER:
                sprintf(buffer, "%.*s", (int) lengths[i], row[i]);
                lua_pushnumber(L, strtod(buffer, NULL));
                break;
            default:
                luaL_error(L, "Unsupported field type for field %s\n", fields[i].name);
                break;
            }
            
            // Add the field and value to the table
            lua_setfield(L, -2, fields[i].name);
        }

        lua_seti(L, -2, r);
    }

    mysql_free_result(my_res);
    return 1;

failed:
    lua_pushnil(L);
    mysql_free_result(my_res);

    return 1;
}

static int lua_mysql_delete(lua_State *L)
{
    lua_mysql_t *self;
    char *tbl_name = NULL, *del_cond = NULL;

    luaL_Buffer sql_buffer;
    const char *query_sql = NULL;
    size_t sql_len;

    luaL_argcheck(L, lua_gettop(L) >= 3, 1, "mysql object, table name and cond expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    tbl_name = (char *) luaL_checkstring(L, 2);
    del_cond = (char *) luaL_checkstring(L, 3);

    /* Make the query SQL */
    luaL_buffinit(L, &sql_buffer);
    luaL_addstring(&sql_buffer, "DELETE FROM ");
    luaL_addstring(&sql_buffer, tbl_name);
    luaL_addstring(&sql_buffer, " WHERE ");
    luaL_addstring(&sql_buffer, del_cond);

    /* Get the final query SQL for insertion */
    luaL_pushresult(&sql_buffer);
    query_sql = lua_tolstring(L, -1, &sql_len);
    if ( self->debug ) {
        printf("[Info]: delete#sql=%s\n", query_sql);
    }

    /* Do the query SQL real execution */
    lua_pushboolean(L, mysql_real_query(
        self->my_conn, query_sql, sql_len) == 0 ? 1 : 0);

    return 1;
}

static int lua_mysql_update(lua_State *L)
{
    lua_mysql_t *self;
    char *tbl_name = NULL, *upd_cond = NULL;
    char *key = NULL, *val = NULL, *nval = NULL;
    int _type, counter;

    luaL_Buffer sql_buffer;
    const char *query_sql = NULL;
    size_t vallen, new_len, sql_len;

    luaL_argcheck(L, lua_gettop(L) >= 4, 1, 
        "mysql object, table name, data and cond expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    tbl_name = (char *) luaL_checkstring(L, 2);
    upd_cond = (char *) luaL_checkstring(L, 4);

    luaL_buffinit(L, &sql_buffer);
    luaL_addstring(&sql_buffer, "UPDATE ");
    luaL_addstring(&sql_buffer, tbl_name);
    luaL_addstring(&sql_buffer, " SET ");

    /* Append the field list */
    counter = 0;
    lua_pushnil(L);
    while ( lua_next(L, 3) != 0 ) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
        if ( lua_type(L, -2) != LUA_TSTRING ) {
            luaL_error(L, "Invalid field "
                "type=%s, string expected\n", lua_typename(L, -2));
            continue;
        }

        key = (char *) lua_tostring(L, -2);
        if ( counter > 0 ) {
            luaL_addchar(&sql_buffer, ',');
        }

        luaL_addstring(&sql_buffer, key);
        luaL_addchar(&sql_buffer, '=');

        _type = lua_type(L, -1);
        switch ( _type ) {
        case LUA_TSTRING:
            val  = (char *)lua_tolstring(L, -1, &vallen);
            nval = sarah_malloc(sizeof(char) * (2 * vallen + 1));
            if ( nval == NULL ) {
                luaL_error(L, "Fail to do the allocate");
            }

            new_len = mysql_real_escape_string(self->my_conn, nval, val, vallen);
            lua_pushlstring(L, nval, new_len);
            sarah_free(nval);
            val = (char *)lua_tostring(L, -1);
            lua_pop(L, 1);
            break;
        case LUA_TNUMBER:
            if ( lua_isinteger(L, -1) ) {
                val = (char *)lua_pushfstring(L, "%I", lua_tointeger(L, -1));
            } else {
                val = (char *)lua_pushfstring(L, "%f", lua_tonumber(L, -1));
            }
            lua_pop(L, 1);
            break;
            break;
        case LUA_TBOOLEAN:
            val = lua_toboolean(L, -1) ? "1" : "0";
            break;
        default:
            luaL_error(L, "Invalid value type=%s for field=%s", 
                lua_typename(L, -1), (char *)lua_tostring(L, -2));
        }

        if ( _type == LUA_TSTRING ) {
            luaL_addchar(&sql_buffer, '\'');
        }

        luaL_addstring(&sql_buffer, val);
        if ( _type == LUA_TSTRING ) {
            luaL_addchar(&sql_buffer, '\'');
        }

        counter++;

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }

    /* Append the where (must needed) */
    luaL_addstring(&sql_buffer, " WHERE ");
    luaL_addstring(&sql_buffer, upd_cond);

    /* Get the final query SQL for insertion */
    luaL_pushresult(&sql_buffer);
    query_sql = lua_tolstring(L, -1, &sql_len);
    if ( self->debug ) {
        printf("[Info]: update#sql=%s\n", query_sql);
    }

    /* Do the query SQL real execution */
    lua_pushboolean(L, mysql_real_query(
        self->my_conn, query_sql, sql_len) == 0 ? 1 : 0);

    return 1;
}

static int lua_mysql_set_auto_commit(lua_State *L)
{
    lua_mysql_t *self;
    int autocommit;

    luaL_argcheck(L, lua_gettop(L) >= 2, 1, "mysql object and bool expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);
    autocommit = lua_toboolean(L, 2);

    lua_pushboolean(L, mysql_autocommit(self->my_conn, autocommit));
    return 1;
}

static int lua_mysql_commit(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);

    lua_pushboolean(L, mysql_commit(self->my_conn) == 0 ? 1 : 0);
    return 1;
}

static int lua_mysql_rollback(lua_State *L)
{
    lua_mysql_t *self;

    luaL_argcheck(L, lua_gettop(L) >= 1, 1, "mysql object expected");
    self = (lua_mysql_t *) luaL_checkudata(L, 1, L_METATABLE_NAME);
    assert_object_state(L, self);

    lua_pushboolean(L, mysql_rollback(self->my_conn) == 0 ? 1 : 0);
    return 1;
}


/* Mysql cursor method array */
static const struct luaL_Reg mysql_cursor_methods[] = {
    { "next",       lua_mysql_cursor_next },
    { "rows",       lua_mysql_cursor_rows },
    { "cols",       lua_mysql_cursor_cols },
    { "fields",     lua_mysql_cursor_fields },
    { "destroy",    lua_mysql_cursor_destroy },
    { "__len",      lua_mysql_cursor_rows },
    { "__gc",       lua_mysql_cursor_destroy },
    { NULL, NULL }
};


/* Mysql module method array */
static const struct luaL_Reg mysql_methods[] = {
    { "new",                lua_mysql_new },
    { "setDebug",           lua_mysql_set_debug },
    { "connect",            lua_mysql_connect },
    { "ping",               lua_mysql_ping },
    { "realEscape",         lua_mysql_real_escape_string },
    { "selectDb",           lua_mysql_select_db },
    { "query",              lua_mysql_query },
    { "insertId",           lua_mysql_insert_id },
    { "affectedRows",       lua_mysql_affected_rows },
    { "errno",              lua_mysql_errno },
    { "error",              lua_mysql_error },
    { "add",                lua_mysql_add },
    { "batchAdd",           lua_mysql_batch_add },
    { "count",              lua_mysql_count },
    { "getRow",             lua_mysql_get_row },
    { "getList",            lua_mysql_get_list },
    { "delete",             lua_mysql_delete },
    { "update",             lua_mysql_update },
    { "setAutoCommit",      lua_mysql_set_auto_commit },
    { "commit",             lua_mysql_commit },
    { "rollback",           lua_mysql_rollback },
    { "close",              lua_mysql_destroy },
    { "__gc",               lua_mysql_destroy },
    { NULL, NULL }
};


/**
 * register the current library to specified stack
 *
 * @param   L
 * @return  integer
*/
int luaopen_mysql(lua_State *L)
{
    /* 1, Register the leveldb metatable */
    luaL_newmetatable(L, L_CURSOR_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    /* Register the methods */
    luaL_setfuncs(L, mysql_cursor_methods, 0);


    /* 2, Register the mysql metatable */
    luaL_newmetatable(L, L_METATABLE_NAME);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    /* Register the methods */
    luaL_setfuncs(L, mysql_methods, 0);

    /* Register the fields */
    lua_pushinteger(L, MYSQL_VERSION_ID);
    lua_setfield(L, -2, "VERSION_ID");
    lua_pushliteral(L, MYSQL_BASE_VERSION);
    lua_setfield(L, -2, "BASE_VERSION");
    lua_pushliteral(L, MYSQL_SERVER_VERSION);
    lua_setfield(L, -2, "SERVER_VERSION");

    return 1;
}
