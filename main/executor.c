/**
 * sarah executor implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "sarah.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

static size_t _post_recv_callback(void *, size_t, size_t, void *);

/**
 * task function implementation
*/
SARAH_API sarah_task_t *new_task_entry()
{
    sarah_task_t *task = (sarah_task_t *) sarah_malloc(sizeof(sarah_task_t));
    memset(task, 0x00, sizeof(sarah_task_t));

    task->engine   = NULL;
    task->cmd_argv = NULL;
    task->cmd_text = NULL;
    task->sign     = NULL;

    return task;
}

SARAH_API void free_task_entry(void **ptr)
{
    sarah_task_t **tptr = (sarah_task_t **) ptr;
    sarah_task_t *task = *tptr;
    if ( task != NULL ) {
        if ( task->engine != NULL ) {
            free(task->engine);
            task->engine = NULL;
        }

        if ( task->cmd_argv != NULL ) {
            free(task->cmd_argv);
            task->cmd_argv = NULL;
        }

        if ( task->cmd_text != NULL ) {
            free(task->cmd_text);
            task->cmd_text = NULL;
        }

        if ( task->sign != NULL ) {
            free(task->sign);
            task->sign = NULL;
        }

        sarah_free(task);
        *tptr = NULL;
    }
}

SARAH_API void sarah_print_task(sarah_task_t *task)
{
    printf(
        "id=%lld, created_at=%u, sync_mask=%u, engine=%s, sign=%s, argv=%s, content=%s\n",
        task->id, task->created_at, task->sync_mask, 
        task->engine, task->sign, task->cmd_argv, task->cmd_text
    );
}

/**
 * initialize the task entry from the specified JSON package
 *  which usually from the remote task pull interface defined
 *  in sarah-executor @see sarah-executor#_pull_remote_task(sarah_task_config_t *)
 *
 * @Note the passed-in data cJSON Object will be deleted after this
 * So all the string value should be copied.
 *
 * @param   task
 * @param   data
 * @return  int
*/
SARAH_API int sarah_task_init_from_json(sarah_task_t *task, cJSON *data)
{
    cJSON *cursor = NULL;

    if ( data == NULL ) {
        return 1;
    }

    cursor = cJSON_GetObjectItem(data, "id_str");
    if ( cursor == NULL ) {
        return 2;
    }

    task->id = (unsigned long long int) strtoll(cursor->valuestring, NULL, 10);
    // printf("valuedouble=%lf, id=%llu\n", cursor->valuedouble, task->id);

    cursor = cJSON_GetObjectItem(data, "engine");
    if ( cursor == NULL ) {
        return 3;
    }

    task->engine = strdup(cursor->valuestring);
    cursor = cJSON_GetObjectItem(data, "cmd_text");
    if ( cursor == NULL ) {
        return 4;
    }

    task->cmd_text = strdup(cursor->valuestring);
    cursor = cJSON_GetObjectItem(data, "sign");
    if ( cursor == NULL ) {
        return 5;
    }

    task->sign = strdup(cursor->valuestring);
    cursor = cJSON_GetObjectItem(data, "cmd_argv");
    if ( cursor != NULL ) {
        task->cmd_argv = strdup(cursor->valuestring);
    }

    cursor = cJSON_GetObjectItem(data, "created_at");
    if ( cursor != NULL ) {
        task->created_at = cursor->valueint;
    }

    cursor = cJSON_GetObjectItem(data, "sync_mask");
    if ( cursor != NULL ) {
        task->sync_mask = cursor->valueint;
    }

    return 0;
}

/**
 * check wether the task is validity or not.
 *
 * @param   node
 * @param   task
 * @return  int
*/
SARAH_API int sarah_task_validity(sarah_node_t *node, sarah_task_t *task)
{
    // task sign validity
    cel_md5_t md5_context;
    unsigned char digest[16] = {'\0'};
    char seed[11] = {'\0'}, md5_str[33] = {'\0'};

    // calculate the md5 signature
    // md5("{node_uid}{cmd}{engine}{timestamp}")
    sprintf(seed, "%010u", task->created_at);
    cel_md5_init(&md5_context);
    cel_md5_update(&md5_context, (uchar_t *)node->node_id, strlen(node->node_id));
    cel_md5_update(&md5_context, (uchar_t *)task->cmd_text, strlen(task->cmd_text));
    cel_md5_update(&md5_context, (uchar_t *)task->engine, strlen(task->engine));
    cel_md5_update(&md5_context, (uchar_t *)seed, strlen(seed));
    cel_md5_final(&md5_context, digest);
    cel_md5_print(digest, md5_str);

    return strcmp(task->sign, md5_str) == 0 ? 0 : 1;
}




/**
 * create a new executor entry
*/
SARAH_API sarah_executor_t *new_executor_entry(sarah_node_t *node)
{
    sarah_executor_t *executor = (sarah_executor_t *) sarah_malloc(sizeof(sarah_executor_t));
    if ( executor == NULL ) {
        SARAH_ALLOCATE_ERROR(__FUNCTION__, sizeof(sarah_executor_t));
    }

    sarah_executor_init(node, executor);
    return executor;
}

SARAH_API void free_executor_entry(void **ptr)
{
    sarah_executor_t **executor = (sarah_executor_t **) ptr;
    if ( *executor != NULL ) {
        sarah_executor_destroy(*executor);
        sarah_free(*executor);
        *ptr = NULL;
    }
}

SARAH_API int sarah_executor_init(sarah_node_t *node, sarah_executor_t *executor)
{
    unsigned int url_len = 0, errno = 0;
    char buffer[256] = {'\0'}, *sync_base = NULL;
    char *cmd_id = "6639193343686609942", *sign = "7f9106d9a1fa6d0f3550cc1783342638";

    // basic initialization
    memset(executor, 0x00, sizeof(sarah_executor_t));
    executor->node = node;
    executor->dostring = NULL;
    executor->dofile = NULL;
    executor->sync_url = NULL;
    executor->headers  = NULL;

    executor->status   = 0;
    executor->progress = 0.00;
    executor->max_datalen = 1024 * 64 + 1;      // 64KB for the most by default
    executor->errno  = 0;
    executor->attach = 0;

    /* 
     * remote command pull url make
     * command pull: /api/node/command/export/pull?node_uid=&app_key=
     */
    sync_base = executor_sync_url;

    /* 
     * remote command status synchronization url make
     * command status: /api/node/command/export/notify?node_uid=&app_key=&sign=&cmd_id=
     */
    url_len  = strlen(sync_base);
    url_len += strlen("?node_uid=&app_key=");
    url_len += strlen(node->node_id);
    url_len += strlen(node->config.app_key);
    url_len += strlen("&sign=");
    executor->sign_offset = url_len;
    url_len += strlen(sign);
    url_len += strlen("&cmd_id=");
    executor->cmid_offset = url_len;
    url_len += strlen(cmd_id);
    executor->sync_url = (char *) sarah_malloc(url_len + 1);
    if ( executor->sync_url == NULL ) {
        errno = 2;
        goto failed;
    }

    sprintf(
        executor->sync_url, "%s?node_uid=%s&app_key=%s&sign=%s&cmd_id=%s",
        sync_base, node->node_id, node->config.app_key, sign, cmd_id
    );
    executor->sync_url[url_len] = '\0';

    // Initialize the http header
    snprintf(buffer, sizeof(buffer), "Content-Type: application/json");
    executor->headers = curl_slist_append(executor->headers, buffer);
    snprintf(buffer, sizeof(buffer), "User-Agent: task-sync executor/1.0 sarahos/%f", sarah_version());
    executor->headers = curl_slist_append(executor->headers, buffer);


    // initialize the http response, error and the data string buffer
    if ( cel_strbuff_init(&executor->res_buffer, 128, NULL) == 0 ) {
        errno = 3;
        goto failed;
    }

    if ( cel_strbuff_init(&executor->error_buffer, 256, NULL) == 0 ) {
        errno = 4;
        goto failed;
    }

    if ( cel_strbuff_init(&executor->data_buffer, executor->max_datalen, NULL) == 0 ) {
        errno = 5;
        goto failed;
    }


failed:
    if ( errno > 0 ) {
        if ( executor->sync_url != NULL ) {
            sarah_free(executor->sync_url);
            executor->sync_url = NULL;
        }

        if ( executor->headers != NULL ) {
            curl_slist_free_all(executor->headers);
            executor->headers = NULL;
        }

        if ( errno > 3 ) {
            cel_strbuff_destroy(&executor->res_buffer);
        }

        if ( errno > 4 ) {
            cel_strbuff_destroy(&executor->error_buffer);
        }
    }

    return errno;
}


SARAH_API void sarah_executor_destroy(sarah_executor_t *executor)
{
    if ( executor->sync_url != NULL ) {
        sarah_free(executor->sync_url);
        executor->sync_url = NULL;
    }

    if ( executor->headers != NULL ) {
        curl_slist_free_all(executor->headers);
        executor->headers = NULL;
    }

    cel_strbuff_destroy(&executor->res_buffer);
    cel_strbuff_destroy(&executor->error_buffer);
    cel_strbuff_destroy(&executor->data_buffer);
}


/**
 * clear the current executor for later use
 *
 * @param   executor
*/
SARAH_API void sarah_executor_clear(sarah_executor_t *executor)
{
    // basic attribute
    executor->task = NULL;
    executor->status = 0;
    executor->progress = 0;
    executor->errno = 0;

    cel_strbuff_clear(&executor->error_buffer);
    cel_strbuff_clear(&executor->data_buffer);
}

SARAH_API void sarah_executor_set_task(sarah_executor_t *executor, sarah_task_t *task)
{
    executor->task = task;
}

/**
 * set the syntax of the executor
 *  could be any VM engine like: bash, lua, php, golang eg ...
 *
 * @param   executor
 * @param   syntax
*/
SARAH_API int sarah_executor_set_syntax(sarah_executor_t *executor, char *syntax)
{
    if ( strcmp(syntax, "bash") == 0 ) {
        executor->dostring = script_bash_dostring;
        executor->dofile = script_bash_dofile;
    } else if ( strcmp(syntax, "lua") == 0 ) {
        executor->dostring = script_lua_dostring;
        executor->dofile = script_lua_dofile;
    } else if ( strcmp(syntax, "sarah") == 0 ) {
        executor->dostring = script_sarah_dostring;
        executor->dofile = script_sarah_dofile;
    } else {
        return 1;
    }

    return 0;
}

/**
 * set the max data length limitation of the current executor.
 *
 * @param   executor
 * @param   maxlen
*/
SARAH_API void sarah_executor_set_maxdatalen(sarah_executor_t *executor, int maxlen)
{
    executor->max_datalen = maxlen;
}

/**
 * load the specified string and try to execute the block with the specified engine
 *  define or setted by #sarah_executor_set_syntax interface.
 *
 * @param   executor
 * @param   str
 * @param   argv
*/
SARAH_API int sarah_executor_dostring(sarah_executor_t *executor, char *str, char *argv)
{
    if ( executor->dostring == NULL ) {
        return -1;
    }

    return executor->dostring(executor, str, argv);
}

/**
 * execute the specified task
 *
 * @param   executor
 * @param   task
*/
SARAH_API int sarah_executor_dotask(sarah_executor_t *executor, sarah_task_t *task)
{
    if ( strcmp(task->engine, "bash") == 0 ) {
        return script_bash_dostring(executor, task->cmd_text, task->cmd_argv);
    } else if ( strcmp(task->engine, "lua") == 0 ) {
        return script_lua_dostring(executor, task->cmd_text, task->cmd_argv);
    }

    return -1;
}

/**
 * load and execute the specified file with the specified engine define
 *  or setted by the #sarah_executor_set_syntax interface.
 *
 * @param   executor
 * @param   file
 * @param   argv
*/
SARAH_API int sarah_executor_dofile(sarah_executor_t *executor, char *file, char *argv)
{
    if ( executor->dofile == NULL ) {
        return -1;
    }

    return executor->dofile(executor, file, argv);
}

SARAH_API int sarah_executor_data_available(sarah_executor_t *executor, int len)
{
    return (executor->data_buffer.size + len > executor->max_datalen) ? 1 : 0;
}

SARAH_API int sarah_executor_data_append(sarah_executor_t *executor, char *data)
{
    int dlen, new_len, errno;

    if ( executor->data_buffer.size >= executor->max_datalen ) {
        return 0;
    }

    // check and override the data length to append
    dlen = strlen(data);
    new_len = executor->data_buffer.size + dlen;
    if ( new_len > executor->max_datalen ) {
        dlen = executor->max_datalen - executor->data_buffer.size;
    }

    errno = cel_strbuff_append_from(&executor->data_buffer, data, dlen, 1);
    return errno == 0 ? dlen : 0;
}

SARAH_API int sarah_executor_data_insert(sarah_executor_t *executor, int idx, char *data)
{
    int dlen = strlen(data), new_len, errno;

    if ( executor->data_buffer.size >= executor->max_datalen ) {
        return 0;
    }

    dlen = strlen(data);
    new_len = executor->data_buffer.size + dlen;
    if ( new_len > executor->max_datalen ) {
        dlen = executor->max_datalen - executor->data_buffer.size;
    }

    errno = cel_strbuff_insert_from(&executor->data_buffer, idx, data, dlen, 1);
    return errno == 0 ? dlen : 0;
}

SARAH_API void sarah_executor_data_clear(sarah_executor_t *executor)
{
    cel_strbuff_clear(&executor->data_buffer);
}

SARAH_API int sarah_executor_error_append(sarah_executor_t *executor, char *errstr)
{
    return cel_strbuff_append(&executor->error_buffer, errstr, 1) == 0 ? 1 : 0;
}

SARAH_API int sarah_executor_error_insert(sarah_executor_t *executor, int idx, char *errstr)
{
    return cel_strbuff_insert(&executor->error_buffer, idx, errstr, 1) == 0 ? 1 : 0;
}

SARAH_API void sarah_executor_error_clear(sarah_executor_t *executor)
{
    cel_strbuff_clear(&executor->error_buffer);
}

SARAH_API void sarah_executor_set_status(sarah_executor_t *executor, int status, float progress)
{
    executor->status = status;
    executor->progress = progress;
}

// execution result fetch
SARAH_API char *sarah_executor_get_data(sarah_executor_t *executor)
{
    return executor->data_buffer.buffer;
}

SARAH_API int sarah_executor_get_datalen(sarah_executor_t *executor)
{
    return executor->data_buffer.size;
}

SARAH_API int sarah_executor_get_errlen(sarah_executor_t *executor)
{
    return executor->error_buffer.size;
}

SARAH_API char *sarah_executor_get_errstr(sarah_executor_t *executor)
{
    return executor->error_buffer.buffer;
}

SARAH_API void sarah_executor_set_errno(sarah_executor_t *executor, int errno)
{
    executor->errno = errno;
}

SARAH_API int sarah_executor_get_errno(sarah_executor_t *executor)
{
    return executor->errno;
}

SARAH_API void sarah_executor_set_attach(sarah_executor_t *executor, unsigned int attach)
{
    executor->attach = attach;
}

SARAH_API unsigned int sarah_executor_get_attach(sarah_executor_t *executor)
{
    return executor->attach;
}

static size_t _post_recv_callback(
    void *buffer, size_t size, size_t nmemb, void *ptr)
{
    sarah_executor_t *executor = (sarah_executor_t *) ptr;
    cel_strbuff_append_from(&executor->res_buffer, (char *) buffer, size * nmemb, 1);
    return size * nmemb;
}

/*
 * This will make a http request
 * and synchronize the current status of the execution to the remote server
 *
 * @param   executor
 * @return  int 0 for succeed and none 0 for failed
*/
SARAH_API int sarah_executor_sync_status(sarah_executor_t *executor)
{
    long http_code;
    int ret_code = 0;
    char id_buffer[20] = {'\0'};

    cel_md5_t md5_context;
    unsigned char digest[16] = {'\0'};
    char md5_str[33] = {'\0'};

    CURL *curl = NULL;
    cJSON *json_pack = NULL, *json_data = NULL;
    cJSON *json_result = NULL, *cursor = NULL;
    char *json_string = NULL;

    if ( executor->task == NULL ) {
        return 1;
    }

    // Initialize the curl and set the options
    curl = curl_easy_init();
    if ( curl == NULL ) {
        curl_global_cleanup();
        return 1;
    }

    /* Make the JSON data package string */
    json_pack = cJSON_CreateObject();
    // version
    cursor = cJSON_CreateNumber(sarah_version());
    cJSON_AddItemToObject(json_pack, "version", cursor);

    // data
    json_data = cJSON_CreateObject();
    cJSON_AddItemToObject(json_pack, "data", json_data);
    cursor = cJSON_CreateNumber(executor->status);
    cJSON_AddItemToObject(json_data, "status", cursor);
    cursor = cJSON_CreateNumber(executor->progress);
    cJSON_AddItemToObject(json_data, "progress", cursor);
    cursor = cJSON_CreateNumber(executor->errno);
    cJSON_AddItemToObject(json_data, "errno", cursor);
    cursor = cJSON_CreateNumber(executor->attach);
    cJSON_AddItemToObject(json_data, "attach", cursor);

    /* 
     * If the status is marked to execution error, We try to 
     * take the error buffer as the res_data or take the data buffer as the data
     *
     * @Note since 2019/04/04 we change to post the res_data and the res_error
     */
    cursor = cJSON_CreateStringReference(executor->error_buffer.buffer);
    cJSON_AddItemToObject(json_data, "res_error", cursor);
    cursor = cJSON_CreateStringReference(executor->data_buffer.buffer);
    cJSON_AddItemToObject(json_data, "res_data", cursor);
    json_string = cJSON_PrintUnformatted(json_pack);
    if ( json_string == NULL ) {
        ret_code = 2;
        goto failed;
    }

    /* Update the arguments needed for synchronization */ 
    sprintf(id_buffer, "%lld", executor->task->id);
    memcpy(executor->sync_url + executor->cmid_offset, id_buffer, 19);

    cel_md5_init(&md5_context);
    cel_md5_update(&md5_context, (uchar_t *)executor->node->node_id, strlen(executor->node->node_id));
    cel_md5_update(&md5_context, (uchar_t *)id_buffer, strlen(id_buffer));
    cel_md5_update(&md5_context, (uchar_t *)json_string, strlen(json_string));
    cel_md5_update(&md5_context, 
        (uchar_t *)executor->node->config.app_key, strlen(executor->node->config.app_key));
    cel_md5_final(&md5_context, digest);
    cel_md5_print(digest, md5_str);
    memcpy(executor->sync_url + executor->sign_offset, md5_str, 32);
    // printf("sync_url=%s\n", executor->sync_url);


    curl_easy_setopt(curl, CURLOPT_URL, executor->sync_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);          // Follow http redirect
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);               // max redirect times
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, executor->headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000);          // 10 seconds
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);    // 1 hour cache for DNS
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_string);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _post_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, executor);

    // Perform the http request
    if ( curl_easy_perform(curl) != CURLE_OK ) {
        ret_code = 3;
        goto failed;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if ( http_code != 200 ) {
        ret_code = 4;
        goto failed;
    }

    // Parse the api response and create a task entry
    // printf("res_string: %s\n", executor->res_buffer.buffer);
    json_result = cJSON_Parse(executor->res_buffer.buffer);
    if ( json_result == NULL ) {
        ret_code = 5;
        goto failed;
    }

    cursor = cJSON_GetObjectItem(json_result, "errno");
    if ( ! cJSON_HasObjectItem(json_result, "data") 
        || cursor == NULL || cursor->valueint != 0 ) {
        ret_code = 10 + cursor->valueint;
        goto failed;
    }

failed:
    if ( json_pack != NULL ) {
        cJSON_Delete(json_pack);
    }

    if ( json_string != NULL ) {
        free(json_string);
    }

    if ( json_result != NULL ) {
        cJSON_Delete(json_result);
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return ret_code;
}

/**
 * interface to print the executor
 *
 * @param   executor
*/
SARAH_API void sarah_print_executor(sarah_executor_t *executor)
{
    printf(
        "errno=%d, status=%u, progress=%f, max_datalen=%u\n",
        executor->errno, executor->status, executor->progress, executor->max_datalen
    );

    printf(
        "sync_url=%s, errstr=%s, data=%s\n",
        executor->sync_url, 
        executor->error_buffer.buffer, executor->data_buffer.buffer
    );
}
