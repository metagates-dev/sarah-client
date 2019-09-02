/**
 * sarah util interface define
 *
 * @author  grandhelmsman<desupport@grandhelmsman.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include "sarah.h"
#include "cel_api.h"
#include "cel_array.h"
#include "cel_hash.h"
#include "cel_hashmap.h"
#include "cel_link.h"
#include "cel_string.h"
#include <curl/curl.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/base64.h>

#ifndef _sarah_util_h
#define _sarah_util_h

typedef void (* sarah_entry_release_fn_t) (void **);
SARAH_API void sarah_clear_link_entries(cel_link_t *, sarah_entry_release_fn_t);

SARAH_API int sarah_mkdir(const char *, mode_t);
SARAH_API char *sarah_get_line(char *, FILE *);


SARAH_API int sarah_file_get_buffer(const char *, char *, size_t);
SARAH_API int sarah_file_put_buffer(const char *, char *, size_t, int);
SARAH_API int sarah_file_contains(const char *, char *);

/**
 * download the specified resource to the specified local file.
*/
SARAH_API int sarah_http_download(const char *, const char *, int);


#endif  /* end ifndef */
