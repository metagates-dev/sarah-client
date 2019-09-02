-- mysql extension test and sample lua script
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local mysql = require("mysql");

print("+---- Table information: \n");
print("version_id=", mysql.VERSION_ID, "\n");
print("base version=", mysql.BASE_VERSION, "\n");
print("server version=", mysql.SERVER_VERSION, "\n");
print("\n");


-- create a new mysql object
local db = mysql.new();
db:setDebug(true);

-- connect to the specified remote database
local config = {
    host = "localhost",
    username = "root",
    password = "153759",
    database = "test",
    port = 3306,
    charset = 'utf8',
    unix_socket = "/var/run/mysqld/mysqld.sock",
    client_flag = 0
};
if ( db:connect(config) == false ) then
    print("Failed to connect to the database with errno=", 
        db:errno(), ", error=", db:error());
    return false;
end



-- @util interface
print("+---- Basic methods: \n");
local res = db:ping();
print("db:ping()=", res, "\n");
print("db:realEscape('abc')=", db:realEscape("'abc'"), "\n");
print("\n");

-- set the charset
print("+---- charset setting: \n");
local charset_sql = {
    "SET NAMES 'utf8'",
    "SET CHARACTER_SET_CLIENT = 'utf8'",
    "SET CHARACTER_SET_RESULTS = 'utf8'"
};
print("set the charset: \n");
for _,sql in ipairs(charset_sql) do
    res = db:query(sql);
    if ( res == false ) then
        print("db:query(", sql, ")=false, with error=", db:error(), "\n");
    else
        print("db:query(", sql, ")=true\n");
    end
end
print("\n");



-- @add
print("+---- Add operation: \n");
local line = {
    name  = "Lion",
    title = "Backend engineer",
    intro = [[Technology Enthusiasm Geek, Full Stack Developer. Web system architecture, NLP, Machine learning, God's son]],
    years = 0.5,
    created_at = time()
};
if ( db:add("member", line, "update created_at=" .. line.created_at) == false ) then
    print("Failed to do the add with errno=", db:errno(), ", error=", db:error(), "\n");
    return false;
end

print("insert_id: ", db:insertId(), "\n");
print("affectedRows: ", db:affectedRows(), "\n");
print("\n");



-- @batch insert
print("+---- Batch add operation: \n");
local c_time = time();
local list = {
    {
        name = "Rock",
        title = "Backend engineer",
        intro = "Whatever ...",
        years = 0.5,
        created_at = c_time,
    },
    {
        name = "Wendy",
        title = "Frontend engineer",
        intro = "Whatever ... ",
        years = 0.5,
        created_at = c_time
    },
    {
        name = "YOYO",
        title = "Product manager",
        intro = "Whatever ... ",
        years = 0.5,
        created_at = c_time
    }
};
if ( db:batchAdd("member", list, "update created_at=values(created_at)") == false ) then
    print("Failed to do the batch add with errno=", 
        db:errno(), ", error=", db:error(), "\n");
    return false;
end

print("affectedRows: ", db:affectedRows(), "\n");
print("\n");



print("+---- Basic data stat: \n");
local total = db:count("member");
local total_group = db:count("member", nil, "years");
local total_where = db:count("member", "name = 'Lion'");
print("total=", total, ", total_group=", total_group, ", total_where=", total_where, "\n");
print("\n");



-- @query
print("+---- Basic data query: \n");
local ret = db:query("select * from member order by Id desc");
if ( ret == false ) then
    print("Failed to do the query with errno=", db:errno(), ", error=", db:error(), "\n");
    return false;
end

print("cols:", ret:cols(), ", rows: ", ret:rows(), ", #ret=", #ret, "\n");
print("fields: ", ret:fields(), "\n");
print("data list: ", "\n");
local row = ret:next();
while ( row ~= nil ) do
    print(row, "\n");
    row = ret:next();
end
print("\n");



-- @getRow
print("+---- GetRow operation: \n");
local row = db:getRow("select Id,name from member order by Id asc limit 0,1");
print("getRow()=", row, "\n");
print("\n");



-- @update
print("+---- Update operation: \n");
local update = {
    years = 0.6,
    updated_at = time()
};
if ( db:update("member", update, "name = 'Lion'") == false ) then
    print("Failed to do the update with errno=", 
        db:errno(), ", error=", db:error(), "\n");
    return false;
end

print("affectedRows: ", db:affectedRows(), "\n");
print("\n");



--- @getList
print("+---- GetList operation: \n");
local list = db:getList("select Id,name,years from member order by Id asc");
print("getList()=", list, "\n");
print("\n");



--- @transaction
print("+---- Transaction operation: \n");
db:setAutoCommit(false);
local q1 = db:query("select Id,name from member where name = 'Lion'");
local q2 = db:query("select Id,name from member where name = 'Rock'");
if ( q1 == nil or q2 == nil ) then
    db:rollback();
    print("Failed do the query operation\n");
    return false;
end

if ( db:commit() == false ) then
    print("Failed do the commit\n");
    return false;
end
print("transaction query finished\n");
print("\n");




-- @delete
print("+---- Delete operation: \n");
if ( db:delete("member", "Id > 100") == false ) then
    print("Failed to do the deletion with errno=", 
        db:errno(), ", error=", db:error(), "\n");
    return false;
end

print("affectedRows: ", db:affectedRows(), "\n");
print("\n");



-- @close the connection
db:close();
