/**
 * sarah executor client
 *
 * 1, Pull the command package from the sarah-console
 * 2, Check the validity of the command package.
 * 3, Execute the command package.
 * 4, Report the status according to the setting of the  package
 *
 * @Author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"


struct sarah_task_config_entry
{
    char *api_url;
    struct curl_slist *headers;
    cel_strbuff_t *strbuff;
};
typedef struct sarah_task_config_entry sarah_task_config_t;


SARAH_STATIC int sarah_task_config_init(sarah_node_t *, sarah_task_config_t *);
SARAH_STATIC void sarah_task_config_destroy(sarah_task_config_t *);
SARAH_STATIC void sarah_task_config_clear(sarah_task_config_t *);
SARAH_STATIC int _pull_remote_task(sarah_task_config_t *, sarah_task_t **);
static size_t _task_recv_callback(void *, size_t, size_t, void *);


SARAH_STATIC int sarah_task_config_init(sarah_node_t *node, sarah_task_config_t *config)
{
    unsigned int url_len = 0, errno = 0;
    char buffer[256] = {'\0'}, *url_base = NULL;

    memset(config, 0x00, sizeof(sarah_task_config_t));
    config->api_url = NULL;
    config->headers = NULL;
    config->strbuff = NULL;

    /* 
     * remote command pull url make
     * command pull: /api/node/command/export/pull?node_uid=&app_key=
     */
    url_base = executor_pull_url;

    url_len  = strlen(url_base);
    url_len += strlen("?node_uid=&app_key=");
    url_len += strlen(node->node_id);
    url_len += strlen(node->config.app_key);
    config->api_url = (char *) sarah_malloc(url_len + 1);
    if ( config->api_url == NULL ) {
        errno = 1;
        goto failed;
    }

    sprintf(
        config->api_url, "%s?node_uid=%s&app_key=%s", 
        url_base, node->node_id, node->config.app_key
    );
    config->api_url[url_len] = '\0';
    // printf("api_url=%s\n", config->api_url);


    // http request header initialization
    snprintf(buffer, sizeof(buffer), "Content-Type: application/json");
    config->headers = curl_slist_append(config->headers, buffer);
    snprintf(buffer, sizeof(buffer), "User-Agent: task-pull executor/1.0 sarahos/%.02f", sarah_version());
    config->headers = curl_slist_append(config->headers, buffer);


    // Initialize the string buffer
    config->strbuff = new_cel_strbuff_opacity(256);
    if ( config->strbuff == NULL ) {
        errno = 2;
        goto failed;
    }


failed:
    if ( errno > 0 ) {
        if ( config->api_url != NULL ) {
            sarah_free(config->api_url);
            config->api_url = NULL;
        }

        if ( config->headers != NULL ) {
            curl_slist_free_all(config->headers);
            config->headers = NULL;
        }

        if ( config->strbuff != NULL ) {
            free_cel_strbuff(&config->strbuff);
            config->strbuff = NULL;
        }
    }

    return errno;
}

SARAH_STATIC void sarah_task_config_destroy(sarah_task_config_t *config)
{
    if ( config->api_url != NULL ) {
        sarah_free(config->api_url);
        config->api_url = NULL;
    }

    if ( config->headers != NULL ) {
        curl_slist_free_all(config->headers);
        config->headers = NULL;
    }

    if ( config->strbuff != NULL ) {
        free_cel_strbuff(&config->strbuff);
        config->strbuff = NULL;
    }
}

SARAH_STATIC void sarah_task_config_clear(sarah_task_config_t *config)
{
    cel_strbuff_clear(config->strbuff);
}



static size_t _task_recv_callback(
    void *buffer, size_t size, size_t nmemb, void *ptr)
{
    sarah_task_config_t *config = (sarah_task_config_t *) ptr;
    cel_strbuff_append_from(config->strbuff, (char *) buffer, size * nmemb, 1);
    return size * nmemb;
}

/**
 * internal function to pull a remote task from the console terminal
 *
 * @param   config
 * @param   task
 * @return  Mixed NULL or sarah_task_t *
*/
SARAH_STATIC int _pull_remote_task(sarah_task_config_t *config, sarah_task_t **task)
{
    long http_code;
    int ret_code = 0;

    CURL *curl = NULL;
    cJSON *json_result = NULL, *cursor = NULL;

    // Initialize the curl and set the options
    curl = curl_easy_init();
    if ( curl == NULL ) {
        curl_global_cleanup();
        return 1;
    }

    sarah_task_config_clear(config);
    curl_easy_setopt(curl, CURLOPT_URL, config->api_url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);          // Follow http redirect
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);               // max redirect times
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, config->headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000);          // 10 seconds
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);    // 1 hour cache for DNS
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _task_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, config);

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
    // printf("cmd_pull_str: %s\n", config->strbuff->buffer);
    json_result = cJSON_Parse(config->strbuff->buffer);
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

    // create a new task entry and initialize it from the JSON
    *task = new_task_entry();
    if ( sarah_task_init_from_json(*task, 
        cJSON_GetObjectItem(json_result, "data")) != 0 ) {
        free_task_entry((void **)task);
        ret_code = 3;
        goto failed;
    }

failed:
    if ( json_result != NULL ) {
        cJSON_Delete(json_result);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return ret_code;
}


#define norm_pull_duration 3
#define busy_pull_duration 1
#define max_busy_count 12

/**
 * sarah script executor application implementation
 *
 * @param   booter
 * @param   node
 * @return  int
*/
SARAH_API int sarah_executor_application(sarah_booter_t *booter, sarah_node_t *node)
{
    int errno, sync_errno;
    int pull_duration_sec, empty_count;
    struct timeval tv;

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    pid_t boot_ppid, cur_ppid;
#endif

    sarah_task_config_t config;
    sarah_executor_t executor;
    sarah_task_t *task = NULL;

    pid_t pid;
    sarah_string_executor_fn_t dostring;

    // -- Basic initialization
    if ( sarah_task_config_init(node, &config) != 0 ) {
        printf("[Error]: Failed to initialized the task config\n");
        return 1;
    }

    if ( sarah_executor_init(node, &executor) != 0 ) {
        printf("[Error]: Failed to initialized the executor\n");
        return 1;
    }

    /* set the default working directly to `/` by default */
    if ( chdir("/") == -1 ) {
        printf("[Warning]: Failed to set the working directly");
    }

    // sarah_executor_set_maxdatalen(&executor, 1024 * 20);

#ifdef _SARAH_ASSERT_PARENT_PROCESS
    /* Get and boot parent id */
    boot_ppid = getppid();
#endif

#ifdef _SARAH_GHOST_UDISK_ENABLE
    /* Default the process status to stop for ghost u-disk version */
    booter->status = SARAH_PROC_STOP;
#endif

    /**
     * keep pull task from the remote terminal and execute it
     *  if there is one available then push the execution result.
    */ 
    empty_count = 0;
    pull_duration_sec = norm_pull_duration;
    while ( 1 ) {
        if ( booter->status == SARAH_PROC_EXIT ) {
            break;
        } else if ( booter->status == SARAH_PROC_STOP ) {
            _SARAH_STOP_WARNNING("sarah-executor");
            sleep(10);
            continue;
        }

#ifdef _SARAH_ASSERT_PARENT_PROCESS
        /* check the parent process has change then break the loop */
        cur_ppid = getppid();
        if ( cur_ppid != boot_ppid ) {
            _SARAH_PPID_ASSERT_ERR("sarah-executor");
            break;
        }
#endif

        gettimeofday(&tv, NULL);
        sarah_executor_clear(&executor);

        /* child process wait and Recycling */
        while ( waitpid(-1, NULL, WNOHANG) > 0 );

        // --- 1, pull task from the remote console
        printf("+-[%ld]: Try to pull the task ... ", tv.tv_sec);
        errno = _pull_remote_task(&config, &task);
        if ( errno != 0 ) {
            if ( empty_count++ >= max_busy_count ) {
                empty_count = 0;
                pull_duration_sec = norm_pull_duration;
            }

            printf(" --[errno=%d]\n", errno);
            goto cleanup;
        }
        printf(" --[Ok]\n");


        // --- 2, validity the task
        // sarah_print_task(task);
        printf("+-Check the validity of the task ... ");
        if ( sarah_task_validity(node, task) != 0 ) {
            printf(" --[Invalid]\n");
            goto cleanup;
        }
        printf(" --[Ok]\n");


        // --- 3, task execution
        /* 
         * Update and reduce the pull duration to 1 second
         * If we found one task, and next it will keep firing
         * every 1 second until there is no 
        */
        pull_duration_sec = busy_pull_duration;
        sarah_executor_set_task(&executor, task);
        printf("+-Try to execute the task ... \n");

        /*
         * @Note We will fork a new process for the execution.
         * The child process should do the resource clean up.
         */
        if ( strcmp(task->engine, "bash") == 0 ) {
            dostring = script_bash_dostring;
        } else if ( strcmp(task->engine, "lua") == 0 ) {
            dostring = script_lua_dostring;
        } else if ( strcmp(task->engine, "sarah") == 0 ) {
            dostring = script_sarah_dostring;
        } else {
            goto cleanup;
        }

        if ( (pid = fork()) < 0 ) {
            printf("|--[Error]: Failed to fork a new process.\n");
        } else if ( pid == 0 ) {
            if ( (task->sync_mask & sarah_task_sync_start) != 0 ) {
                sarah_executor_set_status(&executor, sarah_task_executing, 0.0);
                sarah_executor_sync_status(&executor);
            }

            errno = dostring(&executor, task->cmd_text, task->cmd_argv);

            // --- 4, task result synchronization
            sync_errno = -1;
            if ( errno == 0 ) {
                sarah_executor_set_status(&executor, sarah_task_completed, 1.0);
                if ( (task->sync_mask & sarah_task_sync_completed) != 0 ) {
                    sync_errno = sarah_executor_sync_status(&executor);
                }
            } else {
                sarah_executor_set_status(&executor, sarah_task_err, executor.progress);
                if ( (task->sync_mask & sarah_task_sync_err) != 0 ) {
                    sync_errno = sarah_executor_sync_status(&executor);
                }
            }

            printf(
                "|-[Info]: task.id=%lld, execution.errno=%d, sync.errno=%d\n", 
                task->id, errno, sync_errno
            );

            // --- 5, resource clean up
            free_task_entry((void **)&task);
            sarah_executor_destroy(&executor);
            sarah_task_config_destroy(&config);
            return 0;
        }

cleanup:
        printf("[Executor]: Waiting for the next invocation ...\n");
        free_task_entry((void **) &task);
        if ( booter->status == SARAH_PROC_EXIT ) {
            break;
        }

        sleep(pull_duration_sec);
    }


    // --- Basic resource clean up
    sarah_executor_destroy(&executor);
    sarah_task_config_destroy(&config);

    return 0;
}
