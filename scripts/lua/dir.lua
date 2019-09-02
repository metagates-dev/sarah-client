-- dir function test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local dir = "/usr/local/lib";
print("dir=", dir, "\n");
print("is_dir=");
if ( is_dir(dir) ) then
    print("true\n");
else
    print("false\n");
end


-- open the dir
local handler = opendir(dir);
if ( handler == nil ) then
    print("[Error]: Failed to opendir\n");
    return false;
end

-- read the dir
print("\n------------readdir------------\n");
local file = readdir(handler);
while ( file ~= nil ) do
    if ( file == '.' or file == '..' ) then
    else
        print("file=", file, ", ");
        print("is_dir=");
        if ( is_dir(dir .. "/" .. file) ) then
            print("true\n");
        else
            print("false\n");
        end
    end

    file = readdir(handler);
end
print("------------END------------\n");


-- close the dir
closedir(handler);
