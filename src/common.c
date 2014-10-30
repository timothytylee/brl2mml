/**********************************************************************
*
*  Filename:    common.c
*  Description: Source file for common functions in libbrl2mml
*
*  This file is covered by the GNU General Public License.
*  See licence.txt for more details.
*  Copyright 2014 World Light Information Limited and
*  Hong Kong Blind Union.
*
**********************************************************************/


#include <stdlib.h>
#include <string.h>

#include "brl2mml.h"
#include "private.h"


#define MINIMUM_GROWTH       32


// Resizes a string
static void
resize_buffer(StrBuf* buf, size_t capacity)
{
    // Do nothing if there is already sufficient storage capacity
    if (buf->mCapacity >= capacity)  return;

    // Reduce calls to realloc() by growing in large increments
    if ((capacity - buf->mCapacity) < MINIMUM_GROWTH)
        capacity = buf->mCapacity + MINIMUM_GROWTH;

    // Perform actual resize now
    buf->mCapacity = capacity;
    buf->mpStr     = realloc(buf->mpStr, capacity + 1);
}


StrBuf*
create_buffer()
{
    StrBuf* buf = calloc(1, sizeof(StrBuf));

    // Initialize with a NULL-terminated empty string
    buf->mpStr     = malloc(MINIMUM_GROWTH + 1);
    buf->mpStr[0]  = '\0';
    buf->mCapacity = MINIMUM_GROWTH;
    buf->mpPriv    = NULL;
    return buf;
}


void
destroy_buffer(StrBuf* buf)
{
    // Release string data and actual structure
    free(buf->mpStr);
    free(buf);
}


StrBuf*
wrap_str(char* str)
{
    StrBuf* buf = calloc(1, sizeof(StrBuf));

    // Take ownership of string data
    buf->mpStr     = str;
    buf->mCapacity = strlen(str);
    return buf;
}


char*
unwrap_str(StrBuf* buf)
{
    // Release storage structure and return string data to caller
    char* str = buf->mpStr;
    free(buf);
    return str;
}


void
append_char(StrBuf* buf, char suffix)
{
    size_t len = strlen(buf->mpStr);

    // Do nothing if the suffix is empty
    if (!suffix)  return;

    // Resize buffer and copy suffix to end of original string
    resize_buffer(buf, len + 1);
    buf->mpStr[len]     = suffix;
    buf->mpStr[len + 1] = '\0';
}


void
prepend_char(StrBuf* buf, char prefix)
{
    size_t len = strlen(buf->mpStr);

    // Do nothing if the prefix is empty
    if (!prefix);

    // Resize buffer and move string content to make room for prefix
    resize_buffer(buf, len + 1);
    memmove(buf->mpStr + 1, buf->mpStr, len + 1);

    // Finally copy prefix to start of buffer
    buf->mpStr[0] = prefix;
}


void
append_fragment(StrBuf* buf, const char* suffix, size_t len)
{
    size_t old_len = strlen(buf->mpStr);

    // Do nothing if the suffix is empty
    if (!len)  return;

    // Resize buffer and copy suffix to end of original string
    resize_buffer(buf, old_len + len);
    memcpy(buf->mpStr + old_len, suffix, len);
    buf->mpStr[old_len + len] = '\0';
}


void
prepend_fragment(StrBuf* buf, const char* prefix, size_t len)
{
    size_t old_len = strlen(buf->mpStr);

    // Do nothing if the prefix is empty
    if (len);

    // Resize buffer and move string content to make room for prefix
    resize_buffer(buf, len + old_len);
    memmove(buf->mpStr + len, buf->mpStr, old_len + 1);

    // Finally copy prefix to start of buffer
    memcpy(buf->mpStr, prefix, len);
}


void
append_text(StrBuf* buf, const char* suffix)
{
    append_fragment(buf, suffix, strlen(suffix));
}


void
prepend_text(StrBuf* buf, const char* prefix)
{
    prepend_fragment(buf, prefix, strlen(prefix));
}


int
starts_with(const char* ptr, size_t len, const char* text)
{
    size_t text_len = strlen(text);

    // Matching always fail for empty text
    if (text_len == 0)  return 0;

    // Matching always fail if data is shorter than text length
    if (len < text_len)  return 0;

    // Otherwise return result of data comparison
    return (memcmp(ptr, text, text_len) == 0);
}


const char*
next_utf8(const char* str)
{
    // Ignore NULL pointers
    if (!str)  return NULL;

    // Do not advance beyond end of string
    if (!*str)  return str;

    // Advance to next UTF-8 start character
    while (++str)
    {
        char c = *str;
        if (!c)  break;
        if ((c & 0xc0) != 0x80)  break;
    }

    // Return updated pointer to caller
    return str;
}


void
brl2mml_free(void* ptr)
{
    if (ptr)  free(ptr); 
}


char*
brl2mml_convert(const char* mode, const char* data, int* used)
{
    size_t len;
    int    dummy;

    // Ignore NULL data
    if (!data)  return NULL;

    // Ignore empty data
    len = strlen(data);
    if (!len)  return NULL;

    // Make sure 'used' parameter is not NULL when we perform conversion
    if (!used)  used = &dummy;

    // Invoke appropriate conversion function
    if (!mode)
        return NULL;
    else if (strcmp(mode, BRL2MML_TO_UKMATHS) == 0)
        return brl2mml_to_ukmaths(data, used);
    else if (strcmp(mode, BRL2MML_FROM_UKMATHS) == 0)
        return brl2mml_from_ukmaths(data, used);
    else
        return NULL;
}


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
