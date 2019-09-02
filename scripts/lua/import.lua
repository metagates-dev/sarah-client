-- lua remote import function test
-- @Author  grandhelmsman<desupport@grandhelmsman.com>


-- complex module name tested -- passed
local iptest = import("Ip2region.Test");
print("data=", iptest:search("183.14.132.246"), "\n");


-- duplicate import tested -- passed
-- It is ok to run the moudle/init.lua for more than one time
-- in a same stack.
local Ip2region = import("Ip2region");
local handler = Ip2region.new(_LIB_DIR .. "ip2region.db");
local data = handler:btreeSearch("119.23.187.68");
print("data=", data, "\n");


-- import error handling
local importerr = import("NotExistsModule");


-- make sure the import error will terminal the execution
print("This is now the end\n");
