/**********************************************************************
*
*  Filename:    private.h
*  Description: Private header file for libbrl2mml
*
*  This file is covered by the GNU General Public License.
*  See licence.txt for more details.
*
*  Copyright 2014-2015 World Light Information Limited and
*  Hong Kong Blind Union.
*
**********************************************************************/


/** @file private.h
    @brief Private header file for libbrl2mml
 */


#ifndef PRIVATE_H
#define PRIVATE_H


#include "mxml.h"


#define UTF8_COMBINING_OVERLINE         "\xcc\x85"      // U+0305
#define UTF8_COMBINING_OVERDOT          "\xcc\x87"      // U+0307
#define UTF8_APPLY_FUNCTION             "\xe2\x81\xa1"  // U+2061


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/// @name Conversion functions @{
char* brl2mml_to_ukmaths(const char*, int*);
char* brl2mml_from_ukmaths(const char*, int*);
/// @}


/// @brief @b StrBuf private data destructor
typedef void(private_data_destructor_t)(void* priv);


/// @brief Resizable string buffer
typedef struct
{
    char*  mpStr;       ///< NULL-terminated string data
    size_t mCapacity;   ///< Longest string that can be stored
    void*  mpPriv;      ///< Private data
    private_data_destructor_t* mpPrivDtor;
                        ///< Private data destructor
} StrBuf;


/// @name Utility functions for resizable string buffer @{

/** @brief Creates a string buffer.
    @return     Pointer to a new string buffer.
  */
StrBuf* create_buffer();

/** @brief Destroys a string buffer.
    @param buf  Pointer to a string buffer.
  */
void destroy_buffer(StrBuf* buf);

/** @brief Wraps a C string inside a string buffer.
    @return     Pointer to a new string buffer.
    @param str  Pointer to a NULL-terminated C string.  This will be owned by
                the string buffer after the call.
  */
StrBuf* wrap_str(char* str);

/** @brief Unwraps the C string from a string buffer.
    @return     Pointer to a NULL-terminated C string.  Use free() to release
                it after use.
    @param str  Pointer to a string buffer.  This string buffer will no
                longer be valid after the call.
  */
char* unwrap_str(StrBuf* buf);

/** @brief Appends a character to a string buffer.
    @param buf      Pointer to a string buffer.
    @param suffix   Character to append to the string.
  */
void append_char(StrBuf* buf, char suffix);

/** @brief Prepends a character to a string buffer.
    @param buf      Pointer to a string buffer.
    @param prefix   Character to prepend to the string.
  */
void prepend_char(StrBuf* buf, char prefix);

/** @brief Appends text fragment to a string buffer.
    @param buf      Pointer to a string buffer.
    @param suffix   Text fragment to append to the string.
    @param len      Number of bytes in text fragment.
  */
void append_fragment(StrBuf* buf, const char* suffix, size_t len);

/** @brief Prepends text fragment to a string buffer.
    @param buf      Pointer to a string buffer.
    @param prefix   Text fragment to prepend to the string.
    @param len      Number of bytes in text fragment.
  */
void prepend_fragment(StrBuf* buf, const char* prefix, size_t len);

/** @brief Appends NULL-terminated text to a string buffer.
    @param buf      Pointer to a string buffer.
    @param suffix   NULL-terminated text to append to the string.
  */
void append_text(StrBuf* buf, const char* suffix);

/** @brief Prepends NULL-terminated text to a string buffer.
    @param buf      Pointer to a string buffer.
    @param prefix   NULL-terminated text to prepend to the string.
  */
void prepend_text(StrBuf* buf, const char* prefix);

/** @brief Obtains pointer to tail of string buffer.
    @return     Tail of string buffer containing at most @p count characters.
    @param buf      Pointer to a string buffer.
    @param count    Number of characters.
  */
const char* tail_of_buffer(StrBuf* buf, int count);

/// @}


/// @name Utility functions for C string @{

/** @brief Checks whether a string starts with a given text.
    @return     Non-zero if a match is found.
    @return     Zero if there is no match.
    @param str      Pointer to a string that requires checking.
    @param len      Number of bytes in @p str that can be checked.
    @param text     NULL-terminated text to match against. 
  */
int starts_with(const char* str, size_t len, const char* text);

/** @brief Advances to start of next UTF-8 character sequence.
    @return     NULL if @p str is NULL.
    @return     Pointer to next UTF-8 character sequence in @p str.
    @param str  NULL-terminated string.
  */
const char* next_utf8(const char* str);

/** @brief Checks if a string is a trigonometric operator.
    @return     Non-zero if the string is a trigonometric operator.
    @return     Zero if the string is not a trigonometric operator.
    @param str  NULL-terminated string to check.
  */
int is_trigonometric_operator(const char* str);
/// @}


/// @name Utility functions for Mini-XML @{

/** @brief Parses MathML into DOM tree.
    @return     Pointer to new DOM tree.
  */
mxml_node_t* parse_mathml(const char* mml);

/** @brief Gets the first child element.
    @return     Pointer to first child element.  @p NULL if there is none.
    @param x    Pointer to a DOM element.
  */
mxml_node_t* first_child_elem(mxml_node_t* x);

/** @brief Gets the last child element.
    @return     Pointer to last child element.  @p NULL if there is none.
    @param x    Pointer to a DOM element.
  */
mxml_node_t* last_child_elem(mxml_node_t* x);

/** @brief Gets the prev sibling element.
    @return     Pointer to previous sibling element.  @p NULL if there is none.
    @param x    Pointer to a DOM element.
  */
mxml_node_t* get_prev_element(mxml_node_t* x);

/** @brief Gets the next sibling element.
    @return     Pointer to next sibling element.  @p NULL if there is none.
    @param x    Pointer to a DOM element.
  */
mxml_node_t* get_next_element(mxml_node_t* x);

/** @brief Gets the text value of the first child.
    @return     NULL-terminated text value.  Blank string if there is none.
    @param x    Pointer to a DOM element.
  */
const char* get_element_text(mxml_node_t* x);

/** @brief Searches for first non-style and non-index node on DOM tree.
    @return     Pointer to first non-style and non-index node.
    @param x    Pointer to a DOM element.
  */
mxml_node_t* bypass_style_and_indices(mxml_node_t* x);

/** @brief Groups sibling elements under a new element at the same level.
    @return     Pointer to new element.
    @param name     Name of new element.
    @param first    First element to group under new element.
    @param last     Last element to group under new element.
  */
mxml_node_t* group_siblings(const char* name,
        mxml_node_t* first, mxml_node_t* last);

/** @brief Removes outer <mrow> if there is only a single child element
    @return     Pointer to simplified outer element.
    @param mrow     Pointer to <mrow> that needs simplification.
  */
mxml_node_t* simplify_single_child_mrow(mxml_node_t* mrow);

/** @brief Converts <x> to <outer> <x> <mo> <outer>
    @return     Pointer to the new outer element.
    @param x        The element to apply the operator to.
    @param name     Name of the new outer element.
    @param op       The operator to apply.
  */
mxml_node_t* apply_operator(mxml_node_t* x, const char* name, const char* op);

/** @brief Checks if a node is a specific XML element.
    @return     Non-zero if the node matches.
    @param x        Pointer to DOM element.
    @param name     The required name.  @p NULL matches any element.
  */
int is_xml_element(mxml_node_t* x, const char* name);

/** @brief Checks if a node is a specific <mo> operator.
    @return     Non-zero if the node matches.
    @param x    Pointer to DOM element.
    @param op   The required operator.  @p NULL matches any operator.
  */
int is_operator(mxml_node_t* x, const char* op);

/** @brief Checks if a node is a specific <mi> identifier.
    @return     Non-zero if the node matches.
    @param x    Pointer to DOM element.
    @param id   The required identifier.  @p NULL matches any identifier.
  */
int is_identifier(mxml_node_t* x, const char* id);

/** @brief Checks if a node is a specific <mn> number.
    @return     Non-zero if the node matches.
    @param x    Pointer to DOM element.
    @param op   The required number.  @p NULL matches any number.
  */
int is_number(mxml_node_t* x, const char* num);

/** @brief Converts <mfenced open="(" close=")"> into <mrow>
    @param x    Pointer to a DOM node that requires removal of round bracket.
  */
void remove_round_bracket(mxml_node_t* x);

/** @brief Creates a <mi> element representing a unit.
    @return     Pointer to newly created element.
    @param unit     Name of the unit.
  */
mxml_node_t* new_unit_element(mxml_node_t* x, const char* unit);

/// @}


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#endif /* PRIVATE_H */


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
