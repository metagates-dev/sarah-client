-- fork and exec test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>
-- @date    2019/03/14

local pid = fork();
if ( pid < 0 ) then
    print("Fail to do the fork");
elseif ( pid == 0 ) then
    -- exec("/bin/ls", {
    --     "-l",
    --     "/opt"
    -- });
    while ( true ) do end;
else
    print("i am the parent process with pid=", getpid(), "\n");
end
