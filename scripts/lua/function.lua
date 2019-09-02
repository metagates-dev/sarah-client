-- lua function test
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local file = "/home/lionsoul/examples.desktop";
local dir  = "/opt";
local str = "  This is a String  ";

print("+------ profile ------+\n");
print("file=", file, "\n");
print("dir=", dir, "\n");
print("str=", str, "\n");
print("\n");

print("time=", time(), "\n");
print("md5(abc)=", md5("abc"), "\n");
print("md5_file(/home/lionsoul/examples.desktop)=", md5_file(file), "\n");

-- print("ltrim(str)=", ltrim(str), "\n");
-- print("rtrim(str)=", rtrim(str), "\n");
print("trim(str)=", trim(str), "\n");
print("statvfs(dir)=", statvfs(dir), "\n");
print("stat(file)=", stat(file), "\n");
print("lstat(file)=", stat(file), "\n");
print("basename(file)=", basename(file), "\n");
print("dirname(file)=", dirname(file), "\n");
