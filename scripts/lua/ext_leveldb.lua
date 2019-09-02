                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           -- leveldb extension test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local leveldb = require('leveldb');

local data_sets = {
    {"string", "this is a string value"},
    {"float", 3.1415},
    {"integer", 1234},
    {"boolean", true},
    {"table", {
        name = "SarahOS",
        year = 0.5,
        tag  = {"cloud", "distributed", "storage", "computing"}
    }}
};


print("MAJOR_VERSION=", leveldb.MAJOR_VERSION, "\n");
print("MINOR_VERSION=", leveldb.MINOR_VERSION, "\n");
print("\n");


-- create a new read option instance
local readOption = leveldb.createReadOption({
    verify_checksums = false,
    fill_cache = true
});

-- create a new write option instance
local writeOption = leveldb.createWriteOption({
    sync = false
});

local db = leveldb.new("/tmp/testdb", {
    create_if_missing = true,
    error_if_exists = false,
    paranoid_checks = false,
    -- write_buffer_size = 4 * 1024 * 1024,
    max_open_files = 1024,
    bloomfilter_bits = 10,
    compression = leveldb.SNAPPY_COMPRESSION,
    -- read_option = readOption,
    -- write_option = writeOption
});


db:setReadOption(readOption);
db:setWriteOption(writeOption);

local key = "";
local val = "";
local ret = false;

print("+---Test add ---+\n");
for _,v in ipairs(data_sets) do
    key = v[1];
    val = v[2];
    add = db:put(key, val);
    print("Try to add(", key, ", ", val, ") ... ");
    if ( add == true ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end


print("\n+---Test get ---+\n");
for _, v in ipairs(data_sets) do
    key = v[1];
    val = db:get(key);
    print("Try to get(", key, ")=", val);
    if ( type(val) == "table" or val == v[2] ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end


print("\n+---Test iterator ---+\n");
local it = db:iterator();
while ( it:valid() ) do
    key = it:key();
    val = it:value();
    print(type(val), ": ", key, "=", val, "\n");
    it:next();
end


print("\n+---Test add again ---+\n");
db:put("new_key", "value of the new_key");
print("get(new_key)=", db:get("new_key"), "\n");


print("\n+---Test iterator ---+\n");
it:seekToFirst();
while ( it:valid() ) do
    key = it:key();
    val = it:value();
    print(type(val), ": ", key, "=", val, "\n");
    it:next();
end


print("\n+---Test writebatch ---+\n");
local batch = leveldb.createWriteBatch();
batch:put("batch_key_01", "value of the batch_key_01");
batch:put("batch_key_02", "value of the batch_key_02");
batch:put("batch_key_03", {"Apple", "Orange", "Melon", "Peach"});
batch:put("batch_key_04", 608);
batch:delete("float");
print("write(batch)=", db:write(batch), "\n");


print("\n+---Test iterator refresh ---+\n");
--nit:seekToFirst();
it = db:iterator();
while ( it:valid() ) do
    key = it:key();
    val = it:value();
    print(type(val), ": ", key, "=", val, "\n");
    it:next();
end



print("\n+---Test assert get ---+\n");
table.insert(data_sets, {"new_key", ""});
table.insert(data_sets, {"batch_key_01", ""});
table.insert(data_sets, {"batch_key_02", ""});
table.insert(data_sets, {"batch_key_03", ""});
table.insert(data_sets, {"batch_key_04", ""});
for _, v in ipairs(data_sets) do
    key = v[1];
    val = db:get(key);
    print("Try to get(", key, ")=", val, "\n");
end


print("\n+---Test delete ---+\n");
for _, v in ipairs(data_sets) do
    key = v[1];
    ret = db:delete(key);
    print("Try to delete(", key, ") ... ");
    if ( ret == true ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end


print("\n+---Test assert get ---+\n");
for _, v in ipairs(data_sets) do
    key = v[1];
    val = db:get(key);
    print("Try to get(", key, ")=", val, "\n");
end

-- destroy the database
db:destroy();
