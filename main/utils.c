/**
 * sarah utils interface implementation
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>

#include "sarah.h"
#include "sarah_utils.h"

// clear and free all the entires in the specified link list
SARAH_API void sarah_clear_link_entries(
    cel_link_t *list, sarah_entry_release_fn_t _free)
{
    void *value = NULL;
    while ( (value = cel_link_remove_first(list)) != NULL ) {
        _free(&value);
    }
}

// create the specified directory
SARAH_API int sarah_mkdir(const char *path, mode_t mode_mask)
{
    int i, len;
    char dirname[256] = {'\0'};
    snprintf(dirname, sizeof(dirname), "%s", path);
    len = strlen(dirname);

    // directly start from 1 with the first / ignored
    for ( i = 1; i < len; i++ ) {
        if ( dirname[i] != '/' ) {
            continue;
        }

        dirname[i] = 0;
        // printf("dir=%s\n", dirname);
        if ( access(dirname, F_OK) != 0 
            && mkdir(dirname, mode_mask) == -1 ) {
            dirname[i] = '/';
            return 1;
        }

        dirname[i] = '/';
    }

    // process the path that did not start with /
    if ( dirname[len-1] != '/' ) {
        if ( access(dirname, F_OK) != 0 
            && mkdir(dirname, mode_mask) == -1 ) {
            return 1;
        }
    }

    return 0;
}

SARAH_API char *sarah_get_line(char *__dst, FILE *_stream)
{
    register int c;
    char *cs = __dst;
    while ( (c = fgetc(_stream)) != EOF ) {
        if ( c == '\n' ) break;
        *cs++ = c; 
    }
    *cs = '\0';

    return (c == EOF && cs == __dst) ? NULL : __dst;
}


SARAH_API int sarah_file_get_buffer(const char *file, char *buffer, size_t size)
{
    int fno, ret_code = -1;
    if ( (fno = open(file, O_RDONLY)) > -1 ) {
        if ( flock(fno, LOCK_SH) == 0 ) {
            ret_code = (int) read(fno, buffer, size);
            flock(fno, LOCK_UN);
        }
        close(fno);
    }

    return ret_code;
}

SARAH_API int sarah_file_put_buffer(
    const char *file, char *buffer, size_t size, int append)
{
    int fno, ret_code = -1;
    if ( (fno = open(file, 
        O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) > -1 ) {
        if ( flock(fno, LOCK_EX) == 0 ) {
            if ( append == 1 ) {
                ret_code = (int) write(fno, buffer, size);
            } else {
                lseek(fno, 0, SEEK_SET);
                /* Clear the content of the file */
                if ( ftruncate(fno, 0) == 0 ) {
                    ret_code = (int) write(fno, buffer, size);
                }
            }
            flock(fno, LOCK_UN);
        }
        close(fno);
    }

    return ret_code;
}


/*
 * search a specified string(not include \n char) in a file
 * if there is a line that has more than 2048 chars this may failed.
 *
 * @param   file
 * @param   str
 * @return  integer
*/
SARAH_API int sarah_file_contains(const char *file, char *str)
{
    FILE *fp = NULL;
    int _lock = -1, matched = 0;
    char buffer[2049] = {'\0'};

    if ( (fp = fopen(file, "r")) == NULL ) {
        return -1;
    }

    if ( (_lock = flock(fp->_fileno, LOCK_SH)) == -1 ) {
        goto failed;
    }

    while ( 1 ) {
        if ( fgets(buffer, sizeof(buffer), fp) == NULL ) {
            break;
        }

        if ( strstr(buffer, str) != NULL ) {
            matched = 1;
            break;
        }
    }

    flock(fp->_fileno, LOCK_UN);

failed:
    if ( fp != NULL ) {
        if ( _lock > -1 ) {
            flock(fp->_fileno, LOCK_UN);
        }

        fclose(fp);
    }

    return matched == 0 ? -1 : 1;
}



static size_t _download_recv_callback(
    void *buffer, size_t size, size_t nmemb, void *ptr)
{
    FILE *fd = (FILE *) ptr;
    fwrite(buffer, size, nmemb, fd);
    return size * nmemb;
}

/**
 * download the specified resource to the specified local file.
 *
 * @param   url
 * @param   file
 * @param   override
 * @return  int 0 for succeed or failed
*/
SARAH_API int sarah_http_download(
    const char *url, const char *file, int override)
{
    FILE *fd = NULL;
    int ret_code = 0, _fileno, _lock = -1;
    long http_code;
    char buffer[256] = {'\0'};

    CURL *curl = NULL;
    struct curl_slist *_headers = NULL;

    if ( override == 0 
        && access(file, F_OK) == 0 ) {
        ret_code = 1;
        goto failed;
    }

    if ( (fd = fopen(file, "wb")) == NULL ) {
        ret_code = 2;
        goto failed;
    }

    /* Lock the target local file */
    _fileno = fileno(fd);
    if ( (_lock = flock(_fileno, LOCK_EX)) == -1 ) {
        ret_code = 3;
        goto failed;
    }


    // Initialize the curl and set the options
    curl = curl_easy_init();
    if ( curl == NULL ) {
        curl_global_cleanup();
        ret_code = 4;
        goto failed;
    }

    /* http request header initialization */
    snprintf(buffer, sizeof(buffer), "User-Agent: downloader util/1.0 sarahos/%.02f", sarah_version());
    _headers = curl_slist_append(_headers, buffer);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, _headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 7200000);        // 120 minutes
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300);     // 5 mins cache for DNS
    curl_easy_setopt(curl, CURLOPT_HEADER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _download_recv_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);

    // Perform the http request
    if ( curl_easy_perform(curl) != CURLE_OK ) {
        ret_code = 5;
        goto failed;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if ( http_code != 200 ) {
        ret_code = 6;
        goto failed;
    }


failed:
    if ( fd != NULL ) {
        if ( _lock > -1 ) {
            flock(_fileno, LOCK_UN);
            _lock = -1;
        }

        fclose(fd);
        fd = NULL;
    }

    if ( _headers != NULL ) {
        curl_slist_free_all(_headers);
        _headers = NULL;
    }

    if ( curl != NULL ) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }

    curl_global_cleanup();

    /* Check and remove the file if the http_code is not 200 */
    if ( ret_code == 6 ) {
        if ( unlink(file) == -1 ) {
            // @TODO: do error log here
        }
    }

    return ret_code;
}
