/**********************************************************************
*
*  Filename:    test_util.c
*  Description: Utility functions for test programs
*
*  This file is covered by the GNU General Public License.
*  See licence.txt for more details.
*  Copyright 2014 World Light Information Limited and
*  Hong Kong Blind Union.
*
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_util.h"


#define GROW_SIZE                       4096


char*
read_stdin()
{
    size_t total = 0;
    char*  data = malloc(GROW_SIZE + 1);

    if (!data)  return NULL;

    // Read stdin until there is no more data
    while (data)
    {
        size_t len = fread(data + total, 1, GROW_SIZE, stdin);
        total += len;
        if (len < GROW_SIZE)  break;
        data = realloc(data, total + GROW_SIZE + 1);
    }

    // NULL-terminate the data
    if (data)  data[total] = '\0';
    return data;
}


char*
read_file_data(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    char* data = NULL;
    long  len = 0;

    // Make sure the file could be opened
    if (!file)  return NULL;

    // Obtain file size
    if (0 == fseek(file, 0, SEEK_END))  len = ftell(file);
    if (len > 0)  data = malloc(len + 1);
    if (data)
    {
        // Read the file in one go
        fseek(file, 0, SEEK_SET);
        fread(data, len, 1, file);

        // NULL-terminate the data
        data[len] = '\0';
    }

    // Close the file
    fclose(file);
    return data;
}


void
convert_now(const char* mode, const char* in_str)
{
    for (;;)
    {
        int   used = 0; 
        char* out_str = brl2mml_convert(mode, in_str, &used);

        // Stop when no further conversion is possible
        if (!used)  break;

        // Print conversion result and clean up
        if (out_str)
        {
            puts(out_str);
            brl2mml_free(out_str);
        }

        // Advance to next portion
        in_str += used;
    }
}


/* vim: set cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
