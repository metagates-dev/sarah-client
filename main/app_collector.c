/**
 * sarah monitor client
 * 
 * 1, gathering the CPU,RAM,DISK,NETWORK data
 * 2, build a JSON string define in README.md
 * 3, post the data to the remote concole server
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <curl/curl.h>
#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

// collector entry and interface define
// @Note: Please Copy them from the remove server
struct sarah_collector_entry
{
    char *api_url;
    unsigned int seed_offset;       // seed offset in the url buffer
    unsigned int sign_offset;       // signature offset in the url buffer
    struct curl_slist *headers;
    cel_strbuff_t *strbuff;         // http recv buffer
};
typedef struct sarah_collector_entry sarah_collector_t;

SARAH_STATIC int sarah_collector_init(sarah_node_t *, sarah_collector_t *);
SARAH_STATIC void sarah_collector_destroy(sarah_collector_t *);
static size_t _post_recv_callback(void *, size_t, size_t, void *);



SARAH_STATIC void _cal_bandwidth_stat(
    cel_link_t *, cel_link_t *, cel_link_t *, double);
SARAH_STATIC void _make_json_package(
    cJSON *,                // JSON Object
    sarah_node_t *,         // Node
    sarah_hd_bios_t *, sarah_hd_system_t *, sarah_hd_board_t *,
    cel_link_t *, sarah_cpu_stat_t *, sarah_cpu_loadavg_t *,   // CPU
    sarah_ram_stat_t *,     // RAM
    cel_link_t *,           // DISK
    cel_link_t *,           // NETWORK
    cel_link_t *            // PROCESS
);
SARAH_STATIC int _do_json_upload(sarah_node_t *, sarah_collector_t *, char *);



SARAH_STATIC int sarah_collector_init(sarah_node_t *node, sarah_collector_t *collector)
{
    char buffer[256] = {'\0'}, *api_base;
    unsigned int url_len = 0;

	api_base = collector_post_url;
    memset(collector, 0x00, sizeof(sarah_collector_t));

    /* 
     * make the request uri
     * app_key={app_key}
     * seed={current_timestamp}
     * json={JSON package}
     * sign={md5(seed+json+app_key)}
     * style: ?app_key=&seed=&sign=
    */
    url_len += strlen(api_base);
    url_len += strlen("?app_key=");
    url_len += strlen(node->config.app_key);
    url_len += strlen("&seed=");
    collector->seed_offset = url_len;
    url_len += 10;      // random seed string length
    url_len += strlen("&sign=");
    collector->sign_offset = url_len;
    url_len += 32;      // md5 signature string length
    collector->api_url = (char *) sarah_malloc(url_len + 1);
    if ( collector->api_url == NULL ) {
        printf("Allocate Error In sarah_collector_init For %u Bytes.\n", url_len);
        return 1;
    }

    sprintf(
        collector->api_url, 
        "%s?app_key=%s&seed=%010d&sign=%32s",
        api_base,
        node->config.app_key,
        0,
        "7f9106d9a1fa6d0f3550cc1783342638"
    );
    collector->api_url[url_len - 1] = '\0';

    // Initialize the http header
    snprintf(buffer, sizeof(buffer), "Content-Type: application/json");
    collector->headers = curl_slist_append(collector->headers, buffer);
    snprintf(buffer, sizeof(buffer), "User-Agent: node collector/1.0 sarahos/%.02f", sarah_version());
    collector->headers = curl_slist_append(collector->headers, buffer);
    // printf("seed_offset=%s\n", collector->api_url + collector->seed_offset);
    // printf("sign_offset=%s\n", collector->api_url + collector->sign_offset);

    // Initialize the string buffer
    collector->strbuff = new_cel_strbuff_opacity(256);
    if ( collector->strbuff == NULL ) {
        return 2; 
    }

    return 0;
}

SARAH_API void sarah_collector_destroy(sarah_collector_t *collector)
{
    if ( collector->api_url != NULL ) {
        sarah_free(collector->api_url);
        collector->api_url = NULL;
    }

    if ( collector->headers != NULL ) {
        curl_slist_free_all(collector->headers);
        collector->headers = NULL;
    }

    if ( collector->strbuff != NULL ) {
        free_cel_strbuff(&collector->strbuff);
        collector->strbuff = NULL;
    }
}


/**
 * calculate the bandwidth according to the network statistics list
*/
SARAH_STATIC void _cal_bandwidth_stat(
    cel_link_t *bandwidth_list,     // bandwidth list
    cel_link_t *net_stat_l1, cel_link_t *net_stat_l2, double cost_secs)
{
    cel_link_node_t *l_node1, *l_node2;
    sarah_net_stat_t *nstat_r1, *nstat_r2;
    sarah_bandwidth_stat_t *bd_stat;

    for ( l_node1 = net_stat_l1->head->_next, l_node2 = net_stat_l2->head->_next; 
        l_node1 != net_stat_l1->tail && l_node2 != net_stat_l2->tail;
            l_node1 = l_node1->_next, l_node2 = l_node2->_next ) {
        nstat_r1 = (sarah_net_stat_t *) l_node1->value;
        nstat_r2 = (sarah_net_stat_t *) l_node2->value;

        // create a new bandwidth entry
        bd_stat = new_bandwidth_stat_entry();
        copy_net_stat_entry(&bd_stat->round_1, nstat_r1);
        copy_net_stat_entry(&bd_stat->round_2, nstat_r2);
        bd_stat->incoming = (nstat_r2->receive - nstat_r1->receive) / cost_secs;
        bd_stat->outgoing = (nstat_r2->transmit - nstat_r1->transmit) / cost_secs;

        cel_link_add_last(bandwidth_list, bd_stat);
    }
}

/**
 * Make a std JSON package according to the JSON format defination
 *  desc in the README JSON package section
*/
SARAH_STATIC void _make_json_package(
    cJSON *stat_json, 
    sarah_node_t *node, 
    // hardware
    sarah_hd_bios_t *hd_bios, 
    sarah_hd_system_t *hd_sys, 
    sarah_hd_board_t *hd_board,
    // cpu start
    cel_link_t *cpu_info_list, 
    sarah_cpu_stat_t *cpu_stat, 
    sarah_cpu_loadavg_t *cpu_loadavg, 
    // end cpu
    sarah_ram_stat_t *ram_stat, 
    cel_link_t *disk_list,
    cel_link_t *bandwidth_list, cel_link_t *ps_list )
{
    // tmp vars
    cel_link_node_t *l_node;
    sarah_cpu_info_t *cpu_info;
    sarah_disk_t *disk;
    sarah_bandwidth_stat_t *bwidth;
    sarah_process_t *ps;
    cJSON *_object, *_array, *_number, *_string;

    // JSON object header vars
    cJSON *json_node = NULL;
    cJSON *json_hd_bios = NULL, *json_hd_sys = NULL, *json_hd_board = NULL;
    cJSON *json_cpu = NULL;
    cJSON *json_cpu_list, *json_cpu_stat, *json_cpu_loadavg;
    cJSON *json_ram = NULL;
    cJSON *json_disk = NULL;
    cJSON *json_net = NULL;
    cJSON *json_round1 = NULL, *json_round2 = NULL;
    cJSON *json_sys = NULL;
    cJSON *json_process = NULL;
    char mac_addr[18] = {'\0'};

    // version number
    cJSON_AddItemToObject(stat_json, "version", cJSON_CreateNumber(sarah_version()));


    // serialize the node info
    json_node = cJSON_CreateObject();
    cJSON_AddItemToObject(stat_json, "node", json_node);
    _string = cJSON_CreateString(node->node_id);
    cJSON_AddItemToObject(json_node, "id", _string);
    _number = cJSON_CreateNumber(node->created_at);
    cJSON_AddItemToObject(json_node, "created_at", _number);
    _number = cJSON_CreateNumber(node->last_boot_at);
    cJSON_AddItemToObject(json_node, "last_boot_at", _number);
    _number = cJSON_CreateNumber(node->interval);
    cJSON_AddItemToObject(json_node, "interval", _number);
    _number = cJSON_CreateNumber(node->last_stat_begin);
    cJSON_AddItemToObject(json_node, "last_stat_begin", _number);
    _number = cJSON_CreateNumber(node->last_stat_end);
    cJSON_AddItemToObject(json_node, "last_stat_end", _number);
    _string = cJSON_CreateString(node->base_dir);
    cJSON_AddItemToObject(json_node, "storage_media", _string);


    // serialize the hardware info
    json_hd_bios = cJSON_CreateObject();
    cJSON_AddItemToObject(stat_json, "hd_bios", json_hd_bios);
    _string = cJSON_CreateString(hd_bios->vendor);
    cJSON_AddItemToObject(json_hd_bios, "vendor", _string);
    _string = cJSON_CreateString(hd_bios->version);
    cJSON_AddItemToObject(json_hd_bios, "version", _string);
    _string = cJSON_CreateString(hd_bios->release_date);
    cJSON_AddItemToObject(json_hd_bios, "r_date", _string);
    _number = cJSON_CreateNumber(hd_bios->runtime_size);
    cJSON_AddItemToObject(json_hd_bios, "run_size", _number);
    _number = cJSON_CreateNumber(hd_bios->rom_size);
    cJSON_AddItemToObject(json_hd_bios, "rom_size", _number);

    json_hd_sys = cJSON_CreateObject();
    cJSON_AddItemToObject(stat_json, "hd_sys", json_hd_sys);
    _string = cJSON_CreateString(hd_sys->manufacturer);
    cJSON_AddItemToObject(json_hd_sys, "manufacturer", _string);
    _string = cJSON_CreateString(hd_sys->product_name);
    cJSON_AddItemToObject(json_hd_sys, "pro_name", _string);
    _string = cJSON_CreateString(hd_sys->version);
    cJSON_AddItemToObject(json_hd_sys, "version", _string);
    _string = cJSON_CreateString(hd_sys->serial_no);
    cJSON_AddItemToObject(json_hd_sys, "serial_no", _string);
    _string = cJSON_CreateString(hd_sys->uuid);
    cJSON_AddItemToObject(json_hd_sys, "uuid", _string);

    json_hd_board = cJSON_CreateObject();
    cJSON_AddItemToObject(stat_json, "hd_board", json_hd_board);
    _string = cJSON_CreateString(hd_board->manufacturer);
    cJSON_AddItemToObject(json_hd_board, "manufacturer", _string);
    _string = cJSON_CreateString(hd_board->product_name);
    cJSON_AddItemToObject(json_hd_board, "pro_name", _string);
    _string = cJSON_CreateString(hd_board->version);
    cJSON_AddItemToObject(json_hd_board, "version", _string);
    _string = cJSON_CreateString(hd_board->serial_no);
    cJSON_AddItemToObject(json_hd_board, "serial_no", _string);



    // serialize the CPU info
    json_cpu = cJSON_CreateObject();
    cJSON_AddItemToObject(stat_json, "cpu", json_cpu);
    json_cpu_list = cJSON_CreateArray();
    cJSON_AddItemToObject(json_cpu, "list", json_cpu_list);
    // Append the cpu list
    for ( l_node = cpu_info_list->head->_next; 
        l_node != cpu_info_list->tail; l_node = l_node->_next ) {
        cpu_info = (sarah_cpu_info_t *) l_node->value;

        // sarah_print_cpu_info(cpu_info);
        _array = cJSON_CreateObject();
        cJSON_AddItemToArray(json_cpu_list, _array);

        // set all the attributes
        _string = cJSON_CreateString(cpu_info->vendor_id);
        cJSON_AddItemToObject(_array, "vendor_id", _string);
        _number = cJSON_CreateNumber(cpu_info->model);
        cJSON_AddItemToObject(_array, "model", _number);
        _string = cJSON_CreateString(cpu_info->model_name);
        cJSON_AddItemToObject(_array, "model_name", _string);
        _number = cJSON_CreateNumber(cpu_info->mhz);
        cJSON_AddItemToObject(_array, "mhz", _number);
        _number = cJSON_CreateNumber(cpu_info->cache_size);
        cJSON_AddItemToObject(_array, "cache_size", _number);
        _number = cJSON_CreateNumber(cpu_info->physical_id);
        cJSON_AddItemToObject(_array, "physical_id", _number);
        _number = cJSON_CreateNumber(cpu_info->core_id);
        cJSON_AddItemToObject(_array, "core_id", _number);
        _number = cJSON_CreateNumber(cpu_info->cores);
        cJSON_AddItemToObject(_array, "cores", _number);
    }


    // serialize the CPU stat
    json_cpu_stat = cJSON_CreateObject();
    cJSON_AddItemToObject(json_cpu, "stat", json_cpu_stat);
    json_round1 = cJSON_CreateObject();
    cJSON_AddItemToObject(json_cpu_stat, "round_1", json_round1);
    _string = cJSON_CreateString(cpu_stat->round_1.name);
    cJSON_AddItemToObject(json_round1, "name", _string);
    _number = cJSON_CreateNumber(cpu_stat->round_1.user);
    cJSON_AddItemToObject(json_round1, "user", _number);
    _number = cJSON_CreateNumber(cpu_stat->round_1.nice);
    cJSON_AddItemToObject(json_round1, "nice", _number);
    _number = cJSON_CreateNumber(cpu_stat->round_1.system);
    cJSON_AddItemToObject(json_round1, "system", _number);
    _number = cJSON_CreateNumber(cpu_stat->round_1.idle);
    cJSON_AddItemToObject(json_round1, "idle", _number);

    json_round2 = cJSON_CreateObject();
    cJSON_AddItemToObject(json_cpu_stat, "round_2", json_round2);
    _string = cJSON_CreateString(cpu_stat->round_2.name);
    cJSON_AddItemToObject(json_round2, "name", _string);
    _number = cJSON_CreateNumber(cpu_stat->round_2.user);
    cJSON_AddItemToObject(json_round2, "user", _number);
    _number = cJSON_CreateNumber(cpu_stat->round_2.nice);
    cJSON_AddItemToObject(json_round2, "nice", _number);
    _number = cJSON_CreateNumber(cpu_stat->round_2.system);
    cJSON_AddItemToObject(json_round2, "system", _number);
    _number = cJSON_CreateNumber(cpu_stat->round_2.idle);
    cJSON_AddItemToObject(json_round2, "idle", _number);

    _number = cJSON_CreateNumber(cpu_stat->load_avg);
    cJSON_AddItemToObject(json_cpu_stat, "load_avg", _number);

    json_cpu_loadavg = cJSON_CreateObject();
    cJSON_AddItemToObject(json_cpu_stat, "sys_loadavg", json_cpu_loadavg);
    _number = cJSON_CreateNumber(cpu_loadavg->t_1m);
    cJSON_AddItemToObject(json_cpu_loadavg, "t_1m", _number);
    _number = cJSON_CreateNumber(cpu_loadavg->t_5m);
    cJSON_AddItemToObject(json_cpu_loadavg, "t_5m", _number);
    _number = cJSON_CreateNumber(cpu_loadavg->t_15m);
    cJSON_AddItemToObject(json_cpu_loadavg, "t_15m", _number);



    // serialize the the ram stat
    json_ram = cJSON_CreateObject();
    cJSON_AddItemToObject(stat_json, "ram", json_ram);
    _number = cJSON_CreateNumber(ram_stat->total);
    cJSON_AddItemToObject(json_ram, "total", _number);
    _number = cJSON_CreateNumber(ram_stat->free);
    cJSON_AddItemToObject(json_ram, "free", _number);
    _number = cJSON_CreateNumber(ram_stat->available);
    cJSON_AddItemToObject(json_ram, "available", _number);


    // serialize the disk stat array
    json_disk = cJSON_CreateArray();
    cJSON_AddItemToObject(stat_json, "disk", json_disk);
    for ( l_node = disk_list->head->_next; 
        l_node != disk_list->tail; l_node = l_node->_next ) {
        disk = (sarah_disk_t *) l_node->value;

        _object = cJSON_CreateObject();
        cJSON_AddItemToArray(json_disk, _object);

        // set the attributes
        _string = cJSON_CreateString(disk->wwn);
        cJSON_AddItemToObject(_object, "wwn", _string);
        _string = cJSON_CreateString(disk->serial_no);
        cJSON_AddItemToObject(_object, "serial_no", _string);
        _string = cJSON_CreateString(disk->model);
        cJSON_AddItemToObject(_object, "model", _string);

        _string = cJSON_CreateString(disk->name);
        cJSON_AddItemToObject(_object, "name", _string);
        _number = cJSON_CreateNumber(disk->part_no);
        cJSON_AddItemToObject(_object, "pt_no", _number);
        _string = cJSON_CreateString(disk->format);
        cJSON_AddItemToObject(_object, "format", _string);
        _string = cJSON_CreateString(disk->mount);
        cJSON_AddItemToObject(_object, "mount", _string);

        _number = cJSON_CreateNumber(disk->p_start);
        cJSON_AddItemToObject(_object, "p_start", _number);
        _number = cJSON_CreateNumber(disk->p_blocks);
        cJSON_AddItemToObject(_object, "p_blocks", _number);
        _number = cJSON_CreateNumber(disk->p_size);
        cJSON_AddItemToObject(_object, "p_size", _number);

        _number = cJSON_CreateNumber(disk->inode);
        cJSON_AddItemToObject(_object, "inode", _number);
        _number = cJSON_CreateNumber(disk->ifree);
        cJSON_AddItemToObject(_object, "ifree", _number);
        _number = cJSON_CreateNumber(disk->size);
        cJSON_AddItemToObject(_object, "size", _number);
        _number = cJSON_CreateNumber(disk->free);
        cJSON_AddItemToObject(_object, "free", _number);
    }


    // serialize the network the stat array
    json_net = cJSON_CreateArray();
    cJSON_AddItemToObject(stat_json, "network", json_net);
    for ( l_node = bandwidth_list->head->_next; 
        l_node != bandwidth_list->tail; l_node = l_node->_next ) {
        bwidth = (sarah_bandwidth_stat_t *) l_node->value;

        _object = cJSON_CreateObject();
        cJSON_AddItemToArray(json_net, _object);

        // set all attributes
        _string = cJSON_CreateString(bwidth->round_1.device);
        cJSON_AddItemToObject(_object, "name", _string);
        sarah_hardware_addr_print(bwidth->round_1.hd_addr, mac_addr);
        _string = cJSON_CreateString(mac_addr);
        cJSON_AddItemToObject(_object, "hd_addr", _string);

        // round_1
        json_round1 = cJSON_CreateObject();
        cJSON_AddItemToObject(_object, "round_1", json_round1);
        _number = cJSON_CreateNumber(bwidth->round_1.receive);
        cJSON_AddItemToObject(json_round1, "receive", _number);
        _number = cJSON_CreateNumber(bwidth->round_1.r_packets);
        cJSON_AddItemToObject(json_round1, "r_packets", _number);
        _number = cJSON_CreateNumber(bwidth->round_1.transmit);
        cJSON_AddItemToObject(json_round1, "transmit", _number);
        _number = cJSON_CreateNumber(bwidth->round_1.t_packets);
        cJSON_AddItemToObject(json_round1, "t_packets", _number);


        // round_2
        json_round2 = cJSON_CreateObject();
        cJSON_AddItemToObject(_object, "round_2", json_round2);
        _number = cJSON_CreateNumber(bwidth->round_2.receive);
        cJSON_AddItemToObject(json_round2, "receive", _number);
        _number = cJSON_CreateNumber(bwidth->round_2.r_packets);
        cJSON_AddItemToObject(json_round2, "r_packets", _number);
        _number = cJSON_CreateNumber(bwidth->round_2.transmit);
        cJSON_AddItemToObject(json_round2, "transmit", _number);
        _number = cJSON_CreateNumber(bwidth->round_2.t_packets);
        cJSON_AddItemToObject(json_round2, "t_packets", _number);

        _number = cJSON_CreateNumber(bwidth->incoming);
        cJSON_AddItemToObject(_object, "incoming", _number);
        _number = cJSON_CreateNumber(bwidth->outgoing);
        cJSON_AddItemToObject(_object, "outgoing", _number);
    }


    // serialize the kernal and the system information
    json_sys = cJSON_CreateObject();
    cJSON_AddItemToObject(stat_json, "system", json_sys);

    _string = cJSON_CreateString("Linux");      // predefined system name
    cJSON_AddItemToObject(json_sys, "pre_def", _string);
    _string = cJSON_CreateString(node->kernel_info.sysname);
    cJSON_AddItemToObject(json_sys, "sys_name", _string);
    _string = cJSON_CreateString(node->kernel_info.nodename);
    cJSON_AddItemToObject(json_sys, "node_name", _string);
    _string = cJSON_CreateString(node->kernel_info.release);
    cJSON_AddItemToObject(json_sys, "release", _string);
    _string = cJSON_CreateString(node->kernel_info.version);
    cJSON_AddItemToObject(json_sys, "version", _string);
    _string = cJSON_CreateString(node->kernel_info.machine);
    cJSON_AddItemToObject(json_sys, "machine", _string);


    // serialize the process information
    json_process = cJSON_CreateArray();
    cJSON_AddItemToObject(stat_json, "process", json_process);
    for ( l_node = ps_list->head->_next; 
        l_node != ps_list->tail; l_node = l_node->_next ) {
        ps = (sarah_process_t *) l_node->value;

        _object = cJSON_CreateObject();
        cJSON_AddItemToArray(json_process, _object);

        // set all attributes of the object
        _string = cJSON_CreateString(ps->user);
        cJSON_AddItemToObject(_object, "user", _string);
        _number = cJSON_CreateNumber(ps->pid);
        cJSON_AddItemToObject(_object, "pid", _number);
        _number = cJSON_CreateNumber(ps->cpu_percent);
        cJSON_AddItemToObject(_object, "cpu", _number);
        _number = cJSON_CreateNumber(ps->mem_percent);
        cJSON_AddItemToObject(_object, "mem", _number);
        _number = cJSON_CreateNumber(ps->vir_mem);
        cJSON_AddItemToObject(_object, "vsz", _number);
        _number = cJSON_CreateNumber(ps->rss_mem);
        cJSON_AddItemToObject(_object, "rss", _number);
        _string = cJSON_CreateString(ps->tty);
        cJSON_AddItemToObject(_object, "tty", _string);
        _string = cJSON_CreateString(ps->status);
        cJSON_AddItemToObject(_object, "stat", _string);
        _string = cJSON_CreateString(ps->start);
        cJSON_AddItemToObject(_object, "start", _string);
        _string = cJSON_CreateString(ps->time);
        cJSON_AddItemToObject(_object, "time", _string);
        _string = cJSON_CreateString(ps->command);
        cJSON_AddItemToObject(_object, "cmd", _string);
    }
}


static size_t _post_recv_callback(
    void *buffer, size_t size, size_t nmemb, void *ptr)
{
    sarah_collector_t *collector = (sarah_collector_t *) ptr;
    cel_strbuff_append_from(collector->strbuff, (char *) buffer, size * nmemb, 1);
    return size * nmemb;
}

/*
 * try to upload the JSON package to the remote server
 *
 * @param   json_string
 * @return  int 0 for succeed and none-0 for failed
*/
SARAH_STATIC int _do_json_upload(
    sarah_node_t *node, sarah_collector_t *collector, char *json_string)
{
    char seed[11] = {'\0'};
    long http_code;
    int ret_code = 0;

    cel_md5_t md5_context;
    unsigned char digest[16] = {'\0'};
    char md5_str[33] = {'\0'};

    struct timeval tv;
    CURL *curl = NULL;
    cJSON *json_result = NULL, *cursor = NULL;

    // Initialize the curl and set the options
    curl = curl_easy_init();
    if ( curl == NULL ) {
        curl_global_cleanup();
        return 2;
    }

    // get the current unix timestamp as the seed
    gettimeofday(&tv, NULL);
    sprintf(seed, "%010ld", tv.tv_sec);
    memcpy(collector->api_url + collector->seed_offset, seed, 10);

    // calculate the md5 signature
    cel_md5_init(&md5_context);
    cel_md5_update(&md5_context, (uchar_t *)seed, 10);
    cel_md5_update(&md5_context, (uchar_t *)json_string, strlen(json_string));
    cel_md5_update(&md5_context, 
        (uchar_t *)node->config.app_key, strlen(node->config.app_key));
    cel_md5_final(&md5_context, digest);
    cel_md5_print(digest, md5_str);
    memcpy(collector->api_url + collector->sign_offset, md5_str, 32);

    // clear the receive string buffer
    cel_strbuff_clear(collector->strbuff);


    curl_easy_setopt(curl, CURLOPT_URL, collector->api_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, collector->headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000);          // 15 seconds
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);    // 1 hour cache for DNS
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_string);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _post_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, collector);

    // Perform the http request
    if ( curl_easy_perform(curl) != CURLE_OK ) {
        ret_code = 3;
        goto failed;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if ( http_code != 200 ) {
        ret_code = 3;
        goto failed;
    }

    // Parse the api response ?
    // Like override the local setting, eg ...
    json_result = cJSON_Parse(collector->strbuff->buffer);
    if ( json_result == NULL ) {
        ret_code = 4;
        goto failed;
    }

    cursor = cJSON_GetObjectItem(json_result, "errno");
    if ( cursor == NULL || cursor->valueint != 0 ) {
        ret_code = 4;
        goto failed;
    }

    // @Note parse the result and do something ?

failed:
    if ( json_result != NULL ) {
        cJSON_Delete(json_result);
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return ret_code;
}

/* disk scan filter */
static int _collector_disk_filter(sarah_disk_t *disk, int step)
{
    switch ( step ) {
    case -1:
        if ( strncmp(disk->name, "/dev/loop", 9) == 0 
            || strncmp(disk->name, "/dev/ram", 8) == 0 ) {
            return 1;
        }
        break;
    case SARAH_DISK_FILL_PT:
        if ( sarah_is_valid_disk_type(disk->format) > 0 ) {
            return 1;
        }
        break;
    }

    return 0;
}


/**
 * sarah hardware information collector application implementation
 *
 * @param   booter
 * @param   node
 * @return  int
*/
SARAH_API int sarah_collector_application(sarah_booter_t *booter, sarah_node_t *node)
{
    int errno, stat_duration_sec;
    struct timeval tv_s, tv_e;
    double cost_secs;

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    pid_t boot_ppid, cur_ppid;
#endif

    sarah_collector_t collector;

    // hardware
    sarah_hd_bios_t hd_bios;
    sarah_hd_system_t hd_sys;
    sarah_hd_board_t hd_board;

    // CPU information list and CPU time
    cel_link_t cpu_info_list;
    sarah_cpu_stat_t cpu_stat;
    sarah_cpu_time_t cpu_time_r1;
    sarah_cpu_time_t cpu_time_r2;
    sarah_cpu_loadavg_t cpu_loadavg;

    sarah_ram_stat_t ram_stat;      // RAM statistics information
    cel_link_t disk_list;           // DISK information list

    // network and bandwidth statistics list
    cel_link_t net_stat_l1;
    cel_link_t net_stat_l2;     
    cel_link_t bandwidth_list;

    // process list
    cel_link_t ps_list;

    cJSON *stat_json = NULL;
    char *json_string = NULL;
    int post_ret = 0, stat_to_go = 0;

    // ------ initialize work
    stat_duration_sec = 1;
    if ( (errno = sarah_collector_init(node, &collector)) != 0 ) {
        printf("[Error]: Fail to initialize the collector with errno=%d\n", errno);
        return 1;
    }

    sarah_hd_bios_init(&hd_bios);
    sarah_hd_system_init(&hd_sys);
    sarah_hd_board_init(&hd_board);
    cel_link_init(&cpu_info_list);
    cel_link_init(&disk_list);
    cel_link_init(&net_stat_l1);
    cel_link_init(&net_stat_l2);
    cel_link_init(&bandwidth_list);
    cel_link_init(&ps_list);

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    /* Get and boot parent id */
    boot_ppid = getppid();
#endif

#ifdef _SARAH_GHOST_UDISK_ENABLE
    /* Default the process status to stop for ghost u-disk version */
    booter->status = SARAH_PROC_STOP;
#endif
    

    // so here we are keep firing
    // gathering the data print them and post them to the remove server
    while ( 1 ) {
        if ( booter->status == SARAH_PROC_EXIT ) {
            break;
        } else if ( booter->status == SARAH_PROC_STOP ) {
            _SARAH_STOP_WARNNING("sarah-collector");
            sleep(10);
            continue;
        }

#ifdef _SARAH_ASSERT_PARENT_PROCESS
        /* check the parent process has change then break the loop */
        cur_ppid = getppid();
        if ( cur_ppid != boot_ppid ) {
            _SARAH_PPID_ASSERT_ERR("sarah-collector");
            break;
        }
#endif

        printf("+-Try to collecting data... ");
        /// ---1. Do the statistics and calculations
        sarah_get_hd_info(&hd_bios, &hd_sys, &hd_board);
        sarah_get_cpu_info(&cpu_info_list);     // get CPU information
        sarah_get_ram_stat(&ram_stat);          // get the RAM statistics
        sarah_get_disk_list(                    // get the DISK list
            &disk_list, 
            SARAH_DISK_FILL_PT | SARAH_DISK_FILL_ST, 
            _collector_disk_filter
        );
        sarah_get_process_list(&ps_list);

        if ( stat_to_go == 0 ) {
            // ROUND 1
            // @Note: it will only fire once when first boot up
            gettimeofday(&tv_s, NULL);
            sarah_get_cpu_time(&cpu_time_r1);   // get the cpu time
            sarah_get_net_stat(&net_stat_l1);   // get network statistics
            sleep(stat_duration_sec);           // statistics duration
        }

        // ROUND 2
        sarah_get_cpu_time(&cpu_time_r2);
        sarah_get_net_stat(&net_stat_l2);
        gettimeofday(&tv_e, NULL);

        // cost duration in seconds
        node->last_stat_begin = tv_s.tv_sec;
        node->last_stat_end = tv_e.tv_sec;
        cost_secs  = (tv_e.tv_sec - tv_s.tv_sec);
        cost_secs += (tv_e.tv_usec - tv_s.tv_usec) * 0.000001;

        // CPU occupy and Network bandwidth calculation
        sarah_get_cpu_loadavg(&cpu_loadavg);
        sarah_cal_cpu_stat(&cpu_stat, &cpu_time_r1, &cpu_time_r2);
        _cal_bandwidth_stat(&bandwidth_list, &net_stat_l1, &net_stat_l2, cost_secs);
        printf(" --[Done]\n");


        /// ---2. Make the JSON data package
        printf("+-Try to build the JSON package ... ");
        stat_json = cJSON_CreateObject();
        _make_json_package(
            stat_json, 
            node, 
            &hd_bios, &hd_sys, &hd_board,
            &cpu_info_list, &cpu_stat, &cpu_loadavg,
            &ram_stat,
            &disk_list,
            &bandwidth_list,
            &ps_list
        );
        // json_string = cJSON_Print(stat_json);
        json_string = cJSON_PrintUnformatted(stat_json);
        printf(" --[Done]\n");


        /// ---3. Extra statistics runtime resource clean up
        // CPU info list
        printf("+-Try to clear and free entries ... ");
        sarah_clear_link_entries(&cpu_info_list, free_cpu_info_entry);
        sarah_clear_link_entries(&disk_list, free_disk_entry);
        sarah_clear_link_entries(&net_stat_l1, free_net_stat_entry);
        sarah_clear_link_entries(&net_stat_l2, free_net_stat_entry);
        sarah_clear_link_entries(&bandwidth_list, free_bandwidth_stat_entry);
        sarah_clear_link_entries(&ps_list, free_process_entry);
        cJSON_Delete(stat_json);
        printf(" --[Done]\n");


        /// ---4. print or post the JSON data package 
        //  to the remote target servers ?
        // printf("<<<JSON\n%s\n>>>\n", json_string);
        printf("+-Try to upload the data to the remote server ... \n");
        post_ret = _do_json_upload(node, &collector, json_string);
        printf("|-Url: %s\n", collector.api_url);
        printf("|-Ret: %s\n", collector.strbuff->buffer);
        free(json_string);
        printf("|-Status: [%s]\n", post_ret == 0 ? "Ok" : "Failed");

        /// ---5. interval fire
        // sleep for the specified seconds to enter the next loop
        // Added at 2018/12/06 get the start cpu-time, network 
        //  statistics information
        gettimeofday(&tv_s, NULL);
        sarah_get_cpu_time(&cpu_time_r1);   // get the cpu time
        sarah_get_net_stat(&net_stat_l1);   // get network statistics
        stat_to_go = 1;     // mark to ignore the round 1 statistics
        if ( booter->status == SARAH_PROC_EXIT ) {
            break;
        }

        printf("[Collector]: Waiting for the next invocation ... \n");
        sleep(node->interval);
    }


    // ------ resource clean up work
    /* 
     * CUZ of the tailing invoke of the sarah_get_net_stat 
     * check and free its allocations here.
    */
    if ( net_stat_l1.size > 0) {
        sarah_clear_link_entries(&net_stat_l1, free_net_stat_entry);
    }

    cel_link_destroy(&cpu_info_list, NULL);
    cel_link_destroy(&disk_list, NULL);
    cel_link_destroy(&net_stat_l1, NULL);
    cel_link_destroy(&net_stat_l2, NULL);
    cel_link_destroy(&bandwidth_list, NULL);
    cel_link_destroy(&ps_list, NULL);
    sarah_hd_bios_destroy(&hd_bios);
    sarah_hd_system_destroy(&hd_sys);
    sarah_hd_board_destroy(&hd_board);
    sarah_collector_destroy(&collector);

    return 0;
}
