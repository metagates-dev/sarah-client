-- lua type checking test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

local var_nil = nil;
local var_bool = true;
local var_function = function()
    print("I am a function\n");
end
local var_table = {
    name = "name",
    age = 18
};

local var_array = {
    "Apple",
    "Peach",
    "Orange"
};

local var_integer = 3;
local var_number = 3.14;
local var_string = "This is a string";

print("is_nil(var_nil): ", is_nil(var_nil), "\n");
print("is_bool(var_bool): ", is_bool(var_bool), "\n");
print("is_string(var_string): ", is_string(var_string), "\n");
print("is_number(var_number): ", is_number(var_number), "\n");
print("is_number(var_integer): ", is_number(var_integer), "\n");
print("is_integer(var_number): ", is_integer(var_number), "\n");
print("is_integer(var_integer): ", is_integer(var_integer), "\n");
print("is_table(var_table): ", is_table(var_table), "\n");
print("is_array(var_table): ", is_array(var_table), "\n");
print("is_table(var_array): ", is_table(var_array), "\n");
print("is_array(var_array): ", is_array(var_array), "\n");
print("is_function(var_function): ", is_function(var_function), "\n");
print("is_function(is_cfunction): ", is_function(is_cfunction), "\n");
print("is_cfunction(var_function): ", is_cfunction(var_function), "\n");
print("is_cfunction(is_cfunction): ", is_cfunction(is_cfunction), "\n");


local var_handler = fopen("/etc/hosts", "r");
print("is_userdata(var_handler): ", is_userdata(var_handler), "\n");
print("is_lightuserdata(var_handler): ", is_lightuserdata(var_handler), "\n");
fclose(var_handler);
