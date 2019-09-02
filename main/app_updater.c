/**
 * sarah updater client
 *
 * 1, Check the version status the sarah-console
 * 2, Download the new package if there is new version
 * 3, unpack and install the new package
 * 4, restart the sarahos
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <fcntl.h>

#include <curl/curl.h>
#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"


struct sarah_updater_version_entry
{
    float value;        /* version number */
    char md5_sign[33];      /* md5 signature */
};
typedef struct sarah_updater_version_entry sarah_updater_version_t;

struct sarah_updater_package_entry
{
    FILE *fd;                   /* target local file ptr */
    cel_md5_t md5_context;      /* md5 context */
    unsigned char md5_digest[16];   /* digest bytes */
    char md5_sign[33];
};
typedef struct sarah_updater_package_entry sarah_updater_package_t;
SARAH_STATIC void sarah_updater_package_start(sarah_updater_package_t *);
SARAH_STATIC void sarah_updater_package_update(sarah_updater_package_t *, void *, size_t, size_t);
SARAH_STATIC void sarah_updater_package_end(sarah_updater_package_t *);


struct sarah_updater_entry
{
    char *api_url;                          /* Version check url */
    struct curl_slist *version_headers;

    char *download_url;                     /* Package download url */
    struct curl_slist *download_headers;

    unsigned int interval;          /* Check interval in seconds */
    cel_strbuff_t *strbuff;

    char *sdk_dir;      /* SDK base directory */
    char *LSB_file;     /* LSB executable file */
    char *ver_file;     /* version file */
    float version;      /* updater version number */
};
typedef struct sarah_updater_entry sarah_updater_t;
SARAH_STATIC int sarah_updater_init(sarah_node_t *, sarah_updater_t *);
SARAH_STATIC void sarah_updater_destroy(sarah_updater_t *);
SARAH_STATIC void sarah_updater_clear(sarah_updater_t *);
SARAH_STATIC int sarah_updater_set_version(sarah_updater_t *, float);
SARAH_STATIC int sarah_updater_get_version(sarah_updater_t *, float *);


static size_t _get_recv_callback(void *, size_t, size_t, void *);
SARAH_STATIC int _get_latest_version(sarah_updater_t *, sarah_updater_version_t *);
static size_t _download_recv_callback(void *, size_t, size_t, void *);
SARAH_STATIC int _download_package(sarah_updater_t *, sarah_updater_package_t *);
SARAH_STATIC int _install_package(sarah_updater_t *, char *);


SARAH_STATIC void sarah_updater_package_start(sarah_updater_package_t *package)
{
    memset(package, 0x00, sizeof(sarah_updater_package_t));
    package->fd = NULL;
    cel_md5_init(&package->md5_context);
}

SARAH_STATIC void sarah_updater_package_update(
    sarah_updater_package_t *package, void *buffer, size_t size, size_t nmemb)
{
    if ( package->fd != NULL ) {
        fwrite(buffer, size, nmemb, package->fd);
    }

    cel_md5_update(&package->md5_context, (uchar_t *)buffer, size * nmemb);
}

SARAH_STATIC void sarah_updater_package_end(sarah_updater_package_t *package)
{
    cel_md5_final(&package->md5_context, package->md5_digest);
    cel_md5_print(package->md5_digest, package->md5_sign);
}




#define sarah_sdk_package_dir  "SDK"
#define sarah_sdk_pacakge_file "sarah-booter"
#define sarah_sdk_version_file "version"
SARAH_STATIC int sarah_updater_init(sarah_node_t *node, sarah_updater_t *updater)
{
    unsigned int url_len = 0, errno = 0;
    char buffer[256] = {'\0'}, *version_url_base = NULL, *download_url_base = NULL;

    memset(updater, 0x00, sizeof(sarah_updater_t));
    updater->api_url  = NULL;
    updater->version_headers  = NULL;
    updater->download_headers = NULL;
    updater->interval = 120;    /* check the version status every 2 minutes by default */
    updater->strbuff  = NULL;
    updater->sdk_dir  = NULL;
    updater->LSB_file = NULL;
    updater->ver_file = NULL;
    updater->version  = 0.0;


    /* 
     * version check api make: /sdk/linux/download/version 
     * @Note: we use the dl sub-domain for the release package download
     * in case some day we could seperate the download traffic
    */
	version_url_base  = updater_version_url;
	download_url_base = updater_download_url;

    url_len  = strlen(version_url_base);
    url_len += strlen("?node_uid=&app_key=");
    url_len += strlen(node->node_id);
    url_len += strlen(node->config.app_key);
    updater->api_url = (char *) sarah_malloc(url_len + 1);
    if ( updater->api_url == NULL ) {
        errno = 1;
        goto failed;
    }

    sprintf(
        updater->api_url, "%s?node_uid=%s&app_key=%s",
        version_url_base, node->node_id, node->config.app_key
    );
    updater->api_url[url_len] = '\0';


    /* download api make: /sdk/linux/download/tar */
    url_len  = strlen(download_url_base);
    url_len += strlen("?node_uid=&app_key=");
    url_len += strlen(node->node_id);
    url_len += strlen(node->config.app_key);
    updater->download_url = (char *) sarah_malloc(url_len + 1);
    if ( updater->download_url == NULL ) {
        errno = 2;
        goto failed;
    }

    sprintf(
        updater->download_url, "%s?node_uid=%s&app_key=%s",
        download_url_base, node->node_id, node->config.app_key
    );
    updater->download_url[url_len] = '\0';


    /* http request header initialization */
    snprintf(buffer, sizeof(buffer), "Content-Type: application/json");
    updater->version_headers = curl_slist_append(updater->version_headers, buffer);
    snprintf(buffer, sizeof(buffer), "User-Agent: version-check updater/1.0 sarahos/%.02f", sarah_version());
    updater->version_headers = curl_slist_append(updater->version_headers, buffer);

    snprintf(buffer, sizeof(buffer), "Content-Type: application/json");
    updater->download_headers = curl_slist_append(updater->download_headers, buffer);
    snprintf(buffer, sizeof(buffer), "User-Agent: version-download updater/1.0 sarahos/%.02f", sarah_version());
    updater->download_headers = curl_slist_append(updater->download_headers, buffer);


    /* Initialize the string buffer */
    updater->strbuff = new_cel_strbuff_opacity(128);
    if ( updater->strbuff == NULL ) {
        errno = 3;
        goto failed;
    }

    /* Initialize the local package file directory and path */
    url_len  = strlen(node->base_dir);
    url_len += strlen(sarah_sdk_package_dir);
    url_len += 1;
    updater->sdk_dir = (char *) sarah_malloc(url_len + 1);
    if ( updater->sdk_dir == NULL ) {
        errno = 4;
        goto failed;
    }

    /* Check and make the node.base_dir/sdk directory */
    sprintf(updater->sdk_dir, "%s%s/", node->base_dir, sarah_sdk_package_dir);
    if ( sarah_mkdir(updater->sdk_dir, 
        S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) != 0 ) {
        errno = 5;
        goto failed;
    }


    url_len  = strlen(updater->sdk_dir);
    url_len += strlen(sarah_sdk_pacakge_file);
    updater->LSB_file = (char *) sarah_malloc(url_len + 1);
    if ( updater->LSB_file == NULL ) {
        errno = 6;
        goto failed;
    }

    sprintf(updater->LSB_file, "%s%s", updater->sdk_dir, sarah_sdk_pacakge_file);
    url_len  = strlen(updater->sdk_dir);
    url_len += strlen(sarah_sdk_version_file);
    updater->ver_file = (char *) sarah_malloc(url_len + 1);
    if ( updater->ver_file == NULL ) {
        errno = 7;
        goto failed;
    }

    sprintf(updater->ver_file, "%s%s", updater->sdk_dir, sarah_sdk_version_file);
    sarah_updater_get_version(updater, &updater->version);
    // printf(
    //     "sdk_dir=%s, LSB_file=%s, ver_file=%s, version=%f\n", 
    //     updater->sdk_dir, updater->LSB_file, updater->ver_file, updater->version
    // );



failed:
    if ( errno > 0 ) {
        if ( updater->api_url != NULL ) {
            sarah_free(updater->api_url);
            updater->api_url = NULL;
        }

        if ( updater->download_url != NULL ) {
            sarah_free(updater->download_url);
            updater->download_url = NULL;
        }

        if ( updater->version_headers != NULL ) {
            curl_slist_free_all(updater->version_headers);
            updater->version_headers = NULL;
        }

        if ( updater->download_headers != NULL ) {
            curl_slist_free_all(updater->download_headers);
            updater->download_headers = NULL;
        }

        if ( updater->strbuff != NULL ) {
            free_cel_strbuff(&updater->strbuff);
            updater->strbuff = NULL;
        }

        if ( updater->sdk_dir != NULL ) {
            sarah_free(updater->sdk_dir);
            updater->sdk_dir = NULL;
        }

        if ( updater->LSB_file != NULL ) {
            sarah_free(updater->LSB_file);
            updater->LSB_file = NULL;
        }

        if ( updater->ver_file != NULL ) {
            sarah_free(updater->ver_file);
            updater->ver_file = NULL;
        }
    }

    return errno;
}

SARAH_STATIC void sarah_updater_destroy(sarah_updater_t *updater)
{
    if ( updater->api_url != NULL ) {
        sarah_free(updater->api_url);
        updater->api_url = NULL;
    }

    if ( updater->version_headers != NULL ) {
        curl_slist_free_all(updater->version_headers);
        updater->version_headers = NULL;
    }

    if ( updater->download_url != NULL ) {
        sarah_free(updater->download_url);
        updater->download_url = NULL;
    }

    if ( updater->download_headers != NULL ) {
        curl_slist_free_all(updater->download_headers);
        updater->download_headers = NULL;
    }

    if ( updater->strbuff != NULL ) {
        free_cel_strbuff(&updater->strbuff);
        updater->strbuff = NULL;
    }

    if ( updater->sdk_dir != NULL ) {
        sarah_free(updater->sdk_dir);
        updater->sdk_dir = NULL;
    }

    if ( updater->LSB_file != NULL ) {
        sarah_free(updater->LSB_file);
        updater->LSB_file = NULL;
    }

    if ( updater->ver_file != NULL ) {
        sarah_free(updater->ver_file);
        updater->ver_file = NULL;
    }
}

SARAH_STATIC void sarah_updater_clear(sarah_updater_t *updater)
{
    cel_strbuff_clear(updater->strbuff);
}

SARAH_STATIC int sarah_updater_set_version(sarah_updater_t *updater, float version)
{
    char buffer[5] = {'\0'};
    updater->version = version;
    sprintf(buffer, "%.2f", version);
    return sarah_file_put_buffer(updater->ver_file, buffer, 4, 0) > -1 ? 0 : 1;
}

SARAH_STATIC int sarah_updater_get_version(sarah_updater_t *updater, float *version)
{
    char buffer[5] = {'\0'};
    int errno = sarah_file_get_buffer(updater->ver_file, buffer, 4);
    if ( errno > -1 ) {
        *version = (float) atof(buffer);
    }

    return errno > -1 ? 0 : 1;
}



static size_t _get_recv_callback(
    void *buffer, size_t size, size_t nmemb, void *ptr)
{
    sarah_updater_t *updater = (sarah_updater_t *) ptr;
    cel_strbuff_append_from(updater->strbuff, (char *) buffer, size * nmemb, 1);
    return size * nmemb;
}

SARAH_STATIC int _get_latest_version(sarah_updater_t *updater, sarah_updater_version_t *version)
{
    long http_code;
    int ret_code = 0, length;

    CURL *curl = NULL;
    cJSON *json_result = NULL, *data = NULL, *cursor = NULL;

    // Initialize the curl and set the options
    curl = curl_easy_init();
    if ( curl == NULL ) {
        curl_global_cleanup();
        return 1;
    }

    sarah_updater_clear(updater);
    curl_easy_setopt(curl, CURLOPT_URL, updater->api_url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, updater->version_headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000);          // 10 seconds
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);    // 1 hour cache for DNS
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _get_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, updater);

    // Perform the http request
    if ( curl_easy_perform(curl) != CURLE_OK ) {
        ret_code = 2;
        goto failed;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if ( http_code != 200 ) {
        ret_code = 2;
        goto failed;
    }

    // Parse the api response and create a task entry
    // printf("version_check_url: %s\n", updater->strbuff->buffer);
    json_result = cJSON_Parse(updater->strbuff->buffer);
    if ( json_result == NULL ) {
        ret_code = 3;
        goto failed;
    }

    cursor = cJSON_GetObjectItem(json_result, "errno");
    if ( ! cJSON_HasObjectItem(json_result, "data") 
        || cursor == NULL || cursor->valueint != 0 ) {
        ret_code = 10 + cursor->valueint;
        goto failed;
    }

    data = cJSON_GetObjectItem(json_result, "data");
    cursor = cJSON_GetObjectItem(data, "version");
    if ( cursor == NULL || cursor->type != cJSON_Number ) {
        ret_code = 5;
        goto failed;
    }

    version->value = (float) cursor->valuedouble;
    cursor = cJSON_GetObjectItem(data, "md5_sign");
    if ( cursor == NULL || cursor->type != cJSON_String ) {
        ret_code = 6;
        goto failed;
    }

    length = strlen(cursor->valuestring);
    memcpy(version->md5_sign, cursor->valuestring, length);
    version->md5_sign[length] = '\0';


failed:
    if ( json_result != NULL ) {
        cJSON_Delete(json_result);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return ret_code;
}


static size_t _download_recv_callback(
    void *buffer, size_t size, size_t nmemb, void *ptr)
{
    sarah_updater_package_t *package = (sarah_updater_package_t *) (ptr);
    sarah_updater_package_update(package, buffer, size, nmemb);
    return size * nmemb;
}


/**
 * internal function to download the remote sarah os package
 *
 * @param   updater
 * @param   package
 * @return  int
*/
SARAH_STATIC int _download_package(sarah_updater_t *updater, sarah_updater_package_t *package)
{
    long http_code;
    int ret_code = 0, fno, _flock = -1;
    CURL *curl = NULL;

    /* Try to open and lock the local sdk file */
    sarah_updater_package_start(package);
    if ( (package->fd = fopen(updater->LSB_file, "wb")) == NULL ) {
        return 1;
    }

    fno = fileno(package->fd);
    if ( (_flock = flock(fno, LOCK_EX)) == -1 ) {
        ret_code = 2;
        goto failed;
    }


    // Initialize the curl and set the options
    curl = curl_easy_init();
    if ( curl == NULL ) {
        ret_code = 2;
        goto failed;
    }

    curl_easy_setopt(curl, CURLOPT_URL, updater->download_url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, updater->download_headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1800000);        // 30 minutes
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);    // 1 hour cache for DNS
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _download_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, package);

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

failed:
    if ( package->fd != NULL ) {
        if ( _flock > -1 ) {
            flock(fno, LOCK_UN);
        }

        _flock = -1;
        fclose(package->fd);
    }

    if ( curl != NULL ) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }

    curl_global_cleanup();
    sarah_updater_package_end(package);

    return ret_code;
}


SARAH_STATIC int _install_package(sarah_updater_t *updater, char *bin_file)
{
    int fno_src = -1, fno_dst = -1, _lock_src = -1, _lock_dst = -1;
    int errno = 0, rd_errno, wr_errno;
    char buffer[1024] = {'\0'};

    if ( (fno_src = open(updater->LSB_file, O_RDONLY)) == -1 ) {
        return 1;
    }

    if ( (_lock_src = flock(fno_src, LOCK_SH)) == -1 ) {
        errno = 2;
        goto failed;
    }


    /* Remove the original booter original file 
     * then copy the sarah-booter to the target 
    */
    if ( access(bin_file, F_OK) == 0 ) {
        if ( remove(bin_file) == -1 ) {
            errno = 3;
            goto failed;
        }
    }

    if ( (fno_dst = open(bin_file, 
        O_WRONLY | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH)) == -1 ) {
        errno = 4;
        goto failed;
    }

    if ( (_lock_dst = flock(fno_dst, LOCK_EX)) == -1 ) {
        errno = 5;
        goto failed;
    }


    /* Try to copy the binary data from source file to target file */
    while ( 1 ) {
        rd_errno = read(fno_src, buffer, sizeof(buffer));
        if ( rd_errno == -1 ) {
            errno = 6;
            break;
        } else if ( rd_errno == 0 ) {
            break;
        }

        wr_errno = write(fno_dst, buffer, sizeof(buffer));
        if ( wr_errno == -1 ) {
            errno = 7;
            break;
        } else if ( wr_errno == 0 ) {
            break;
        }
    }

    /* 
     * restart the service 
     * errno = system("service sarah restart");
    */


failed:
    if ( _lock_src != -1 ) {
        flock(fno_src, LOCK_UN);
    }

    if ( _lock_dst != -1 ) {
        flock(fno_dst, LOCK_UN);
    }

    if ( fno_src > -1 ) {
        close(fno_src);
    }

    if ( fno_dst > -1 ) {
        close(fno_dst);
    }

    return errno;
}


/**
 * sarah updater application implementation
 *
 * @param   booter
 * @param   node
 * @return  int
*/
SARAH_API int sarah_updater_application(sarah_booter_t *booter, sarah_node_t *node)
{
    float c_version_no;
    int errno, download_status, _counter;

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    pid_t boot_ppid, cur_ppid;
#endif

    sarah_updater_t updater;
    sarah_updater_version_t version;
    sarah_updater_package_t package;


    // --- Basic initialization
    _counter = 0;
    download_status = 0;
    c_version_no = sarah_version(); 
    if ( (errno = sarah_updater_init(node, &updater)) != 0 ) {
        printf("[Error]: Failed to initialize the updater with errno=%d\n", errno);
        return 0;
    }

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    /* Get and boot parent id */
    boot_ppid = getppid();
#endif

#ifdef _SARAH_GHOST_UDISK_ENABLE
    /* Default the process status to stop for ghost u-disk version */
    booter->status = SARAH_PROC_STOP;
#endif


    while ( 1 ) {
        if ( booter->status == SARAH_PROC_EXIT ) {
            break;
        } else if ( booter->status == SARAH_PROC_STOP ) {
            _SARAH_STOP_WARNNING("sarah-updater");
            sleep(10);
            continue;
        }

#ifdef _SARAH_ASSERT_PARENT_PROCESS
        /* check the parent process has change then break the loop */
        cur_ppid = getppid();
        if ( cur_ppid != boot_ppid ) {
            _SARAH_PPID_ASSERT_ERR("sarah-updater");
            break;
        }
#endif

        // 1, check if there is any new version available
        printf("+-Try to check the latest version ... \n");
        version.value = 0;
        errno = _get_latest_version(&updater, &version);
        if ( errno != 0 ) {
            printf("|-[Error]: Failed with errno=%d\n", errno);
            goto loop;
        }

        printf("[Info]: latest version=%f\n", version.value);
        if ( version.value <= c_version_no || version.value <= updater.version ) {
            goto loop;
        }


        /* 2, Try to download the SDK package 
         * @Note: Pay attension to the download_status design
         * if the downloaded and install failed we will marked it
         * in case of madly repeatly traffic waste.
         * 
         * If the installation was completed and download_status will
         * be reset to 0
        */
        printf("+-Try to download the latest SDK package ... \n");
        if ( download_status == 1 ) {
            printf("[Info]: SDK package already stored at %s\n", updater.LSB_file);
        } else {
            errno = _download_package(&updater, &package);
            if ( errno != 0 ) {
                printf("|-[Error]: Failed with errno=%d\n", errno);
                goto loop;
            }

            download_status = 1;
            printf("[Info]: Done with SDK package stored at %s\n", updater.LSB_file);


            printf("+-Try to verify the package signature ... \n");
            if ( strncmp(version.md5_sign, package.md5_sign, 32) != 0 ) {
                printf("|-[Error]: Verification failed, version.sign=%s, package.sign=%s\n", version.md5_sign, package.md5_sign);
                goto loop;
            }
            printf("[Info]: Verification passed\n");
        }


        // 3, Unpack and install the SDK package
        printf("+-Try to install the SDK package ... \n");
        errno = _install_package(&updater, booter->exe_file);
        if ( errno != 0 ) {
            printf("|-[Error]: Failed with errno=%d\n", errno);
            goto loop;
        }
        printf("[Info]: installed successfully\n");

        c_version_no = version.value;
        download_status = 0;
        sarah_updater_set_version(&updater, version.value);

        // 4, restart the sarah-booter
        printf("+-Try to restart the sarah booter ... \n");
        for ( _counter = 0; _counter < 5; _counter++ ) {
            if ( kill(booter->pid, SARAH_SIGNAL_RESTART) == 0 ) {
                printf("[Info]: restart signal sended\n");
                break;
            }
            
            // well, lets wait for a second
            sleep(1);
        }

        // 5, auto exit if the exit signal sended
        // if ( _counter < 5 ) {
        //     break;
        // }

loop:
        if ( booter->status == SARAH_PROC_EXIT ) {
            break;
        }

        printf("[Updater]: Waiting for the next invocation ... \n");
        sleep(updater.interval);
    }

    // --- Basic resource clean up
    sarah_updater_destroy(&updater);

    return 0;
}
