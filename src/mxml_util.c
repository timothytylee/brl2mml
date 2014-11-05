/**********************************************************************
*
*  Filename:    mxml_util.c
*  Description: Source file for Mini-MXL uitlity functions
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


mxml_node_t*
first_child_elem(mxml_node_t* x)
{
    for (x = mxmlGetFirstChild(x);  x;  x = mxmlGetNextSibling(x))
        if (mxmlGetElement(x))  break;
    return x;
}


mxml_node_t*
last_child_elem(mxml_node_t* x)
{
    for (x = mxmlGetLastChild(x);  x;  x = mxmlGetPrevSibling(x))
        if (mxmlGetElement(x))  break;
    return x;
}


mxml_node_t*
get_prev_element(mxml_node_t* x)
{
    for (x = mxmlGetPrevSibling(x);  x;  x = mxmlGetPrevSibling(x))
        if (mxmlGetElement(x))  break;
    return x;
}


mxml_node_t*
get_next_element(mxml_node_t* x)
{
    for (x = mxmlGetNextSibling(x);  x;  x = mxmlGetNextSibling(x))
        if (mxmlGetElement(x))  break;
    return x;
}


const char*
get_element_text(mxml_node_t* x)
{
    int         whitespace;
    const char* text = NULL;
    if (x)  x = mxmlGetFirstChild(x);
    if (x)  text = mxmlGetText(x, &whitespace);
    if (!text)  text = "";
    return text;
}


mxml_node_t*
bypass_style_and_indices(mxml_node_t* x)
{
    const char* name;

    if (!x)  return NULL;
    name = mxmlGetElement(x);

    // Ignore outer nodes with style or index information
    while ((strcmp(name, "mstyle") == 0) ||
            (strcmp(name, "msub") == 0) ||
            (strcmp(name, "msup") == 0) ||
            (strcmp(name, "msubsup") == 0) ||
            (strcmp(name, "munder") == 0) ||
            (strcmp(name, "mover") == 0) ||
            (strcmp(name, "munderover") == 0) ||
            (strcmp(name, "mmultiscripts") == 0))
    {
        x = first_child_elem(x);
        if (!x)  return NULL;
        name = mxmlGetElement(x);
    }
    return x;
}


mxml_node_t*
group_siblings(const char* name, mxml_node_t* first, mxml_node_t* last)
{
    mxml_node_t* outer = mxmlNewElement(MXML_NO_PARENT, name);
    mxmlAdd(mxmlGetParent(first), MXML_ADD_BEFORE, first, outer);
    for (;;)
    {
        mxml_node_t* elem = first;
        first = mxmlGetNextSibling(first);
        mxmlAdd(outer, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, elem);
        if (elem == last)  break;
        if (!first)  break;
    }
    return outer;
}


mxml_node_t*
simplify_single_child_mrow(mxml_node_t* mrow)
{
    if (first_child_elem(mrow) == last_child_elem(mrow))
    {
        mxml_node_t* child = first_child_elem(mrow);
        const char*  child_variant = mxmlElementGetAttr(child, "mathvariant");
        const char*  mrow_variant = mxmlElementGetAttr(mrow, "mathvariant");
        if (child)
        {
            // Inherit style
            if (!child_variant && mrow_variant)
                mxmlElementSetAttr(child, "mathvariant", mrow_variant);

            // <mrow> with single child
            mxmlAdd(mxmlGetParent(mrow), MXML_ADD_BEFORE, mrow, child);
            mxmlDelete(mrow);
            mrow = child;
        }
    }
    return mrow;
}


mxml_node_t*
apply_operator(mxml_node_t* x, const char* name, const char* op)
{
    mxml_node_t* mo = mxmlNewElement(MXML_NO_PARENT, "mo");
    mxml_node_t* outer;
    mxmlNewText(mo, 0, op);
    mxmlAdd(mxmlGetParent(x), MXML_ADD_AFTER, x, mo);
    remove_round_bracket(x);
    return group_siblings(name, x, mo);
}


int
is_xml_element(mxml_node_t* x, const char* name)
{
    const char* tag;
    if (!x)  return 0;
    tag = mxmlGetElement(x);
    if (!tag)  return 0;
    return (strcmp(tag, name) == 0);
}


int
is_operator(mxml_node_t* x, const char* op)
{
    const char* name;

    // Ignore style and indices
    x = bypass_style_and_indices(x);
    if (!x)  return 0;
    name = mxmlGetElement(x);

    // Name of operator is inside <mo>
    if (strcmp(name, "mo") != 0)  return 0;
    else if (!op)  return 1;
    else
    {
        const char* mo = get_element_text(x);
        return (strcmp(mo, op) == 0);
    }
}


int
is_identifier(mxml_node_t* x, const char* id)
{
    const char* name;

    // Ignore style and indices
    x = bypass_style_and_indices(x);
    if (!x)  return 0;
    name = mxmlGetElement(x);

    // Name of identifier is inside <mi>
    if (strcmp(name, "mi") != 0)  return 0;
    else if (!id)  return 1;
    else
    {
        const char* mi = get_element_text(x);
        return (strcmp(mi, id) == 0);
    }
}


int
is_number(mxml_node_t* x, const char* num)
{
    const char* name;

    // Ignore style and indices
    x = bypass_style_and_indices(x);
    if (!x)  return 0;
    name = mxmlGetElement(x);

    // Value of number is inside <mn>
    if (strcmp(name, "mn") != 0)  return 0;
    else if (!num)  return 1;
    else
    {
        const char* mn = get_element_text(x);
        return (strcmp(mn, num) == 0);
    }
}


void
remove_round_bracket(mxml_node_t* x)
{
    // Make sure it is a <mfenced> node
    if (strcmp(mxmlGetElement(x), "mfenced") != 0)  return;

    // Make sure it is enclosed in round brackets
    if (strcmp(mxmlElementGetAttr(x, "open"), "(") != 0)  return;
    if (strcmp(mxmlElementGetAttr(x, "close"), ")") != 0)  return;

    // Convert outer element to <mrow>
    mxmlSetElement(x, "mrow");
    mxmlElementDeleteAttr(x, "open");
    mxmlElementDeleteAttr(x, "close");
    mxmlElementDeleteAttr(x, "separators");
}


mxml_node_t*
new_unit_element(mxml_node_t* x, const char* unit)
{
    mxml_node_t* mspace = mxmlNewElement(x, "mspace");
    mxml_node_t* mi = mxmlNewElement(x, "mi");
    mxmlElementSetAttr(mspace, "width", "0.25em");
    mxmlElementSetAttr(mi, "mathvariant", "normal");
    mxmlNewText(mi, 0, unit);
    return mi;
}



/// @brief Recursively removes <mrow> containing only one child element
static mxml_node_t*
recursively_remove_redundant_mrow(mxml_node_t* x)
{
    mxml_node_t* y;

    // Attempt to simplify <mrow>
    const char*  name = mxmlGetElement(x);
    if (name && (strcmp(name, "mrow") == 0))
        x = simplify_single_child_mrow(x);

    // Remove redundant <mrow> in child nodes
    for (y = first_child_elem(x);  y;  y = get_next_element(y))
        y = recursively_remove_redundant_mrow(y);

    // Node might have been replaced, so return to caller
    return x;
}


/// @brief Recursively replaces <mstyle> elements with <mrow>
static void
recursively_replace_mstyle(mxml_node_t* x)
{
    // Rename <mstyle> to <mrow>
    const char* name = mxmlGetElement(x);
    if (name && (strcmp(name, "mstyle") == 0))  mxmlSetElement(x, "mrow");

    // Replace <mstyle> in child nodes
    for (x = first_child_elem(x);  x;  x = get_next_element(x))
        recursively_replace_mstyle(x);
}


mxml_node_t*
parse_mathml(const char* mml)
{
    // Parse XML into DOM tree
    mxml_node_t* x = mxmlNewElement(MXML_NO_PARENT, "xml");
    mxmlLoadString(x, mml, MXML_NO_CALLBACK);

    // Clean up DOM tree
    recursively_replace_mstyle(x);
    recursively_remove_redundant_mrow(x);

    // Return DOM tree to caller
    return x;
}


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
