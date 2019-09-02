/**
 * cel bloom filter implemented functions source file
 *     @package    src/cel_bloomfilter.c
 *
 * @author    grandhelmsman <desupport@grandhelmsman.com>
 */
#include "cel_stdio.h"
#include "cel_hash.h"
#include "cel_bloomfilter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int internal_hash_funcs = 12;
static cel_hash_fn_t internal_hash_functions[] = {
    cel_bkdr_hash,
    cel_fnv_hash,
    cel_fnv1a_hash,
    cel_ap_hash,
    cel_djp_hash,
    cel_djp2_hash,
    cel_js_hash,
    cel_sdms_hash,
    cel_rs_hash,
    cel_dek_hash,
    cel_elf_hash,
    cel_bobJenkins32_hash,
    NULL
};

CEL_API int cel_bloomfilter_optimal_bits(int amounts, double false_rate)
{
    double log2;
    if ( false_rate == 0 ) { 
        false_rate = 0.00000000000001;
    }

    log2 = log(2);
    return (-amounts * log(false_rate) / (log2 * log2)); 
}

CEL_API int cel_bloomfilter_optimal_hfuncs(int amounts, int bits)
{
    return (int) round((float) bits / amounts * log(2));
}


/**
 * create a new bloom filter with a specified length
 *
 * @param   length
 * @param   hfuncs
 * @return  cel_bloomfilter_t
 */
CEL_API cel_bloomfilter_t *new_cel_bloomfilter( int length, int hfuncs )
{
    cel_bloomfilter_t *bloom = ( cel_bloomfilter_t * ) \
                  cel_malloc( sizeof( cel_bloomfilter_t ) );
    if ( bloom == NULL ) {
        CEL_ALLOCATE_ERROR("new_cel_bloomfilter", sizeof(cel_bloomfilter_t));
    }

    if ( cel_bloomfilter_init(bloom, length, hfuncs) == 0 ) {
        cel_free(bloom);
        CEL_ALLOCATE_ERROR("cel_bloomfilter_create", length / CHAR_BIT);
    }

    return bloom;
}

//free the specified bloom filter
CEL_API void free_cel_bloomfilter( cel_bloomfilter_t **bloom )
{
    if ( bloom == NULL ) return;
    if ( *bloom != NULL ) {
        cel_bloomfilter_destroy(*bloom);
        cel_free(*bloom);
        *bloom = NULL;
    }
}

/**
 * create a default bloom filter with a specified length
 *     also, this will load the hash functions from cel
 * defined in header file src/cel_hash.h, and 8 hash functions is loaded for default
 * n: Estimated amount
 * m: bit array length
 *
 * math: 
 * k = ln2 * m/n
 * statistics
 * m/n      false positive rate
 * 9        0.0145
 * 10       0.00846
 * 11       0.00509
 * 12       0.00314
 *     .
 *     .
 *     .
 * 30       9.01e-06
 * 31       7.16e-06
 * 32       5.73e-06
 *
 * @param   length
 * @return  cel_bloomfilter_t
 */
CEL_API int cel_bloomfilter_init( cel_bloomfilter_t *bloom, int length, int hfuncs )
{
    int __bytes;

    // make the space for the str
    __bytes = ( length + CHAR_BIT - 1 ) / CHAR_BIT;
    if ( (bloom->str = (char *) cel_calloc(__bytes, sizeof(char))) == NULL ) {
        return 0;
    }

    // initialize
    bloom->length = length;
    bloom->size = 0;
    bloom->hsize = hfuncs > internal_hash_funcs ? internal_hash_funcs : hfuncs;
    bloom->hfuncs = internal_hash_functions;
    memset(bloom->str, 0x00, __bytes);

    return 1;
}

/*
 * destroy the specified bloomfilter
 *
 * @param   cel_bloomfilter_t *
 * @return  int 1 for success and 0 for failed
 */
CEL_API int cel_bloomfilter_destroy( cel_bloomfilter_t *bloom )
{
    if ( bloom != NULL ) {
        cel_free(bloom->str);
        bloom->str = NULL;
    }

    return 1;
}

/**
 * add a new string to the specified bloom filter
 *
 * @param   bloom
 * @param   str
 * @return  int
 */
CEL_API int cel_bloomfilter_add( cel_bloomfilter_t *bloom, char *str )
{
    register uint_t i;

    if ( bloom == NULL || bloom->hsize == 0 ) {
        return 0;
    }

    for ( i = 0; i < bloom->hsize; i++ ) {
        CEL_BIT_OPEN(bloom->str, bloom->hfuncs[i](str) % bloom->length);
    }
    bloom->size++;

    return 1;
}

/**
 * check the specified string is in the specified bloom filter.
 *
 * @param   bloom
 * @param   str
 * @return  int (1 for true and 0 for false)
 */
CEL_API int cel_bloomfilter_exists( cel_bloomfilter_t *bloom, char *str )
{
    register uint_t i;

    if ( bloom == NULL || bloom->hsize == 0 ) {
        return 0;
    }
    
    for ( i = 0; i < bloom->hsize; i++ ) {
        if ( CEL_BIT_FETCH(bloom->str, bloom->hfuncs[i](str) % bloom->length) == 0 ) {
           return 0;
        }
    }

    return 1;
}
