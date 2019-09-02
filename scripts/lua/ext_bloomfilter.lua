-- bloom filter test script
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local bloomfilter = require('bloomfilter');
-- local bloomfilter = import('bloomfilter');

local filter = bloomfilter.new(100000, 0.0000001);
print("length=", filter:length(), "\n");
print("size=", filter:size(), "\n");
print("hsize=", filter:hsize(), "\n");

local to_add = {
    "Lion",
    "Rock",
    "Karl",
    "Micheal",
    "Erica",
    "Wendy",
    "YOYO",
    "Shulden",
    "Bond"
};

for _,v in ipairs(to_add) do
    print("+-Try to add item=", v, " ... ");
    if ( filter:add(v) == true ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end


local to_check = {
    "Lion",
    "Rock",
    "Karl",
    "Micheal",
    "Erica",
    "Wendy",
    "YOYO",
    "Shulden",
    "Bond",
    "Nick",
    "Cherly",
    "Member 01",
    "Member 02"
};

for _,v in ipairs(to_check) do
    print("+-Exists item=", v, " ? ");
    print(filter:exists(v), "\n");
end
