# sarah host node monitor for Linux ONLY


### 1, How to install
@Note: Do this only if you failed to compile&link the sarah since we've already track the libcurl.a into the libraries
1. install openssl
```shell
# https://blog.csdn.net/yejinxiong001/article/details/77745943
sudo apt-get install openssl
sudo apt-get install libssl-dev
```
2. configure server url
```shell
# Edit ${SARAH_ROOT}/include/sarah.h

/** http api url constants define */
#define updater_version_url "http://sarah.grandhelmsman.com/sdk/linux/download/version"
#define updater_download_url "http://sarah.grandhelmsman.com/sdk/linux/download/bin"
#define executor_sync_url "http://sarah.grandhelmsman.com/api/node/command/export/notify"
#define executor_pull_url "http://sarah.grandhelmsman.com/api/node/command/export/pull"
#define collector_post_url "http://sarah.grandhelmsman.com/api/collector/post"

// lua executor interface define
/* LUA package path base directory */
#define APRIL_PACKAGE_BASE_URL	"http://lib.grandhelmsman.com/"
#define APP_MIRROR_BASE_URL 	"http://mirrors.grandhelmsman.com/"
#define APRIL_PACKAGE_BASE_DIR 	"/usr/local/lib/april/"
#define SARAH_LIB_BASE_DIR 		"/usr/local/lib/"
```

3. install debug sarah
```shell
cd ${SARAH_ROOT}
make debug
cd ./build/debug/
sudo ./install.sh
```

4. install release sarah
```shell
cd ${SARAH_ROOT}
make release
cd .build/release/
sudo ./install.sh
```



### 2, How to start
1. command line start up
```shell
sudo sarah-booter > /dev/null &
```

2. boot autostart
```
A autostart script has been copied to the /etc/init.d/ directory
So, i will automatically started during system booting.
```

3. service manager
```
sudo service sarah {start|stop|restart}
```



### 3, How to uninstall
1. uninstall sarah
```
cd ${SARAH_ROOT}/build/(debug|release)/
sudo ./uninstall.sh
```


### 4, Release package generation
```
cd ${SARAH_ROOT}/
make release
```



### 5, Node store Binary Package
local file binary format:
1. All in local byte order - little-endian byte order
```
+------------+-----------+--------------+----------+
| created_at | interval  | last_boot_at | node_id  |
+------------+-----------+--------------+----------+
  4 bytes      4 bytes        4 bytes    40 bytes
```

2. unique id format: 
```
 p1  p2  p3  p4  p5
%8s-%8s-%4s-%4s-%12s
5bffbb2b-aa737dc0-623e-9001-4cedfbbd1eb9

p1: unix timestamp seconds
p2: unix timestamp u seconds
p3: random string
p4: random string, the last byte of this short indicate the algorithm of the generation
p5: p4[1] == 1 ? mac_addr[6] : random short + random integer
```



### 6, Collector JSON Package
```json
{
    "version": 1.0,     // client version information
    "node": {
        "id": "5bffbb2b-aa737dc0-623e-9001-4cedfbbd1eb9",
        "created_at": 1542702013,
        "last_boot_at": 1542702013,
        "interval": 180,            // statistics interval
        "last_stat_begin": 1542702090,
        "last_stat_end": 1542702013,
        "storage_media": "/etc/sarah/node.db"
    },
    "cpu": {
        "list": [   // CPU info array
            {
                "vendor_id": "GenuineIntel",
                "model": 568,
                "model_name": "Intel(R) Core(TM) i5-8400 CPU @ 2.80GHz",
                "mhz": 800.093,
                "cache_size": 9216,
                "physical_id": 0,
                "cores": 6
            }
        ],
        "stat": {   // CPU statistics object
            "round_1": {
                "name": "CPU",
                "user": 120,
                "nice": 1020,
                "system": 11292,
                "idle": 123
            },
            "round_2": {
                "name": "CPU",
                "user": 120,
                "nice": 1020,
                "system": 11292,
                "idle": 123
            },
            "load_avg": 45.1,
            "sys_loadavg": {        // system load average statistics from /proc/loadavg
                "t_1m": 0.12,
                "t_5m": 0.20,
                "t_15m": 0.64
            }
        }
    },
    "ram": {        // RAM statistics object
        "total": 8008844,
        "free": 3260488,
        "available": 5869412
    },
    "disk": [       // DISK statistics array
        {
            "wwn": "0x5000cca099c3dc1d",
            "serial_no": "VAG8H6XL",
            "model": "WDC WD80EFAX-68KNBN0",
            "name": "/dev/sdd2",
            "pt_no": 2,
            "format": "",
            "mount": "",
            "p_start": 32768,
            "p_blocks": 15628017664,
            "p_size": 8001545043968,
            "inode": 0,
            "ifree": 0,
            "size": 0,
            "free": 0
        }
    ],
    "network": [    // NETWORK statistics array
        {
            "name": "enp2s0",
            "hd_addr": "4c:ed:fb:bd:1e:b9",
            "round_1": {
                "receive": 362655738,
                "r_packets": 318600,
                "transmit": 19846864,
                "t_packets": 120170,
            },
            "round_2": {
                "receive": 362655738,
                "r_packets": 318600,
                "transmit": 19846864,
                "t_packets": 120170,
            },
            "incoming": 34598,
            "outgoing": 2765
        }
    },
    "system": {     // system information
        "pre_def": "Linux",     // self predefined system name
        "sys_name": "Linux",    // system name, like, Linux, Windows
        "node_name": "xjxh-PowerEdge-T110-II",  // node name, the hostname of the system
        "release": "4.15.0-39-generic",         // kernel release version
        "version": "#42-Ubuntu SMP Tue Oct 23 15:48:01 UTC 2018",   // release version
        "machine": "x86_64"     // machine serial
    },
    "process": [    // process list
        {
            "user": "root",
            "pid": 1,
            "cpu": 0,
            "mem": 0.10000000149012,
            "vsz": 225776,
            "rss": 9684,
            "tty": "?",
            "stat": "Ss",
            "start": "11月30",
            "time": "1:34",
            "cmd": "/sbin/init splash"
        }
    ]
}
```


### 7, Remote command JSON package
```json
{
    "id": 1545796230,
    "created_at": 1545796230,
    "engine": "string: local VM engine, could be bash|lua|php|js",
    "cmd_text": "string: command content, a single command or a block of logic",
    "sign": "3c6006f4daa7ba5ffd39e271f05e9e33",
    "sync_mask": 9
}
```
