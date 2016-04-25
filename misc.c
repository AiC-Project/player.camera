// clang-format off
/* Copyright (C) 2007-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

#include "misc.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define  E(...)    fprintf(stderr, __VA_ARGS__)

extern void
print_tabular( const char** strings, int  count,
               const char*  prefix,  int  width )
{
    int  nrows, ncols, r, c, n, maxw = 0;

    for (n = 0; n < count; n++) {
        int  len = strlen(strings[n]);
        if (len > maxw)
            maxw = len;
    }
    maxw += 2;
    ncols = width/maxw;
    nrows = (count + ncols-1)/ncols;

    for (r = 0; r < nrows; r++) {
        printf( "%s", prefix );
        for (c = 0; c < ncols; c++) {
            int  index = c*nrows + r;
            if (index >= count) {
                break;
            }
            printf( "%-*s", maxw, strings[index] );
        }
        printf( "\n" );
    }
}

extern void
string_translate_char( char*  str, char from, char to )
{
    char*  p = str;
    while (p != NULL && (p = strchr(p, from)) != NULL)
        *p++ = to;
}

extern void
buffer_translate_char( char*        buff,
                       unsigned     buffLen,
                       const char*  src,
                       char         fromChar,
                       char         toChar )
{
    int    len = strlen(src);

    if (len >= buffLen)
        len = buffLen-1;

    memcpy(buff, src, len);
    buff[len] = 0;

    string_translate_char( buff, fromChar, toChar );
}



/** HEXADECIMAL CHARACTER SEQUENCES
 **/

static int
hexdigit( int  c )
{
    unsigned  d;

    d = (unsigned)(c - '0');
    if (d < 10) return d;

    d = (unsigned)(c - 'a');
    if (d < 6) return d+10;

    d = (unsigned)(c - 'A');
    if (d < 6) return d+10;

    return -1;
}

int
hex2int( const uint8_t*  hex, int  len )
{
    int  result = 0;
    while (len > 0) {
        int  c = hexdigit(*hex++);
        if (c < 0)
            return -1;

        result = (result << 4) | c;
        len --;
    }
    return result;
}

void
int2hex( uint8_t*  hex, int  len, int  val )
{
    static const uint8_t  hexchars[16] = "0123456789abcdef";
    while ( --len >= 0 )
        *hex++ = hexchars[(val >> (len*4)) & 15];
}

/** STRING PARAMETER PARSING
 **/

int
strtoi(const char *nptr, char **endptr, int base)
{
    long val;

    errno = 0;
    val = strtol(nptr, endptr, base);
    if (errno) {
        return (val == LONG_MAX) ? INT_MAX : INT_MIN;
    } else {
        if (val == (int)val) {
            return (int)val;
        } else {
            errno = ERANGE;
            return val > 0 ? INT_MAX : INT_MIN;
        }
    }
}

int
get_token_value(const char* params, const char* name, char* value, int val_size)
{
    const char* val_end;
    int len = strlen(name);
    const char* par_end = params + strlen(params);
    const char* par_start = strstr(params, name);

    /* Search for 'name=' */
    while (par_start != NULL) {
        /* Make sure that we're within the parameters buffer. */
        if ((par_end - par_start) < len) {
            par_start = NULL;
            break;
        }
        /* Make sure that par_start starts at the beginning of <name>, and only
         * then check for '=' value separator. */
        if ((par_start == params || (*(par_start - 1) == ' ')) &&
                par_start[len] == '=') {
            break;
        }
        /* False positive. Move on... */
        par_start = strstr(par_start + 1, name);
    }
    if (par_start == NULL) {
        return -1;
    }

    /* Advance past 'name=', and calculate value's string length. */
    par_start += len + 1;
    val_end = strchr(par_start, ' ');
    if (val_end == NULL) {
        val_end = par_start + strlen(par_start);
    }
    len = val_end - par_start;

    /* Check if fits... */
    if ((len + 1) <= val_size) {
        memcpy(value, par_start, len);
        value[len] = '\0';
        return 0;
    } else {
        return len + 1;
    }
}

int
get_token_value_alloc(const char* params, const char* name, char** value)
{
    char tmp;
    int res;

    printf("test %s\n", name);
    /* Calculate size of string buffer required for the value. */
    const int val_size = get_token_value(params, name, &tmp, 0);
    if (val_size < 0) {
        *value = NULL;
        return val_size;
    }

    /* Allocate string buffer, and retrieve the value. */
    *value = (char*)malloc(val_size);
    if (*value == NULL) {
        E("%s: Unable to allocated %d bytes for string buffer.",
          __FUNCTION__, val_size);
        return -2;
    }
    res = get_token_value(params, name, *value, val_size);
    if (res) {
        E("%s: Unable to retrieve value into allocated buffer.", __FUNCTION__);
        free(*value);
        *value = NULL;
    }

    return res;
}

int
get_token_value_int(const char* params, const char* name, int* value)
{
    char val_str[64];   // Should be enough for all numeric values.
    if (!get_token_value(params, name, val_str, sizeof(val_str))) {
        errno = 0;
        *value = strtoi(val_str, (char**)NULL, 10);
        if (errno) {
            E("%s: Value '%s' of the parameter '%s' in '%s' is not a decimal number.",
              __FUNCTION__, val_str, name, params);
            return -2;
        } else {
            return 0;
        }
    } else {
        return -1;
    }
}
// clang-format on
