/**********************************************************************
*
*  Filename:    test_util.h
*  Description: Private header file for test programs
*  Version:     $Revision: 1.3 $
*  Date:        $Date: 2014/09/27 12:43:11 $
*
*  This file is covered by the GNU General Public License.
*  See licence.txt for more details.
*  Copyright 2014 World Light Information Limited and
*  Hong Kong Blind Union.
*
**********************************************************************/


/** @file test_util.h
    @brief Private header file for test programs
 */


#ifndef TEST_UTIL_H
#define TEST_UTIL_H


#include <brl2mml.h>


enum
{
    TARGET_UNKNOWN = 0,
    TARGET_UKMATHS = 1,
};


#define match_option(s,b,l) \
    (!strncmp((s), (b), 2) || !strncmp((s), (l), 3))


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @brief Reads data from stdin until end-of-file.
    @return     NULL-terminated string.  Release with @b free() after use.
  */
char* read_stdin();

/** @brief Reads data from file.
    @return     NULL-terminated string.  Release with @b free() after use.
    @param filename     Name of filename to read data file.
  */
char* read_file_data(const char* filename);

/** @brief Converts input string and print result to stdout.
    @param in_str   NULL-terminated input string.
    @param mode     Conversion mode.
  */
void convert_now(const char* mode, const char* in_str);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#endif /* TEST_UTIL_H */


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
