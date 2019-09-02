-- lua env test program
-- @Author  grandhelmsman<desupport@grandhelmsman.com>

print("+---getenv: \n");
print("getenv(PATH)=", getenv("PATH"), "\n");
print("getenv(JAVA_HOME)=", getenv("JAVA_HOME"), "\n");
print("getenv(IPFS_PATH)=", getenv("IPFS_PATH"), "\n");

print("+---setenv(IPFS_PATH): ");
if ( setenv("IPFS_PATH", "/data/Go-IPFS/data", 1) == false ) then
    print(" --[Failed]\n");
else
    print(" --[Ok]\n");
end

print("getenv(IPFS_PATH)=", getenv("IPFS_PATH"), "\n");


print("\n+---shell env operation:\n");
-- os.execute("echo $PATH");
-- os.execute("export IPFS_PATH=/data/Go-IPFS/data PATH=$PATH:/opt/Go-IPFS/");
os.execute("echo $IPFS_PATH $PATH");
-- os.execute("/opt/Go-IPFS/go-ipfs init");
print("getenv(IPFS_PATH)=", getenv("IPFS_PATH"), "\n");



-- local pid = fork();
-- if ( pid < 0 ) then
--     print("Fail to do the fork");
-- elseif ( pid == 0 ) then
--     exec("/opt/Go-IPFS/go-ipfs", {"daemon"});
-- else
--     print("i am the parent process with pid=", getpid(), "\n");
-- end
