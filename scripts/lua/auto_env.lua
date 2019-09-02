-- env vars parse and set
-- @Author	grandhelmsman<desupport@grandhelmsman.com>
local textTpl = import('template.text');
local config = json_decode([[
{"exe_file":{"value":"go-ipfs","desc":"\u53ef\u6267\u884c\u7a0b\u5e8f\u540d\u79f0"},"bootup":{"value":true,"desc":"\u5f00\u673a\u542f\u52a8"},"monitor":{"value":true,"desc":"\u8fdb\u7a0b\u76d1\u6d4b"},"dynamic":{"value":false,"desc":"\u63a8\u9001\u8fdb\u7a0b\u52a8\u6001"},"base_dir":{"value":"\/opt\/Go-IPFS","desc":"\u5e94\u7528\u6839\u76ee\u5f55"},"log_dir":{"value":"\/data\/Go-IPFS\/log","desc":"\u65e5\u5fd7\u76ee\u5f55"},"data_dir":{"value":"\/data\/Go-IPFS\/data","desc":"\u6570\u636e\u76ee\u5f55"},"uninstall_rm_file":{"value":true,"desc":"\u5378\u8f7d\u65f6\u5220\u9664\u5b89\u88c5\u6587\u4ef6"},"uninstall_rm_log":{"value":false,"desc":"\u5378\u8f7d\u65f6\u662f\u5426\u5220\u9664\u65e5\u5fd7"},"uninstall_rm_data":{"value":false,"desc":"\u5378\u8f7d\u65f6\u662f\u5426\u5220\u9664\u6570\u636e(\u8c28\u614e)"},"app_init_cmd":{"value":"${base_dir}\/${exe_file} init","desc":"\u5e94\u7528\u521d\u59cb\u5316\u547d\u4ee4"},"app_boot_cmd":{"value":"${base_dir}\/${exe_file} daemon","desc":"\u5e94\u7528\u542f\u52a8\u547d\u4ee4"},"_env_IPFS_PATH":{"value":"${data_dir}","desc":"IPFS_PATH\u73af\u5883\u53d8\u91cf"},"_env_IPFS_HOME":{"value":"${data_dir}/home","desc":"IPFS_HOME\u73af\u5883\u53d8\u91cf"}}
]]);


for k, v in pairs(config) do
    if ( strlen(k) > 5 and strncmp(k, "_env_", 5) == 0 ) then
    	local key = string.sub(k, 6);
    	local val = textTpl.replaceVars(v["value"], config);
        print("setenv(", key, ")=", val, " ...");
        if ( setenv(key, val, 1) == true ) then
        	print(" --[Ok]\n");
        else
        	print(" --[Failed]\n");
        end
    end
end