-- file module test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

-- copy the /etc/hosts as the target
local content = file_get_contents("/etc/hosts");
if ( content == nil ) then
    print("[Error]: Failed to get the content of /etc/hosts");
    return false;
end

local file = "/tmp/etc_hosts";
local status, errno = file_put_contents("/tmp/etc_hosts", content);
if ( status == false ) then
    print("[Error]: Failed to put the content to ", file, " with errno=", errno, "\n");
    return false;
end


-- open the file for read
--  "r" : read only, file should be exists
--  "w" : write only, create new file if not exists, clear the original content
--  "a" : append only, create new file if not exists
--  "r+": read & write, file should be exists
--  "w+": write & r+, create new file and clear the original content
--  "a+": append & r+, create new file and append content
--  For write only:
--  "t" : "Text file",
--  "b" : "Binary file"
local handler = fopen(file, "r+");
if ( handler == nil ) then
    print("Opend Failed with file=", file, "\n");
    return false;
else
    print(file, "Opend succeed\n");
end


-- lock the handler
if ( flock(handler, LOCK_SH) == false ) then
    print("Lock failed\n");
else
    print("Lock succeed\n");
end

-- read the content
print("\n------------fgets------------\n");
local line = fgets(handler, 1028);
while ( line ~= nil ) do
    print("line=", line)
    line = fgets(handler, 1028);
end
print("------------END------------\n\n");


-- tell the postion
local position = ftell(handler);
print("position=", position, "\n");


-- fseek
if ( fseek(handler, 10, SEEK_SET) == true ) then
    print("fseek successfully\n");
else
    print("fseek failef\n");
end

position = ftell(handler);
print("position=", position, "\n");

-- rewind
rewind(handler);
print("rewind successfully\n");
position = ftell(handler);
print("position=", position, "\n");

-- write a line to the content
print("\n------------fwrite------------\n");
if ( fwrite(handler, "This is the new line added\n", 1024) < 0 ) then
    print("Write failed\n");
else
    print("Write succeed\n");
end
print("------------END------------\n\n");


position = ftell(handler);
print("position=", position, "\n");


-- fread
rewind(handler);
print("\n------------fread------------\n");
line = fread(handler, 1028);
while ( line ~= nil ) do
    print(line);
    line = fread(handler, 1028);
end
print("------------END------------\n\n");


-- unlock the file
if ( flock(handler, LOCK_UN) == false ) then
    print("Unlock failed\n");
else
    print("Unlock succeed\n");
end

-- close the open handler
local close = fclose(handler);
if ( close == false ) then
    print("Close Failed\n");
else
    print("Closed succeed\n");
end
