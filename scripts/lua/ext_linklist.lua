-- linklist test program
-- @Author grandhelmsman<desupport@grandhelmsman.com>

local linklist = require('linklist');
local mixed_values = {
    "string",
    1,
    3.1415,
    666,
    2.14568,
    256,
    true,
    false,
    "lead to",
    "pat on the back",
    "by any chance",
    {
        name = "Lion",
        age = 29
    },
    -- function(str) 
    --     print(str, "\n");
    -- end,
    -- linklist
};

local list = linklist.new();

print("+--- Add elements testing: \n");
for _,v in ipairs(mixed_values) do
    print("+-Try to add ", v , " ... ");
    list:add(v);
    print(" --[Done]\n");
end
print("list.size=", list:size(), "\n");
print("\n");


print("+--- Insert elements testing: \n");
list:addFirst("This string is from list:addFirst");
list:insert(1, "this string is from list:insert");
print("list.size=", list:size(), "\n\n");


print("+--- Iterator testing: \n");
local it = list:iterator();
while ( it:hasNext() ) do
    local v = it:next();
    print("type=", type(v), ", val=", v, "\n");
end
print("\n");


print("+--- Get elements testing: \n");
print("list:get(1)=", list:get(1), "\n");
print("list:get(-1)=", list:get(-1), "\n");
print("list:get(-2)=", list:get(-2), "\n");
for i = 1, #list, 1 do
-- for i = 1, list:size(), 1 do
    print("list:get(", i, ")=", list:get(i), "\n");
end
print("\n");


print("+--- remove elements testing: \n");
print("list:remove(1)=", list:remove(1), "\n");
print("list:remove(-1)=", list:remove(-1), "\n");
print("list:removeLast()=", list:removeLast(), "\n");
print("\n");


print("+--- Iterator rewind testing: \n");
it:rewind();
while ( it:hasNext() ) do
    local v = it:next();
    print("v=", v, "\n");
end
print("\n");

print("+--- remove elements testing: \n");
while ( true ) do
    local v = list:removeFirst();
    if ( v == nil ) then
        break;
    end
    print("v=", v, "\n");
end
print("list.size=", list:size(), "\n");
