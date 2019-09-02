-- http download test demo
local zip_url = "http://dev.xjxh.io/sdk/linux/download/zip?app_key=d13a24676a6370bf58b7d74546f30bf0000000015c0e2689"
local zip_file = "/home/lionsoul/sarah-sdk.zip"

print("+-Try to download the resource to ", zip_file, " ... ");
local status, errno = http_download(zip_url, zip_file, false);
if ( status == true ) then
    print(" --Succeed\n");
else
    print(" --Failed with errno=", errno, "\n")
end
