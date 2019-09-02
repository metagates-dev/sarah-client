/**
 * april script interface define
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

#ifndef _APRIL_H
#define _APRIL_H

/**
 * check if the current table is an array or not.
*/
int table_is_array(lua_State *, int);

/**
 * convert the specifield table to a json object.
*/
void table_to_json(cJSON *, lua_State *, int);

/**
 * convert the specfield cJSON object ot lua table
*/
void cjson_to_table(cJSON *, int, lua_State *);


/**
 * basic std config for april script
 * like the package.path and the package.cpath eg ...
*/
void april_std_config(lua_State *);



/**
 * open and register the april std library module
*/
int luaopen_april(lua_State *);

/**
 * open and register the executor library of sarah
*/
int luaopen_executor(lua_State *);

#endif  /* end ifndef */
