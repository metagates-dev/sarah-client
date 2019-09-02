/**
 * April interface implementations.
 *
 * @Author	grandhelmsman<desupport@grandhelmsman.com>
*/


#include "april.h"
#include "sarah.h"
#include "sarah_utils.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

int table_is_array(lua_State *L, int index)
{
    int k_type, is_array = 1;

    /* Loop the current table to detect it is a Object or an Arry */
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        k_type = lua_type(L, -2);
        if ( k_type == LUA_TSTRING ) {
            is_array = 0;
            lua_pop(L, 2);
            break;
        }

        lua_pop(L, 1);
    }

    return is_array;
}

void table_to_json(cJSON *json, lua_State *L, int index)
{
    int k_type, v_type, top;
    char *key = NULL;
    cJSON *value = NULL;

    lua_pushnil(L);  /* first key */
    while (lua_next(L, index) != 0) {
        /* uses 'key' (at index -2) and 'value' (at index -1) */
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
        case LUA_TTABLE:
            top = lua_gettop(L);
            value = table_is_array(L, top) ? cJSON_CreateArray() : cJSON_CreateObject();
            table_to_json(value, L, top);
            break;
        case LUA_TNUMBER:
            value = cJSON_CreateNumber(lua_tonumber(L, -1));
            break;
        case LUA_TBOOLEAN:
            value = cJSON_CreateBool(lua_toboolean(L, -1));
            break;
        case LUA_TSTRING:
            value = cJSON_CreateString(lua_tostring(L, -1));
            break;
        case LUA_TNIL:
            value = cJSON_CreateNull();
            break;
        }

        if ( json->type == cJSON_Array ) {
            cJSON_AddItemToArray(json, value);
        } else {
            cJSON_AddItemToObject(json, key, value);
        }

        /* removes 'value'; keeps 'key' for next iteration */
        lua_pop(L, 1);
    }
}


void cjson_to_table(cJSON *item, int ptype, lua_State *L)
{
    register int index = 1;

    while ( item != NULL ) {
        switch (item->type) {
        case cJSON_False:
            lua_pushboolean(L, 0);
            break;
        case cJSON_True:
            lua_pushboolean(L, 1);
            break;
        case cJSON_NULL:
            lua_pushnil(L);
            break;
        case cJSON_Number:
            if ( item->valuedouble > item->valueint ) {
                lua_pushnumber(L, item->valuedouble);
            } else {
                lua_pushinteger(L, item->valueint);
            }
            break;
        case cJSON_String:
            lua_pushstring(L, item->valuestring);
            break;
        case cJSON_Array:
            lua_newtable(L);
            cjson_to_table(item->child, cJSON_Array, L);
            break;
        case cJSON_Object:
            lua_newtable(L);
            cjson_to_table(item->child, cJSON_Object, L);
            break;
        case cJSON_Raw:
            break;
        }

        if ( ptype == cJSON_Array || item->string == NULL ) {
            lua_seti(L, -2, index++);
        } else {
            lua_setfield(L, -2, item->string);
        }

        item = item->next;
    }
}

void april_std_config(lua_State *L)
{
    if ( lua_getglobal(L, "package") == LUA_TTABLE ) {
        lua_pushliteral(L, APRIL_PACKAGE_BASE_DIR "?.lua");
        lua_setfield(L, -2, "path");
        lua_pushliteral(L, APRIL_PACKAGE_BASE_DIR "?.so");
        lua_setfield(L, -2, "cpath");
    }
    
    lua_pop(L, 1);
}
