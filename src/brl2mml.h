/**********************************************************************
*
*  Filename:    brl2mml.h
*  Description: Header file for libbrl2mml
*
*  This file is covered by the GNU General Public License.
*  See licence.txt for more details.
*
*  Copyright 2014-2015 World Light Information Limited and
*  Hong Kong Blind Union.
*
**********************************************************************/


/** @file brl2mml.h
    @brief Header file for libbrl2mml
 */


#ifndef BRL2MML_H
#define BRL2MML_H


/// @name Conversion modes @{
#define BRL2MML_TO_UKMATHS              "to_ukmaths"
#define BRL2MML_FROM_UKMATHS            "from_ukmaths"
/// @}


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/** @brief Releases memory allocated by the library.
    @param ptr  Pointer to memory allocated by the library.
  */
void brl2mml_free(void* ptr);

/** @brief Converts between braille and MathML.
    @return     NULL-terminated output.  Release with @b brl2mml_free() after
                use.
    @param mode     Conversion mode specifier.
    @param data     NULL-terminated data that requires conversion.
    @param used     Variable to receive number of bytes used in conversion.
                    Can be @p NULL.
  */
char* brl2mml_convert(const char* mode, const char* data, int* used);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#endif /* BRL2MML_H */


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
