-- hash function test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local hashfunc = require('hashfunc');
-- local hashfunc = import('hashfunc');

print("bkdr(abc)=", hashfunc.bkdr("abc"), "\n");
print("bobJenkins32(abc)=", hashfunc.bobJenkins32("abc"), "\n");
print("murmur32(abc)=", hashfunc.murmur32("abc"), "\n");
