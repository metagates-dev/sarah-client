-- ip2region extension test program
local ip2region = require "ip2region";
-- local ip2region = import("ip2region");
local searcher = ip2region.new(_LIB_DIR .. "ip2region.db");

local data;
-- data = searcher:binarySearch("101.233.153.103");
-- data = searcher:btreeSearch("101.233.153.103");
data = searcher:memorySearch("101.233.153.103");
print("city_id=", data.city_id, "region=", data.region);
