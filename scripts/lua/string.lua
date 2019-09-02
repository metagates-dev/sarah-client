-- string function test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>


local str1 = "This Is String 1";
local str2 = "This Is String 2";

print("str1=", str1, "\n");
print("str2=", str2, "\n");
print("strlen(str1)=", strlen(str1), "\n");
print("strlen(str2)=", strlen(str2), "\n");
print("strtolower(str1)=", strtolower(str1), "\n");
print("strtolower(str2)=", strtolower(str2), "\n");
print("strtoupper(str1)=", strtoupper(str1), "\n");
print("strtoupper(str2)=", strtoupper(str2), "\n");
print("str1=", str1, "\n");
print("str2=", str2, "\n");
print("strcmp(str1, str2)=", strcmp(str1, str2), "\n");
print("strncmp(str1, str2, 15)=", strncmp(str1, str2, 15), "\n");

print("\n----New Function Testing...\n");
print("charat(str1, 1)=", charat(str1, 1), "\n");
print("charat(str1, 2)=", charat(str1, 2), "\n");
print("strchr(str1, I)=", strchr(str1, "I"), "\n");
print("strstr(str1, STR)=", strstr(str1, "STR"), "\n");
print("strpos(str1, STR)=", strpos(str1, "STR"), "\n");
