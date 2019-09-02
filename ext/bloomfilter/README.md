# Lua ip2region c module binding


### 一，如何使用
* 1, 先现在ip2region扩展
* 2, 通过如下流程在你的lua程序中使用
```lua
-- 包含模块
local Ip2region = require "Ip2region";

-- 创建查询对象
-- 设置ip2region.db的文件地址，dbFile表示ip2region.db数据库文件的地址
-- 注意new方法是通过“.”调用，而不是“:”
local ip2region = Ip2region.new("ip2region.db file path");

local data;

-- 查询，备注，尽量请使用“:”调用方法，使用“.”需要主动传递ip2region对象参数
-- 1，binary查询
data = ip2region:binarySearch("101.233.153.103");

-- 2，btree查询
data = ip2region:btreeSearch("101.233.153.103");

-- 3，memory查询
data = ip2region:memorySearch("101.233.153.103");

-- 返回结果如下
print("city_id=", data.city_id, "region=", data.region);
```