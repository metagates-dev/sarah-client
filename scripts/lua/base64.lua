-- base64 function test program
-- @Author grandhelmsman<desupport@grandhelmsman.com>

local base64_sample = {
    {"abc", "YWJj"},
    {"123", "MTIz"},
    {"shift!@#", "c2hpZnQhQCM="},
    {"WTO is short for the World Trade Organization", "V1RPIGlzIHNob3J0IGZvciB0aGUgV29ybGQgVHJhZGUgT3JnYW5pemF0aW9u"}
};

local k = "";
local v = "";
local base64 = "";

print("base64 encode testing:\n");
for _,item in ipairs(base64_sample) do
    k = item[1];
    v = item[2];
    base64 = base64_encode(k);
    print("base64_encode(", k, ")=", base64, " ... ");
    if ( base64 == v ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end


print("base64 encode testing:\n");
for _,item in ipairs(base64_sample) do
    k = item[1];
    v = item[2];
    base64 = base64_decode(v);
    print("base64_decode(", v, ")=", base64, " ... ");
    if ( base64 == k ) then
        print(" --[Ok]\n");
    else
        print(" --[Failed]\n");
    end
end
