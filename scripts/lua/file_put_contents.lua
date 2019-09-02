-- file put contents test
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local file = "/tmp/file_put_contents.log";
local status, errno = file_put_contents(file, "string", FILE_APPEND);
print("string, status=", status, ", errno=", errno, "\n");
status, errno = file_put_contents(file, 10, FILE_APPEND);
print("number, status=", status, ", errno=", errno, "\n");
status, errno = file_put_contents(file, true, FILE_APPEND);
print("true, status=", status, ", errno=", errno, "\n");
status, errno = file_put_contents(file, false, FILE_APPEND);
print("false, status=", status, ", errno=", errno, "\n");

local t = {
    name = "name",
    age = 18
};
status, errno = file_put_contents(file, t, FILE_APPEND);
print("table, status=", status, ", errno=", errno, "\n");

print("+------END------\n");
