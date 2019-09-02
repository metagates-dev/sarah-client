/**
 * sarah common header file
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#ifndef _sarah_h
#define _sarah_h

#define _sarah_version 1.57
#define sarah_version() _sarah_version


#if ( defined(WIN32) || defined(_WIN32) || defined(__WINDOWS_) || defined(WINNT) )
#   define SARAH_API     extern __declspec(dllexport)
#   define SARAH_STATIC  static
#   define SARAH_WINDOWS
#elif ( defined(linux) || defined(_UNIX) ) 
#   define SARAH_API     extern
#   define SARAH_STATIC  static inline
#   define SARAH_LINUX
#endif


#define sarah_calloc( _bytes, _blocks )   calloc( _bytes, _blocks )
#define sarah_malloc( _bytes )            malloc( _bytes )
#define sarah_free( _ptr )                free( _ptr )

// memory allocation error
#define SARAH_ALLOCATE_ERROR(func, bytes)    \
do { \
    printf("<SARAH>: Allocate Error In Function <%s> For %lu Bytes.\n", func, (unsigned long int) bytes);    \
    return NULL; \
} while (0);



/**
 * @Note: this feature started since 2019/04/01
 * since we try to provide a SDK for a pluggable device like a U disk.
 * And the SDK could be started ONLY when the pluggable device is connected.
 *
 * the Micro var define will be passed from the Makefile 
 *  with -D _SARAH_GHOST_UDISK_ENABLE and -D _SARAH_ASSERT_PARENT_PROCESS
*/
// #define _SARAH_GHOST_UDISK_ENABLE
// #define _SARAH_ASSERT_PARENT_PROCESS

#define _SARAH_PPID_ASSERT_ERR(app) printf("[Error]: %s will now exit CUZ of ppid changed\n", app)
#define _SARAH_STOP_WARNNING(app) printf("[Warning]: %s stoped, waiting for the CONT signal.\n", app);


#include "sarah_utils.h"
#include "cJSON.h"
#include "cJSON_Utils.h"
#include <sys/utsname.h>


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

/// NODE entry define
#define sarah_node_id_size 41
#define sarah_node_interval 30
typedef struct utsname sarah_utsname_t;
struct sarah_node_config_entry
{
    char *build;                /* build version, could be debug or release */
    char *app_key;              /* sarah platform app_key */

    int disk_auto_partition;
    char *disk_mount_basedir;
    char *disk_mount_prefix;
    char *disk_mount_algorithm;
};
typedef struct sarah_node_config_entry sarah_node_config_t;

struct sarah_node_entry
{
    char *base_dir;                 // base directory
    char *db_file;                  // local db file
    char *config_file;              // local config file
    int is_new;                     // is the node new ? local file exists or not
    unsigned long long int reg_id;  // node global unique register id
    unsigned int created_at;        // create timestamp
    unsigned int last_boot_at;      // last boot timestamp
    unsigned int interval;          // gathering interval in seconds
    unsigned int last_stat_begin;   // last statistics begin timestamp
    unsigned int last_stat_end;     // last statistics end timestamp

    sarah_node_config_t config;     // sarah node config instance;

    /*
     * struct utsname {
     *  char sysname[];    // Operating system name (e.g., "Linux")
     *  char nodename[];   // Name within "some implementation-defined network"
     *  char release[];    // Operating system release (e.g., "2.6.28")
     *  char version[];    // Operating system version
     *  char machine[];    // Hardware identifier
     *  #ifdef _GNU_SOURCE
     *      char domainname[];  // NIS or YP domain name
     *  #endif
     * };
    */
    sarah_utsname_t kernel_info;        // kernel information
    char node_id[sarah_node_id_size];   // node id
};
typedef struct sarah_node_entry sarah_node_t;

/*
 * initialize the node from a specified node local base directory
 * the specifield file will be created if it not exists yet..
*/
SARAH_API int sarah_node_init(sarah_node_t *, char *);

// it will not remove the local file
// except set the code to 1 then it will clean up the disk file
SARAH_API void sarah_node_destroy(sarah_node_t *, int);

// flush the node to a specified local file
SARAH_API int sarah_node_flush(sarah_node_t *);
SARAH_API int sarah_node_set_created_at(sarah_node_t *, unsigned int, int);
SARAH_API int sarah_node_set_interval(sarah_node_t *, unsigned int, int);
SARAH_API int sarah_node_set_last_boot_at(sarah_node_t *, unsigned int, int);

SARAH_API void sarah_print_node(sarah_node_t *);

// About node id generation mode
SARAH_API void sarah_gen_node_id(char *);



/* hardware information entry and interface */
struct sarah_hd_bios_entry
{
    char vendor[40];
    char version[64];
    char release_date[12];
    unsigned int runtime_size;
    unsigned int rom_size;
};
typedef struct sarah_hd_bios_entry sarah_hd_bios_t;
SARAH_API int sarah_hd_bios_init(sarah_hd_bios_t *);
SARAH_API void sarah_hd_bios_destroy(sarah_hd_bios_t *);
SARAH_API void sarah_print_hd_bios(sarah_hd_bios_t *);

struct sarah_hd_system_entry
{
    char manufacturer[40];
    char product_name[24];
    char version[24];
    char serial_no[40];
    char uuid[40];
};
typedef struct sarah_hd_system_entry sarah_hd_system_t;
SARAH_API int sarah_hd_system_init(sarah_hd_system_t *);
SARAH_API void sarah_hd_system_destroy(sarah_hd_system_t *);
SARAH_API void sarah_print_hd_system(sarah_hd_system_t *);

struct sarah_hd_board_entry
{
    char manufacturer[40];
    char product_name[24];
    char version[24];
    char serial_no[40];
};
typedef struct sarah_hd_board_entry sarah_hd_board_t;
SARAH_API int sarah_hd_board_init(sarah_hd_board_t *);
SARAH_API void sarah_hd_board_destroy(sarah_hd_board_t *);
SARAH_API void sarah_print_hd_board(sarah_hd_board_t *);

/* get the hardware information */
SARAH_API int sarah_get_hd_info(sarah_hd_bios_t *, sarah_hd_system_t *, sarah_hd_board_t *);




/// CPU time statistics entry define
struct sarah_cpu_time_entry
{
    unsigned int user;      // user cpu time
    unsigned int nice;      // nice cpu time
    unsigned int system;    // system cpu time
    unsigned int idle;      // wait cpu time except IO

    char name[16];          // device name
};
typedef struct sarah_cpu_time_entry sarah_cpu_time_t;

// CPU statistics interface
SARAH_API float sarah_cal_cpu_occupy(const sarah_cpu_time_t *, const sarah_cpu_time_t *);

/*
 * get the cpu time statistics information
 *
 * @param   sarah_cpu_time_t
 * @return  0 for succeed and greater than 0 integer for falied
*/
SARAH_API int sarah_get_cpu_time(sarah_cpu_time_t *);

// print the cpu statistics information
SARAH_API void sarah_print_cpu_time(const sarah_cpu_time_t *);




/// CPU statistics entry define
struct sarah_cpu_stat_entry
{
    sarah_cpu_time_t round_1;
    sarah_cpu_time_t round_2;
    float load_avg;     // CPU load average
};
typedef struct sarah_cpu_stat_entry sarah_cpu_stat_t;

SARAH_API void sarah_cal_cpu_stat(
    sarah_cpu_stat_t *, sarah_cpu_time_t *, sarah_cpu_time_t *);
SARAH_API void sarah_print_cpu_stat(sarah_cpu_stat_t *);



/// CPU load average entry define
struct sarah_cpu_loadavg_entry
{
    float t_1m;   // 1 minitues
    float t_5m;   // 5 minitues
    float t_15m;  // 15 minitues
};
typedef struct sarah_cpu_loadavg_entry sarah_cpu_loadavg_t;

SARAH_API int sarah_get_cpu_loadavg(sarah_cpu_loadavg_t *);
SARAH_API void sarah_print_cpu_loadavg(sarah_cpu_loadavg_t *);




/// CPU information entry define
struct sarah_cpu_info_entry
{
    unsigned int model;         // CPU model
    float mhz;                  // CPU MHZ
    unsigned int cache_size;    // CPU cache size in Kbytes
    unsigned int physical_id;   // CPU physical id
    unsigned int core_id;       // CPU core id
    unsigned int cores;         // CPU cores

    char vendor_id[32];         // CPU vendor_id
    char model_name[84];        // CPU model name
};
typedef struct sarah_cpu_info_entry sarah_cpu_info_t;

SARAH_API sarah_cpu_info_t *new_cpu_info_entry();
SARAH_API void free_cpu_info_entry(void **);

/// CPU information
SARAH_API int sarah_get_cpu_info(cel_link_t *);
SARAH_API void sarah_print_cpu_info(const sarah_cpu_info_t *);




/// MEMORY statistics entry define
struct sarah_ram_stat_entry
{
    unsigned long int total;        // total size in KBytes
    unsigned long int free;         // free size in KBytes
    unsigned long int available;    // available size in KBytes
};
typedef struct sarah_ram_stat_entry sarah_ram_stat_t;

// interface to get the RAM statistics information
SARAH_API int sarah_get_ram_stat(sarah_ram_stat_t *);
SARAH_API void sarah_print_ram_stat(const sarah_ram_stat_t *);




/// DISK statistics entry define
struct sarah_disk_stat_entry
{
    unsigned long int inode;    // inode number
    unsigned long int iused;    // used inode number
    unsigned long int size;     // total size in KB
    unsigned long int used;     // total size used in KB

    char type[16];      // file system type
    char name[48];      // file system name
};
typedef struct sarah_disk_stat_entry sarah_disk_stat_t;

SARAH_API sarah_disk_stat_t *new_disk_stat_entry();
SARAH_API sarah_disk_stat_t *clone_disk_stat_entry(sarah_disk_stat_t *);
SARAH_API void free_disk_stat_entry(void **);


// get the disk statistics information
SARAH_API int sarah_get_disk_stat(cel_link_t *);
SARAH_API void sarah_print_disk_stat(const sarah_disk_stat_t *);
SARAH_API int sarah_is_valid_disk_type(char *);


/* New version of disk interface started at 2019/02/13 */
struct sarah_disk_entry
{
    /* disk hardware information */
    char wwn[41];                       /* WWN (disk world wild name) */
    char serial_no[21];                 /* disk serial no */
    char model[41];                     /* disk model */

    /* disk partition information */
    char name[64];                      /* disk partition name */
    char format[16];                    /* disk partition format */
    char mount[156];                    /* mount path for the partition */

    int part_no;                        /* the index of the partition */
    unsigned long int p_start;          /* start blocks of the partition */
    unsigned long long int p_blocks;    /* total blocks of the partition */
    unsigned long long int p_size;      /* total bytes of the partition */

    /* partition storage information */
    unsigned long long int inode;       /* partition inode number */
    // unsigned long int iused;         /* used inode number */
    unsigned long long int ifree;       /* free inode number */
    unsigned long long int size;        /* total size in byte */
    // unsigned long int used;          /* total size used in byte */
    unsigned long long int free;        /* free size used in byte */
};
typedef struct sarah_disk_entry sarah_disk_t;

/* disk partition entry define */
struct sarah_disk_partition_entry
{
	unsigned long int start;        /* start block index */
	unsigned long int blocks;       /* total blocks for this partition */
	char *format;                   /* partition format */
};
typedef struct sarah_disk_partition_entry sarah_disk_partition_t;
SARAH_API sarah_disk_partition_t *new_disk_partition_entry();
SARAH_API void free_disk_partition_entry(void **);
SARAH_API int sarah_disk_partition_init(sarah_disk_partition_t *);
SARAH_API void sarah_disk_partition_destroy(sarah_disk_partition_t *);
SARAH_API void sarah_print_disk_partition(sarah_disk_partition_t *);



#define SARAH_DISK_FILL_PT (0x01 << 0)     /* fill the disk partition information */
#define SARAH_DISK_FILL_ST (0x01 << 1)     /* fill the storage information */

typedef int (* sarah_disk_filter_fn_t) (sarah_disk_t *, int);

SARAH_API int sarah_disk_init(sarah_disk_t *);
SARAH_API void sarah_disk_destroy(sarah_disk_t *);
SARAH_API sarah_disk_t *new_disk_entry();
SARAH_API sarah_disk_t *clone_disk_entry(sarah_disk_t *);
SARAH_API void free_disk_entry(void **);

// get the disk information
SARAH_API int sarah_get_disk_list(cel_link_t *, int, sarah_disk_filter_fn_t);
SARAH_API void sarah_print_disk(const sarah_disk_t *);

/// disk partition and format make
/**
 * create disk partition by shell script
 * @return
 */
SARAH_API int sarah_disk_partition_shell(char *, sarah_disk_partition_t **, int);
/**
 * make partition format by shell script
 * @return
 */
SARAH_API int sarah_disk_format_shell(char *);




/// Network statistics entry define
struct sarah_net_stat_entry
{
    unsigned long int receive;      // receive bytes
    unsigned long int r_packets;    // receive packets
    unsigned long int transmit;     // transmit bytes
    unsigned long int t_packets;    // transmit packets

    unsigned char hd_addr[6];       // hardware address
    char device[48];                // device name
};
typedef struct sarah_net_stat_entry sarah_net_stat_t;

SARAH_API sarah_net_stat_t *new_net_stat_entry();
SARAH_API void copy_net_stat_entry(sarah_net_stat_t *, sarah_net_stat_t *);
SARAH_API void free_net_stat_entry(void **);

// interface to get the network statistics information
SARAH_API int sarah_get_net_stat(cel_link_t *);
SARAH_API void sarah_print_net_stat(const sarah_net_stat_t *);


/* Interface to get the MAC address of the specified net device */
SARAH_API int sarah_get_hardware_addr(unsigned char mac[6], char *);
SARAH_API void sarah_hardware_addr_print(unsigned char mac[6], char *);


struct sarah_bandwidth_stat_entry
{
    sarah_net_stat_t round_1;
    sarah_net_stat_t round_2;
    // sarah_net_stat_t *r1;   // round 1 ptr
    // sarah_net_stat_t *r2;   // round 2 ptr
    float incoming;     // receive rate
    float outgoing;     // transmit rate
};
typedef struct sarah_bandwidth_stat_entry sarah_bandwidth_stat_t;

SARAH_API sarah_bandwidth_stat_t *new_bandwidth_stat_entry();
SARAH_API void free_bandwidth_stat_entry(void **);

// interface to calculate the network rate in BITS
SARAH_API int sarah_get_bandwidth_stat(cel_link_t *);
SARAH_API int sarah_get_bandwidth_stat_for(cel_link_t *, int duration);
SARAH_API void sarah_print_bandwidth_stat(sarah_bandwidth_stat_t *);




/** process module interface */
struct sarah_process_entry
{
    unsigned int pid;           // process id

    float cpu_percent;          // CPU usage percent
    float mem_percent;          // Memory usage percent

    unsigned int vir_mem;       // virtual memory in KB
    unsigned int rss_mem;       // Fix physical memory in KB

    char tty[16];               // running terminator

    /*
     * 其中STAT状态位常见的状态字符有
     * D - 无法中断的休眠状态（通常 IO 的进程）
     * R - 正在运行可中在队列中可过行的
     * S - 处于休眠状态
     * T - 停止或被追踪
     * W - 进入内存交换 （从内核2.6开始无效）
     * X - 死掉的进程 （基本很少见）
     * Z - 僵尸进程
     * < - 优先级高的进程
     * N - 优先级较低的进程
     * L - 有些页被锁进内存
     * s - 进程的领导者（在它之下有子进程）
     * l - 多线程，克隆线程（使用 CLONE_THREAD, 类似 NPTL pthreads）
     * + - 位于后台的进程组
    */
    char status[16];            // process status
    char start[16];             // start time
    char time[16];              // CPU usage time

    char user[32];              // running user (the first 24 bytes)
    char command[256];          // running command
};
typedef struct sarah_process_entry sarah_process_t;

SARAH_API sarah_process_t *new_process_entry();
SARAH_API void free_process_entry(void **);

SARAH_API int sarah_get_process_list(cel_link_t *);
SARAH_API void sarah_print_process(const sarah_process_t *);




/// ------ sarah executor ------

#define SARAH_TASK_STATUS_ERR       -1
#define SARAH_TASK_STATUS_READY      0
#define SARAH_TASK_STATUS_EXECUTING  1
#define SARAH_TASK_STATUS_COMPLETED  2

typedef enum {
    sarah_task_err   = -1,
    sarah_task_ready =  0,
    sarah_task_executing = 1,
    sarah_task_completed = 2
} sarah_task_status_t;

#define SARAH_TASK_SYNC_ERR     (0x01 << 0)
#define SARAH_TASK_SYNC_START   (0x01 << 1)
#define SARAH_TASK_SYNC_ING     (0x01 << 2)
#define SARAH_TASK_SYNC_DONE    (0x01 << 3)

typedef enum {
    sarah_task_sync_err   = (0x01 << 0),
    sarah_task_sync_start = (0x01 << 1),
    sarah_task_sync_executing = (0x01 << 2),
    sarah_task_sync_completed = (0x01 << 3)
} sarah_task_sync_t;


/** sarah task entry define */
#define _sarah_task_string_id
struct sarah_task_entry
{
    unsigned long long int id;      /* command id */
    unsigned int created_at;        /* command created unix timestamp */
    unsigned int sync_mask;         /* synchronize mask define */

    char *engine;                   /* command engine name, like bash, lua eg ... */
    char *cmd_argv;                 /* raw string arguments */
    char *cmd_text;                 /* command text content */
    char *sign;                     /* command md5 sign */
};
typedef struct sarah_task_entry sarah_task_t;

SARAH_API sarah_task_t *new_task_entry();
SARAH_API void free_task_entry(void **);
SARAH_API void sarah_print_task(sarah_task_t *);
SARAH_API int sarah_task_init_from_json(sarah_task_t *, cJSON *);
SARAH_API int sarah_task_validity(sarah_node_t *, sarah_task_t *);


/**
 * sarah task result entry define
 * multi-task could share the same result entry
 * that why we create a separate result entry.
*/
// struct sarah_task_result_entry
// {
//     unsigned int status;            /* command running status */
//     unsigned float progress;        /* command execution progress (0 ~ 1) */
//     int errno;                      /* execution errno */
//     int max_datalen;                /* the max bytes of the data */
//     cel_strbuff_t error_buffer;     /* error string buffer */
//     cel_strbuff_t data_buffer;      /* data string buffer */
// };
// typedef struct sarah_task_result_entry sarah_task_result_t;


// sarah executor error define
#define SARAH_EXECUTOR_ERRSYNTAX    1
#define SARAH_EXECUTOR_ERRMEM       2
#define SARAH_EXECUTOR_ERRGC        3
#define SARAH_EXECUTOR_ERRRUN       4
#define SARAH_EXECUTOR_ERRERR       5
#define SARAH_EXECUTOR_ERRFILE      6
struct sarah_executor_entry
{
    sarah_node_t *node; /* sarah node ptr ONLY and executor will not manage its alloc */
    sarah_task_t *task; /* sarah task ptr ONLY and executor will not manage its alloc */

    /* string executor implementation */
    int (*dostring)(struct sarah_executor_entry *, char *, char *);
    /* file executor implementation */
    int (*dofile)(struct sarah_executor_entry *, char *, char *);

    // char *tsrc_url;              /* task source http url */
    char *sync_url;                 /* execution status synchronization http url */
    cel_strbuff_t res_buffer;       /* http response string buffer */
    struct curl_slist *headers;     /* curl http headers */
    unsigned int sign_offset;
    unsigned int cmid_offset;

    /* the execution result */
    int errno;                      /* execution errno */
    int status;                     /* command running status */
    float progress;                 /* command execution progress (0 ~ 1) */
    unsigned int attach;            /* 4-bytes attach space */
    unsigned int max_datalen;       /* the max bytes of the data */
    cel_strbuff_t error_buffer;     /* error string buffer */
    cel_strbuff_t data_buffer;      /* data string buffer */

    /* the remote task to execute for the current executor */
    // sarah_remote_task_t *task;
};
typedef struct sarah_executor_entry sarah_executor_t;


SARAH_API sarah_executor_t *new_executor_entry(sarah_node_t *);
SARAH_API void free_executor_entry(void **);
SARAH_API int sarah_executor_init(sarah_node_t *, sarah_executor_t *);
SARAH_API void sarah_executor_destroy(sarah_executor_t *);


// before execution
SARAH_API void sarah_executor_clear(sarah_executor_t *);
SARAH_API void sarah_executor_set_maxdatalen(sarah_executor_t *, int);
SARAH_API void sarah_executor_set_task(sarah_executor_t *, sarah_task_t *);

// under the execution
SARAH_API int sarah_executor_set_syntax(sarah_executor_t *, char *);
SARAH_API int sarah_executor_dostring(sarah_executor_t *, char *, char *);
SARAH_API int sarah_executor_dofile(sarah_executor_t *, char *, char *);
SARAH_API int sarah_executor_dotask(sarah_executor_t *, sarah_task_t *);
SARAH_API int sarah_executor_data_available(sarah_executor_t *, int);
SARAH_API int sarah_executor_data_append(sarah_executor_t *, char *);
SARAH_API int sarah_executor_data_insert(sarah_executor_t *, int, char *);
SARAH_API void sarah_executor_data_clear(sarah_executor_t *);
SARAH_API int sarah_executor_error_append(sarah_executor_t *, char *);
SARAH_API int sarah_executor_error_insert(sarah_executor_t *, int, char *);
SARAH_API void sarah_executor_error_clear(sarah_executor_t *);
SARAH_API void sarah_executor_set_status(sarah_executor_t *, int, float);

// execution result fetch
SARAH_API char *sarah_executor_get_data(sarah_executor_t *);
SARAH_API int sarah_executor_get_datalen(sarah_executor_t *);
SARAH_API int sarah_executor_get_errlen(sarah_executor_t *);
SARAH_API char *sarah_executor_get_errstr(sarah_executor_t *);
SARAH_API void sarah_executor_set_errno(sarah_executor_t *, int);
SARAH_API int sarah_executor_get_errno(sarah_executor_t *);
SARAH_API void sarah_executor_set_attach(sarah_executor_t *, unsigned int);
SARAH_API unsigned int sarah_executor_get_attach(sarah_executor_t *);

/*
 * This will make a network(http/socket) request
 * and synchronize the current status of the execution to the remote server
*/
SARAH_API int sarah_executor_sync_status(sarah_executor_t *);
SARAH_API void sarah_print_executor(sarah_executor_t *);



/**
 * sarah string command and file script executor common function interface
 *
 * @param   executor
 * @param   command
 * @param   buffer
 * @param   max length of data set to -1 for no limitation
*/
typedef int (* sarah_string_executor_fn_t) (sarah_executor_t *, char *, char *);
typedef int (* sarah_file_executor_fn_t) (sarah_executor_t *, char *, char *);



// bash executor interface define
// @Note must with the above interface implemented
SARAH_API int script_bash_dostring(sarah_executor_t *, char *, char *);
SARAH_API int script_bash_dofile(sarah_executor_t *, char *, char *);

// @Note must with the above interface implemented
SARAH_API int script_lua_dostring(sarah_executor_t *, char *, char *);
SARAH_API int script_lua_dofile(sarah_executor_t *, char *, char *);

// php executor interface define
SARAH_API int script_sarah_dostring(sarah_executor_t *, char *, char *);
SARAH_API int script_sarah_dofile(sarah_executor_t *, char *, char *);




/* sarah booter interface define */
#define SARAH_PROC_READY    0
#define SARAH_PROC_RUNNING  1
#define SARAH_PROC_STOP     2
#define SARAH_PROC_RESTART  3
#define SARAH_PROC_EXIT     4

#define SARAH_SIGNAL_STOP    SIGQUIT
#define SARAH_SIGNAL_CONT    SIGHUP
#define SARAH_SIGNAL_EXIT_01 SIGINT
#define SARAH_SIGNAL_EXIT_02 SIGTERM
#define SARAH_SIGNAL_RESTART SIGUSR1
#define SARAH_SIGNAL_CHECK   SIGUSR2
#define SARAH_SIGNAL_KILL    SIGKILL

struct sarah_booter_entry
{
    int status;         /* booter running status */
    pid_t pid;          /* booter running pid */

    char *base_dir;     /* booter base directory */
    char *exe_file;     /* LSB executable file path */
    char *pid_file;     /* booter pid loca file */
};
typedef struct sarah_booter_entry sarah_booter_t;

SARAH_API int sarah_booter_init(sarah_booter_t *, char *);
SARAH_API void sarah_booter_destroy(sarah_booter_t *);
SARAH_API pid_t sarah_booter_getpid(sarah_booter_t *);
SARAH_API int sarah_booter_setpid(sarah_booter_t *, pid_t);


/**
 * sarah application interface define
 * All the interface will be started up at the sarah main application
*/
typedef int (* sarah_application_fn_t) (sarah_booter_t *, sarah_node_t *);
struct sarah_application_entry
{
    char *name;
    sarah_application_fn_t boot;
    pid_t pid;
};
typedef struct sarah_application_entry sarah_application_t;

/* build-in application for sarah os */
SARAH_API int sarah_collector_application(sarah_booter_t *, sarah_node_t *);
SARAH_API int sarah_executor_application(sarah_booter_t *, sarah_node_t *);
SARAH_API int sarah_updater_application(sarah_booter_t *, sarah_node_t *);
SARAH_API int sarah_smartdisk_application(sarah_booter_t *, sarah_node_t *);


#endif  /* end ifndef */
