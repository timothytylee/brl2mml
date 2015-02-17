/**********************************************************************
*
*  Filename:    to_ukmaths.c
*  Description: Implementation of MathML to UK Maths translation
*
*  This file is covered by the GNU General Public License.
*  See licence.txt for more details.
*
*  Copyright 2014-2015 World Light Information Limited and
*  Hong Kong Blind Union.
*
**********************************************************************/


#include <stdlib.h>
#include <string.h>

#include "brl2mml.h"
#include "private.h"


#ifdef WIN32
#  define strcasecmp _stricmp
#endif


#define UTF8_MIDDLE_DOT                 "·" // U+00b7
#define UTF8_DOT_OPERATOR               "⋅" // U+22c5

#define NOT_IDENTIFIER                  0
#define IS_IDENTIFIER                   1

#define NOT_OVERSCRIPT                  0
#define IS_OVERSCRIPT                   1

#define NOT_UNDERSCRIPT                 0
#define IS_UNDERSCRIPT                  1


/// @brief MathML styles
enum
{
    STYLE_ITALIC = 0,
    STYLE_NORMAL,
    STYLE_BOLD,
    STYLE_FRAKTUR,
};

/// @brief StrBuf content identifiers
enum
{
    END_WITH_OTHER = 0,
    END_WITH_MTEXT,
    END_WITH_WORD_MO,
    END_WITH_NUM_MN,
    END_WITH_UNIT_MI,
    END_WITH_LOWERED_DIGIT,
};

/// @brief Private data for resizable string buffer
typedef struct
{
    int mStyle;             ///< Current MathML style
    int mEndType;           ///< String buffer content identifier
    int mUnspaced;          ///< Non-zero if optional spaces should be removed
} MathData;


/// Import is_extendable_ukmaths_node() from "from_ukmaths.c"
int is_extendable_ukmaths_node(mxml_node_t* x);

/// Forward declaration for recursive invocation
static int translate_math_node(StrBuf* buf, mxml_node_t* x);
static void translate_terms_in_set(StrBuf* buf, mxml_node_t* x);


/// @brief Private data destructor
static void
destroy_private_data(void* v)
{
    MathData* priv = (MathData*)v;

    // Clean up
    free(priv);
}


/// @brief Obtains private data from a string buffer
static MathData*
get_private_data(StrBuf* buf)
{
    // Ignore invalid buffer
    if (!buf)  return NULL;

    // Create private data if necessary
    if (!buf->mpPriv)
    {
        buf->mpPriv = (MathData*)calloc(1, sizeof(MathData));
        if (buf->mpPriv)
        {
            // Setup private data destructor
            buf->mpPrivDtor = destroy_private_data;
        }
    }
    return (MathData*)buf->mpPriv;
}


/// @brief Gets MathML style from string buffer
static int
get_math_style(StrBuf* buf)
{
    MathData* priv = get_private_data(buf);
    if (priv)  return priv->mStyle;
    else       return STYLE_NORMAL;
}


/// @brief Sets MathML style in string buffer
static void
set_math_style(StrBuf* buf, int style)
{
    MathData* priv = get_private_data(buf);
    if (priv)  priv->mStyle = style;
}


/// @brief Gets end type from string buffer
static int
get_end_type(StrBuf* buf)
{
    MathData* priv = get_private_data(buf);
    if (priv)  return priv->mEndType;
    else       return END_WITH_OTHER;
}


/// @brief Sets end type in string buffer
static void
set_end_type(StrBuf* buf, int endType)
{
    MathData* priv = get_private_data(buf);
    if (priv)  priv->mEndType = endType;
}


/// @brief Gets unspaced flag from string buffer
static int
is_unspaced(StrBuf* buf)
{
    MathData* priv = get_private_data(buf);
    if (priv)  return priv->mUnspaced;
    else       return 0;
}


/// @brief Sets unspaced flag in string buffer
static void
set_unspaced(StrBuf* buf, int unspaced)
{
    MathData* priv = get_private_data(buf);
    if (priv)  priv->mUnspaced = unspaced;
}


/// @brief Creates temporary buffer that retains private data
static StrBuf*
create_temporary_buffer(StrBuf* buf)
{
    StrBuf* tmp_buf = create_buffer();
    *(get_private_data(tmp_buf)) = *(get_private_data(buf));
    return tmp_buf;
}


/// @brief Checks whether a string buffer has a trailing space
static int
has_trailing_space(StrBuf* buf)
{
    const char* tail = tail_of_buffer(buf, 1);

    // Consider empty string to have trailing space
    if (!*tail)  return 1;

    // Otherwise check for trailing space
    return (strchr(" <", *tail) != NULL);
}


/// @brief Checks whether there is an item separator child node
static int
contains_item_separator(mxml_node_t* x)
{
    for (x = first_child_elem(x);  x;  x = next_elem(x))
        if (is_item_separator(x))  return 1;
    return 0;
}


/// @brief Encloses standalone <mi> in <mrow> to ensure correct translation
static mxml_node_t*
group_standalone_mi(mxml_node_t* x)
{
    if (is_xml_element(x, "mi"))  x = group_siblings("mrow", x, x);
    return x;
}


/// @brief Appends buffer with optional Latin fount in between if necessary
static void
append_buffer_with_fount(StrBuf* buf, StrBuf* suffix)
{
    char c = suffix->mpStr[0];
    if ((c >= 'a') && (c <= 'z'))
    {
        // New text starts with Latin character
        size_t len = strlen(buf->mpStr);

        // If buffer ends in alpha character, add fount to avoid confusion
        if (len > 0)
        {
            char c = buf->mpStr[len - 1];
            if ((get_end_type(buf) != END_WITH_OTHER) ||
                    (c == '_') || ((c >= 'a') && (c <= 'z')))
                append_char(buf, ';');
        }
    }

    // Append suffix now
    append_text(buf, suffix->mpStr);
    set_end_type(buf, get_end_type(suffix));
}


/// @brief Determines math style of MathML element given its parent's style
static int
find_math_style(int parentStyle, mxml_node_t* x)
{
    const char* variant = mxmlElementGetAttr(x, "mathvariant");
    if (variant)
    {
        if (strcmp(variant, "normal"      ) == 0)  return STYLE_NORMAL;
        if (strcmp(variant, "bold"        ) == 0)  return STYLE_BOLD;
        if (strcmp(variant, "italic"      ) == 0)  return STYLE_ITALIC;
        if (strcmp(variant, "bold-italic" ) == 0)  return STYLE_ITALIC;
        if (strcmp(variant, "fraktur"     ) == 0)  return STYLE_FRAKTUR;
        if (strcmp(variant, "bold-fraktur") == 0)  return STYLE_FRAKTUR;
    }
    return parentStyle;
}


/// @brief Updates math style in buffer and returns old style
static int
update_math_style(StrBuf* buf, mxml_node_t* x)
{
    int old_style = get_math_style(buf);
    set_math_style(buf, find_math_style(old_style, x));
    return old_style;
}


/// @brief Checks whether a string contains only digits
static int
is_numeric_string(const char* str)
{
    // Check every character in the string
    while (*str)
    {
        size_t len = strlen(str);
        if (strchr("0123456789", *str))
        {
            ++str;
            continue;
        }
        else if (starts_with(str, len, UTF8_DOT_OPERATOR) ||
                starts_with(str, len, UTF8_MIDDLE_DOT))
        {
            str = next_utf8(str);
            continue;
        }
        return 0;
    }

    // All characters are digits
    return 1;
}


/// @brief Checks whether an element is a <mn> node containing only digits
static int
is_numeric_mn(mxml_node_t* x)
{
    // Check for <mn> element
    if (!is_xml_element(x, "mn"))  return 0;

    // Check the value of <mn>
    return is_numeric_string(get_element_text(x));
}


/// @brief Checks for a numeric index that can be encoded with lowered digits
static int
is_numeric_index(mxml_node_t* x)
{
    const char* value;
    size_t      len;

    // A numeric <mn> can be encoded with lowered digits
    if (is_numeric_mn(x))  return 1;

    // Otherwise check for <mrow> element
    if (is_xml_element(x, "mrow"))
    {
        mxml_node_t* mo = first_child_elem(x);
        mxml_node_t* mn = mo ? next_elem(mo) : NULL;
        mxml_node_t* last = last_child_elem(x);
        if ((mn == last) &&
                (is_operator(mo, "−") || is_operator(mo, "-")) &&
                is_numeric_mn(mn))
            return 1;
    }

    // Report failure
    return 0;
}


static const char* overheads[] =
{
    "¯",   ":",
    "ˆ",   "@:",
    "‸",   "@:",
    "ˇ",   "@>",
    "~",   "^:",
    "∼",   "^:",
    "···", "-'",
    "··",  "-",
    "·",   "'",
    "∗",   "@5",
    "∗∗",  "@55",
    "∗∗∗", "@555",

    // List terminator
    NULL
};

/// @brief Checks whether an element is a simple overhead <mo> node
static int
is_overhead_mo(mxml_node_t* x)
{
    const char*  text;
    const char** oh;

    // Check for <mo> element
    if (!is_xml_element(x, "mo"))  return 0;

    // Check all known overhead symbols 
    text = get_element_text(x);
    for (oh = overheads;  *oh;  oh += 2)
        if (strcmp(text, oh[0]) == 0)  return 1;
    return 0;
}


/// @brief Checks whether a node contains a simple maths term
static int
is_simple_term(mxml_node_t* x)
{
    while (x)
    {
        // <mfenced> are simple
        if (is_xml_element(x, "mfenced"))  return 1;

        // <msub> or <msup> with simple base and alphanumeric index are simple
        if (is_xml_element(x, "msub") || is_xml_element(x, "msup"))
        {
            x = first_child_elem(x);
            if (is_simple_term(x))
            {
                x = next_elem(x);
                if (is_identifier(x, NULL))  return 1;
                if (is_numeric_index(x))  return 1;
                if (is_operator(x, "+"))  return 1;
                if (is_operator(x, "-"))  return 1;
            }
            return 0;
        }

        // <munder> or <mover> with simple base and alphanumeric index or
        // overhead symbol are simple
        if (is_xml_element(x, "mover") || is_xml_element(x, "munder"))
        {
            x = first_child_elem(x);
            if (is_simple_term(x))
            {
                x = next_elem(x);
                if (is_identifier(x, NULL))  return 1;
                if (is_numeric_index(x))  return 1;
                if (is_overhead_mo(x))  return 1;
            }
            return 0;
        }

        // Check child node
        x = first_child_elem(x);
        if (!x)  break;

        // Not simple if there is a sibling element
        if (next_elem(x))  return 0;
    }

    // All nodes are single element
    return 1;
}


/// @brief Translates digit to un-shifted numerical braille
static char
digit_to_normal_brl(char c)
{
    if (c == '0')  return 'j';
    else           return (c - '1' + 'a');
}


/// @brief Translates bracket to braille
static const char*
bracket_to_brl(const char* bracket)
{
    static const char* brackets[] =
    {
        "(",  "<",
        ")",  ">",
        "[",  "(",
        "]",  ")",
        "{",  "[",
        "}",  "o",
        "〈", ".(",
        "〉", ".)",
        "|",  "_",
        "‖",  "__",

        // List terminator
        NULL
    };
    const char** b;

    // Ignore NULL pointers
    if (!bracket)  return NULL;

    // Check all known overhead brackets 
    for (b = brackets;  *b;  b += 2)
        if (strcmp(bracket, b[0]) == 0)  return b[1];
    return NULL;
}


/// @brief Translates string as normal braille digits
static void
translate_normal_digits(StrBuf* buf, const char* value)
{
    int is_recurring = 0;
    while (*value)
    {
        char c = *value;
        if (c == '.')
        {
            // Dot 2 is decimal point
            append_text(buf, "1");
            ++value;
        }
        else if (c == ',')
        {
            // Dot 3 is comma
            append_text(buf, "\'");
            ++value;
        }
        else if (strchr("0123456789", c))
        {
            int used = 1;
            if (strncmp(value + 1, UTF8_COMBINING_OVERDOT,
                        strlen(UTF8_COMBINING_OVERDOT)) == 0)
            {
                // Dot 5 starts recurring sequence
                if (!is_recurring)
                {
                    is_recurring = 1;
                    append_char(buf, '"');
                }
                used += strlen(UTF8_COMBINING_OVERDOT);
            }
            else if (strncmp(value + 1, UTF8_COMBINING_OVERLINE,
                        strlen(UTF8_COMBINING_OVERLINE)) == 0)
            {
                // Dot 45 starts negative number with bar above
                append_char(buf, '^');
                used += strlen(UTF8_COMBINING_OVERLINE);
            }
            append_char(buf, digit_to_normal_brl(c));
            value += used;
        }
        else
        {
            // Skip unknown characters
            ++value;
        }
    }
}


/// @brief Translates string as lowered braille digits
static void
translate_lowered_digits(StrBuf* buf, const char* value)
{
    while (*value)
    {
        size_t len = strlen(value);
        if (strchr("0123456789", *value))
        {
            append_char(buf, *value);
            ++value;
        }
        else if (starts_with(value, len, UTF8_DOT_OPERATOR) ||
                starts_with(value, len, UTF8_MIDDLE_DOT))
        {
            // Dot 46-3 is the dot used in statistics
            append_text(buf, ".'");
            value = next_utf8(value);
        }
        else  ++value;
    }
}


/// @brief Translates an element with lowered braille digits
static void
translate_numeric_index(StrBuf* buf, mxml_node_t* x)
{
    if (is_xml_element(x, "mn"))
    {
        translate_lowered_digits(buf, get_element_text(x));
    }
    else if (is_xml_element(x, "mrow"))
    {
        mxml_node_t* mo = first_child_elem(x);
        mxml_node_t* mn = mo ? next_elem(mo) : NULL;
        if (mo && mn)
        {
            append_text(buf, ";-");
            translate_lowered_digits(buf, get_element_text(mn));
        }
    }
}


/// @brief Strips trailing space from a string buffer
static void
strip_trailing_space(StrBuf* buf)
{
    size_t len = strlen(buf->mpStr);
    if ((len > 0) && (buf->mpStr[len - 1] == ' '))
        buf->mpStr[len - 1] = '\0';
}


/// @brief Strips trailing index terminators from a string buffer
static void
strip_trailing_index_terminators(StrBuf* buf)
{
    int   len = strlen(buf->mpStr);
    char* ptr = buf->mpStr + len - 1;
    while (len)
    {
        if (*ptr != ']')  break;

        // Replace dot 12456 index terminator from end of string
        *ptr = '\0';
        --ptr;
        --len;
    }
}


/// @brief Translates child nodes
static int
translate_children(StrBuf* buf, mxml_node_t* x)
{
    StrBuf* tmp_buf;
    int     end_with = END_WITH_OTHER;

    // Remove <mspace>, <mphantom> and "apply function" <mo> nodes
    mxml_node_t* el = first_child_elem(x);
    while (el)
    {
        mxml_node_t* next = next_elem(el);
        if (is_xml_element(el, "mspace") ||
                is_xml_element(el, "mphantom") ||
                is_operator(el, UTF8_APPLY_FUNCTION))
            mxmlDelete(el);
        el = next;
    }

    // Translate each of the child node
    tmp_buf = create_temporary_buffer(buf);
    for (x = first_child_elem(x);  x;  x = next_elem(x))
        end_with = translate_math_node(tmp_buf, x);

    // Append translated content to buffer
    strip_trailing_space(tmp_buf);
    append_buffer_with_fount(buf, tmp_buf);
    destroy_buffer(tmp_buf);
    return end_with;
}


/// @brief Translates a mathematical expression and add brackets if necessary
static int
translate_expression(StrBuf* buf, mxml_node_t* x)
{
    StrBuf* tmp_buf = create_temporary_buffer(buf);
    int     simple = is_simple_term(x);
    int     end_with;

    // Protect standalone <mi>
    x = group_standalone_mi(x);

    // Translate node into temporary buffer
    if (is_unspaced(buf) && contains_item_separator(x))
    {
        translate_terms_in_set(tmp_buf, x);
        simple   = 0;
        end_with = END_WITH_OTHER;
    }
    else
        end_with = translate_math_node(tmp_buf, x);
    strip_trailing_space(tmp_buf);

    if (simple || (is_unspaced(buf) && !strchr(tmp_buf->mpStr, ' ')))
    {
        // Prevent arrow from being mis-interpreted as numeric index
        if (tmp_buf->mpStr[0] == '3')  prepend_char(tmp_buf, '@');
    }
    else
    {
        // Surround complex expression with bracket
        prepend_char(tmp_buf, '<');
        append_char(tmp_buf, '>');
        end_with = END_WITH_OTHER;
    }

    // Append translated content to buffer
    append_buffer_with_fount(buf, tmp_buf);
    destroy_buffer(tmp_buf);
    return end_with;
}


/// @brief Translates node as a base
static int
translate_base(StrBuf* buf, mxml_node_t* x)
{
    // Retain spaces in base expression
    set_unspaced(buf, 0);
    return translate_expression(buf, x);
}


/// @brief Translates node as an overhead symbol
static int
translate_overhead_symbol(StrBuf* buf, mxml_node_t* x)
{
    const char*  text;
    const char** oh;

    // Ignore NULL nodes
    if (!is_xml_element(x, "mo"))  return 0;
    text = get_element_text(x);

    // Check all known overhead symbols 
    for (oh = overheads;  *oh;  oh += 2)
    {
        if (strcmp(text, oh[0]) != 0)  continue;
        append_text(buf, oh[1]);
        return 1;
    }

    // Nothing matched
    return 0;
}


/// @brief Translates node as a subscript
static int
translate_subscript(StrBuf* buf, mxml_node_t* x, int isUnder)
{
    // Minimize spaces in subscript
    set_unspaced(buf, 1);

    // Numeric indices are encoded as lowered braille digits
    if (!isUnder && is_numeric_index(x))
    {
        // Use dot 16 to isolate from other lowered digits
        if (get_end_type(buf) == END_WITH_LOWERED_DIGIT)
            append_char(buf, '*');

        // Translate now
        translate_numeric_index(buf, x);
        return END_WITH_LOWERED_DIGIT;
    }

    // Dot 4 starts underscript
    if (isUnder)  append_char(buf, '@');

    // Dot 16 and 12456 encloses subscript
    append_char(buf, '*');
    if (!translate_overhead_symbol(buf, x))
        translate_expression(buf, x);
    append_char(buf, ']');
    return END_WITH_OTHER;
}


/// @brief Translates node as a superscript
static int
translate_superscript(StrBuf* buf, mxml_node_t* x, int isOver)
{
    // Minimize spaces in superscript
    set_unspaced(buf, 1);

    // Overhead symbols are appended directly
    if (translate_overhead_symbol(buf, x))  return END_WITH_OTHER;

    // Dot 4 starts overscript
    if (isOver)  append_char(buf, '@');

    // Dot 346 starts superscript
    append_char(buf, '+');

    // Numeric indices are encoded as lowered braille digits
    if (is_numeric_index(x))
    {
        translate_numeric_index(buf, x);
        return END_WITH_LOWERED_DIGIT;
    }

    // Dot 12456 ends superscript
    translate_expression(buf, x);
    append_char(buf, ']');
    return END_WITH_OTHER;
}


/// @brief Symbolic operator definition
typedef struct
{
    const char* mpName;
    const char* mpBrl;
    int         mLeading;
    int         mTrailing;
} SymbolRec;


/** @brief Translates string as symbolic operator.
    @return     Non-zero value if translation was successful.
  */
static int
translate_symbolic_operator(StrBuf* buf, const char* name)
{
    const SymbolRec symbols[] =
    {
        // Operators without space padding
        {UTF8_MIDDLE_DOT,   ";'",   0, 0},
        {UTF8_DOT_OPERATOR, ";'",   0, 0},
        {"∇", "_0",   0, 0},
        {"∠", "_[",   0, 0},
        {"∫", "!",    0, 0},
        {"∬", "!!",   0, 0},
        {"∭", "!!!",  0, 0},
        {"∮", "@!",   0, 0},
        {"∯", "@!!",  0, 0},
        {"&", "@&",   0, 0},
        {"∂", "@d",   0, 0},
        {"#", "_8",   0, 0},
        {"∐", "_v",   0, 0},
        {"‖", "__",   0, 0},
        {"|", "_",    0, 0},
        {"○", "$3",   0, 0},
        {"△", "$d",   0, 0},
        {"□", "$q",   0, 0},
        {"∑", "_s",   0, 0},
        {"∏", "_p",   0, 0},
        {"⊕", "<;6>", 0, 0},
        {"⊖", "<;->", 0, 0},
        {"⊗", "<;8>", 0, 0},
        {"⊘", "<;4>", 0, 0},
        {"⊙", "<;'>", 0, 0},
        {"⊜", "<;7>", 0, 0},
        {"¬", "\\-",  0, 0},
        {"∼", "\"-",  0, 0},
        {"′", "@9",   0, 0},
        {"″", "@99",  0, 0},
        {"‴", "@999", 0, 0},

        // Escaped punctuations without space padding
        {")", ",7",   0, 0},
        {"!", ",6",   0, 0},
        {".", ",4",   0, 0},
        {":", ",3",   0, 0},
        {";", ",2",   0, 0},
        {",", ",1",   0, 0},

        // Dot 56 operators with optional leading space
        {"×", ";8",  -1, 0},
        {"÷", ";4",  -1, 0},
        {"±", ";6-", -1, 0},
        {"∓", ";-6", -1, 0},
        {"=", ";7",  -1, 0},
        {"≜", ";;7", -1, 0},
        {"+", ";6",  -1, 0},
        {"−", ";-",  -1, 0},
        {"-", ";-",  -1, 0},

        // Operators with leading space
        {"⌅", "^7",    1, 0},
        {"∧", "^8",    1, 0},
        {"∨", "^6",    1, 0},
        {"∗", "\\=",   1, 0},
        {"∘", "\\7",   1, 0},
        {"∅", "\\0",   1, 0},
        {"∩", "\\8",   1, 0},
        {"∪", "\\6",   1, 0},
        {"∖", "\\*",   1, 0},
        {"⊆", "\\[7",  1, 0},
        {"⊇", "\\o7",  1, 0},
        {"⊂", "\\[",   1, 0},
        {"⊃", "\\o",   1, 0},
        {"∈", "\\9",   1, 0},
        {"∋", "\\)",   1, 0},
        {"∀", "\\1",   1, 0},
        {"∃", "\\5",   1, 0},
        {"⊴", "\\+7",  1, 0},
        {"⊵", "\\u7",  1, 0},
        {"⊲", "\\+",   1, 0},
        {"⊳", "\\u",   1, 0},
        {"⊈", "\\\"[7",1, 0},
        {"⊉", "\\\"o7",1, 0},
        {"⊄", "\\\"[", 1, 0},
        {"⊅", "\\\"o", 1, 0},
        {"∉", "\\\"9", 1, 0},
        {"∄", "\\\"5", 1, 0},
        {"≤", "[7",    1, 0},
        {"≥", "o7",    1, 0},
        {"≡", "77",    1, 0},
        {"≈", "_7",    1, 0},
        {"≃", ".7",    1, 0},
        {"≅", "-7",    1, 0},
        {"≲", "[_7",   1, 0},
        {"≳", "o_7",   1, 0},
        {"∝", "37",    1, 0},
        {"⟂", "#'",    1, 0},
        {"≠", "\"7",   1, 0},
        {"≰", "\"[7",  1, 0},
        {"≱", "\"o7",  1, 0},
        {"≢", "\"77",  1, 0},
        {"≉", "\"_7",  1, 0},
        {"≄", "\".7",  1, 0},
        {"≇", "\"-7",  1, 0},
        {"≴", "\"[_7", 1, 0},
        {"≵", "\"o_7", 1, 0},
        {"∦", "\"__",  1, 0},
        {"≨", "[\"7",  1, 0},
        {"≩", "o\"7",  1, 0},
        {"→", "3o",    1, 0},
        {"←", "[3",    1, 0},
        {"⟶", "33o",   1, 0},
        {"⟵", "[33",   1, 0},
        {"↦", "^3o",   1, 0},
        {"↣", "o3o",   1, 0},
        {"↢", "[3[",   1, 0},
        {"↠", "3oo",   1, 0},
        {"↞", "[[3",   1, 0},
        {"⇒", "\\3o",  1, 0},
        {"⇐", "\\[3",  1, 0},
        {"↔", "[3o",   1, 0},
        {"⇔", "\\[3o", 1, 0},
        {"⇀", "3e",    1, 0},
        {"↼", "33",    1, 0},
        {"⇁", "39",    1, 0},
        {"⇌", "53e",   1, 0},
        {"⥄", "553e",  1, 0},
        {"⥂", "53ee",  1, 0},
        {"↑", "3i",    1, 0},
        {"↓", "35",    1, 0},
        {"↛", "\"3o",  1, 0},
        {"↚", "\"[3",  1, 0},
        {"≔", "3;7",   1, 0},
        {"(", "7",     1, 0},

        // Operators with leading and trailing spaces
        {"≪", "[[",   1, 1},
        {"≫", "oo",   1, 1},
        {"<", "[",    1, 1},
        {">", "o",    1, 1},
        {"∴", ",*",   1, 1},
        {"∵", "@/",   1, 1},
        {"⊢", "_1",   1, 1},
        {"⊣", "\"l",  1, 1},
        {"⊨", "_3",   1, 1},
        {"⫤", "3l",   1, 1},
        {"∎", "==",   1, 1},
        {"≮", "\"[",  1, 1},
        {"≯", "\"o",  1, 1},
        {"⊬", "\"_1", 1, 1},
        {"⊭", "\"_3", 1, 1},

        // List terminator
        {NULL}
    };
    const SymbolRec* rec;
    int              style = get_math_style(buf);
    int              unspaced = is_unspaced(buf);
    const char*      brl;

    // Check for bracket
    brl = bracket_to_brl(name);
    if (brl)
    {
        // Report successful translation
        append_text(buf, brl);
        return 1;
    }

    // Check all known symbols
    for (rec = symbols;  rec->mpName;  ++rec)
    {
        if (strcmp(name, rec->mpName) != 0)  continue;

        // Append leading space if necessary
        if (rec->mLeading && !has_trailing_space(buf))
        {
            if ((rec->mpBrl[0] == '3') && unspaced)
            {
                // Insert a dot 4 separator before arrow
                append_char(buf, '@');
            }
            else 
            {
                int need_space = 0;
                if (rec->mLeading == 1)
                {
                    // Compulsory leading space is always needed
                    need_space = 1;
                }
                else if (!unspaced)
                {
                    // Retain optional leading space whenever possible
                    need_space = 1;
                }

                // Add leading space now
                if (need_space)  append_char(buf, ' ');
            }
        }

        // Append braille translation
        if (style == STYLE_BOLD && strchr(";^\\", rec->mpBrl[0]))
        {
            // Dot 4 follows dot 45, dot 56 and dot 1256 in bold operator
            append_char(buf, rec->mpBrl[0]);
            append_char(buf, '@');
            append_text(buf, rec->mpBrl + 1);
        }
        else
        {
            if (style == STYLE_BOLD)  append_char(buf, '@');
            append_text(buf, rec->mpBrl);
        }

        // Append trailing space if necessary
        if (rec->mTrailing)
            append_char(buf, ' ');

        // Report successful translation
        return 1;
    }

    // Indicate translation has failed
    return 0;
}


/** @brief Translates string as word operator.
    @return     Non-zero value if translation was successful.
  */
static int
translate_word_operator(StrBuf* buf, const char* name)
{
    const char* abbreviations[] =
    {
        "sin",       "s",
        "arcsin",    "@s",
        "cos",       "c",
        "arccos",    "@c",
        "tan",       "t",
        "arctan",    "@t",
        "sec",       "-",
        "arcsec",    "@-",
        "cosec",     "<",
        "arccosec",  "@<",
        "cot",       "\\",
        "arccot",    "@\\",
        "sinh",      "hs",
        "arcsinh",   "8s",
        "cosh",      "hc",
        "arccosh",   "8c",
        "tanh",      "ht",
        "arctanh",   "8t",
        "sech",      "h-",
        "arcsech",   "8-",
        "cosech",    "h<",
        "arccosech", "8<",
        "coth",      "h\\",
        "arccoth",   "8\\",
        "log",       "l",
        "antilog",   "@l",
        "colog",     "^l",
        "grad",      "g",
        "curl",      "%",
        "div",       "?",

        // List terminator
        NULL
    };
    const char** ab;
    int          is_cap;
    const char*  ptr;

    // Make sure the name consists purely of Latin characters and symbols
    for (ptr = name;  *ptr;  ++ptr)
    {
        char c = *ptr;
        if ((c >= 'a') && (c <= 'z'))  continue;
        if ((c >= 'A') && (c <= 'Z'))  continue;
        if (strchr(".-", c))  continue;

        // Failed check, report translation failure
        return 0;
    }

    // Check for initial capitalization
    is_cap = ((*name >= 'A') && (*name <= 'Z'));

    // Check all known abbreviations 
    for (ab = abbreviations;  *ab;  ab += 2)
    {
        const char* full = ab[0];
        const char* abbrev = ab[1];
        if (strcasecmp(name, full) != 0)  continue;

        // Replace word with abbreviated form
        name = abbrev;
        break;
    }

    // Translate now
    append_char(buf, '$');
    for (ptr = name;  *ptr;  ++ptr)
    {
        char c = *ptr;
        if ((c >= 'A') && (c <= 'Z'))  c = c - 'A' + 'a';
        else if (c == '.')  c = '0';

        // Please capitalization sign before first Latin character 
        if (is_cap && (c >= 'a') && (c <= 'z'))
        {
            is_cap = 0;
            append_char(buf, ',');
        }
        append_char(buf, c);
    }
    if (is_cap)  append_char(buf, ',');

    // Report successful translation
    return 1;
}


/// @brief Literal text definition
typedef struct
{
    const char* mpText;
    const char* mpBraille;
    const char* mpNormalFount;
    const char* mpBoldFount;
} LiteralRec;


/** @brief Looks up braille translation of the first Unicode character.
    @return     The braille translation.
    @return     @p NULL if the first Unicode character cannot be translated.
    @param style    Math style.
    @param str      The NULL-terminated string to translate.
    @param fount    The fount sign that preceeds the translation.
  */
static const char*
lookup_literal(int style, const char* str, const char** fount)
{
    const LiteralRec literals[] =
    {
        /* Lowercase Latin */
        {"a", "a", ";", "@"},
        {"b", "b", ";", "@"},
        {"c", "c", ";", "@"},
        {"d", "d", ";", "@"},
        {"e", "e", ";", "@"},
        {"f", "f", ";", "@"},
        {"g", "g", ";", "@"},
        {"h", "h", ";", "@"},
        {"i", "i", ";", "@"},
        {"j", "j", ";", "@"},
        {"k", "k", ";", "@"},
        {"l", "l", ";", "@"},
        {"m", "m", ";", "@"},
        {"n", "n", ";", "@"},
        {"o", "o", ";", "@"},
        {"p", "p", ";", "@"},
        {"q", "q", ";", "@"},
        {"r", "r", ";", "@"},
        {"s", "s", ";", "@"},
        {"t", "t", ";", "@"},
        {"u", "u", ";", "@"},
        {"v", "v", ";", "@"},
        {"w", "w", ";", "@"},
        {"x", "x", ";", "@"},
        {"y", "y", ";", "@"},
        {"z", "z", ";", "@"},

        /* Uppercase Latin */
        {"A", "a", ",", "^"},
        {"B", "b", ",", "^"},
        {"C", "c", ",", "^"},
        {"D", "d", ",", "^"},
        {"E", "e", ",", "^"},
        {"F", "f", ",", "^"},
        {"G", "g", ",", "^"},
        {"H", "h", ",", "^"},
        {"I", "i", ",", "^"},
        {"J", "j", ",", "^"},
        {"K", "k", ",", "^"},
        {"L", "l", ",", "^"},
        {"M", "m", ",", "^"},
        {"N", "n", ",", "^"},
        {"O", "o", ",", "^"},
        {"P", "p", ",", "^"},
        {"Q", "q", ",", "^"},
        {"R", "r", ",", "^"},
        {"S", "s", ",", "^"},
        {"T", "t", ",", "^"},
        {"U", "u", ",", "^"},
        {"V", "v", ",", "^"},
        {"W", "w", ",", "^"},
        {"X", "x", ",", "^"},
        {"Y", "y", ",", "^"},
        {"Z", "z", ",", "^"},

        // Lowercase Greek
        {"α", "a", ".", "@."},
        {"β", "b", ".", "@."},
        {"γ", "g", ".", "@."},
        {"δ", "d", ".", "@."},
        {"ε", "e", ".", "@."},
        {"ϵ", "e", ".", "@."},  // Alternate lowercase epsilon (U+03f5)
        {"ζ", "z", ".", "@."},
        {"η", ":", ".", "@."},
        {"θ", "?", ".", "@."},
        {"ϑ", "?", ".", "@."},  // Alternate lowercase theta (U+03d1)
        {"ι", "i", ".", "@."},
        {"κ", "k", ".", "@."},
        {"ϰ", "k", ".", "@."},  // Alternate lowercase kappa (U+03f0)
        {"λ", "l", ".", "@."},
        {"μ", "m", ".", "@."},
        {"ν", "n", ".", "@."},
        {"ξ", "x", ".", "@."},
        {"ο", "o", ".", "@."},
        {"π", "p", ".", "@."},
        {"ϖ", "p", ".", "@."},  // Alternate lowercase pi (U+03d6)
        {"ρ", "r", ".", "@."},
        {"ϱ", "r", ".", "@."},  // Alternate lowercase rho (U+03f1)
        {"σ", "s", ".", "@."},
        {"ϲ", "s", ".", "@."},  // Alternate lowercase sigma (U+03f2)
        {"τ", "t", ".", "@."},
        {"υ", "u", ".", "@."},
        {"φ", "f", ".", "@."},
        {"ϕ", "f", ".", "@."},  // Alternate lowercase phi (U+03d5)
        {"χ", "&", ".", "@."},
        {"ψ", "y", ".", "@."},
        {"ω", "w", ".", "@."},

        // Uppercase Greek
        {"Α", "a", "_", "^_"},
        {"Β", "b", "_", "^_"},
        {"Γ", "g", "_", "^_"},
        {"Δ", "d", "_", "^_"},
        {"Ε", "e", "_", "^_"},
        {"Ζ", "z", "_", "^_"},
        {"Η", ":", "_", "^_"},
        {"Θ", "?", "_", "^_"},
        {"ϴ", "?", "_", "^_"},  // Alternate uppercase theta (U+03f4)
        {"Ι", "i", "_", "^_"},
        {"Κ", "k", "_", "^_"},
        {"Λ", "l", "_", "^_"},
        {"Μ", "m", "_", "^_"},
        {"Ν", "n", "_", "^_"},
        {"Ξ", "x", "_", "^_"},
        {"Ο", "o", "_", "^_"},
        {"Π", "p", "_", "^_"},
        {"Ρ", "r", "_", "^_"},
        {"Σ", "s", "_", "^_"},
        {"Ϲ", "s", "_", "^_"},  // Alternate uppercase sigma (U+03f9)
        {"Τ", "t", "_", "^_"},
        {"Υ", "u", "_", "^_"},
        {"ϒ", "u", "_", "^_"},  // Alternate uppercase upsilon (U+03d2)
        {"Φ", "f", "_", "^_"},
        {"Χ", "&", "_", "^_"},
        {"Ψ", "y", "_", "^_"},
        {"Ω", "w", "_", "^_"},

        // Monetary symbols
        {"$", "@4", NULL, NULL},
        {"¢", "@c", NULL, NULL},
        {"€", "@e", NULL, NULL},
        {"£", "@l", NULL, NULL},

        // Symbols
        {"∞", "=",   NULL, NULL},
        {"?", "--",  NULL, NULL},
        {"…", "'''", NULL, NULL},
        {"(", "7",   NULL, NULL},
        {")", ",7",  NULL, NULL},
        {"!", "6",   NULL, NULL},
        {".", "4",   NULL, NULL},
        {":", "3",   NULL, NULL},
        {";", "2",   NULL, NULL},
        {",", "1",   NULL, NULL},
        {"'", "'",   NULL, NULL},
        {" ", " ",   NULL, NULL},

        // List terminator
        {NULL}
    };
    const LiteralRec* lit;
    size_t            len = strlen(str);

    // Check all known literals
    for (lit = literals;  lit->mpText;  ++lit)
    {
        if (!starts_with(str, len, lit->mpText))  continue;
        switch (style)
        {
            case STYLE_BOLD:  *fount = lit->mpBoldFount;    break;
            default:          *fount = lit->mpNormalFount;  break;
        }
        return lit->mpBraille;
    }

    // Report failed translation
    return NULL;
}


/// @brief Translates string as literal text
static void
translate_literal_text(StrBuf* buf,
        int isIdentifier, const char* str)
{
    const char* prev_fount;
    const char* next_fount;
    const char* next_str;
    const char* next_letter;
    const char* fount;
    const char* letter;
    int         style = get_math_style(buf);

    // Ignore NULL pointers
    if (!str)  return;

    // Setup starting condition
    fount       = ";";
    next_str    = str;
    next_fount  = NULL;
    next_letter = lookup_literal(style, next_str, &next_fount);

    // Go through every Unicode character in the string
    while (*next_str)
    {
        const char* out_fount;

        // Remember information of previous literal
        prev_fount = fount;

        // Re-use information for current literal
        str    = next_str;
        fount  = next_fount;
        letter = next_letter;

        // Lookup information about next literal
        next_str    = next_utf8(str);
        next_fount  = NULL;
        next_letter = lookup_literal(style, next_str, &next_fount);

        // Skip literals that has no translation
        if (!letter)  continue;

        // Determine actual fount in output
        out_fount = fount;
        if (get_end_type(buf) != END_WITH_OTHER)
        {
            // Retain fount if confusion may arise
            fount = NULL;
        }
        else if (strcmp(tail_of_buffer(buf, 1), "_") == 0)
        {
            // Retain fount if confusion may arise
            fount = NULL;
        }
        else if ((strcmp(tail_of_buffer(buf, 2), ";-") == 0) &&
                (strcmp(letter, "--") == 0))
        {
            // Escape dashes that follow a minus sign
            out_fount = "#";
        }
        else if (isIdentifier && (strcmp(letter, "o") == 0))
        {
            // Retain fount for 'o' to distinguish from right bracket
            fount = NULL;
        }
        else if (isIdentifier && (strcmp(letter, ":") == 0))
        {
            // Retain fount for 'η' to distinguish from overbar
            fount = NULL;
        }
        else if (prev_fount != fount)  // Start of new fount
        {
            if (fount && (fount == next_fount))
            {
                // Use double letter fount for multi-letter sequence
                if      (strcmp(fount, ";") == 0)  out_fount = ";";
                else if (strcmp(fount, ",") == 0)  out_fount = ",,";
                else if (strcmp(fount, "@") == 0)  out_fount = "@@";
                else if (strcmp(fount, ".") == 0)  out_fount = "..";
                else if (strcmp(fount, "^") == 0)  out_fount = "^^";
                else if (strcmp(fount, "_") == 0)  out_fount = ",_";
                else                               fount = ";";
            }
            else
            {
                // Forget fount for single letter
                fount = ";";
            }
        }
        else  // Continuation of same fount
        {
            // Reset to lowercase Latin fount at end of sequence
            if (fount != next_fount)
            {
                if (fount)  fount = NULL;
                else        fount = ";";
            }
            out_fount = NULL;
        }

        // Append translation to buffer now
        append_text(buf, out_fount);
        append_text(buf, letter);
        set_end_type(buf, END_WITH_OTHER);
    }
}


/** @brief Translates <mi> containing mathematical unit.
    @return     Non-zero value if translation was successful.
  */
static int
translate_mathematical_unit(StrBuf* buf, mxml_node_t* x)
{
    const char* units[] =
    {
        // Units without leading space
        "$",    "@4",
        "¢",    "@c",
        "€",    "@e",
        "£",    "@l",
        "/",    "_/",
        "°",    "0",
        "℃",    "0,c",
        "℉",    "0,f",
        "′",    ".",
        "″",    "_",
        "㎭",   "-",

        // Units with leading space
        "Å",    " ^a",
        "%",    " 3p",

        // List terminator
        NULL
    };
    const char** u;
    const char*  text;
    size_t       len;

    // Check all known symbolic units
    text = get_element_text(x);
    if (!strlen(text))  return 0;
    for (u = units;  *u;  u += 2)
    {
        char b0 = u[1][0];
        if (strcmp(text, u[0]) != 0)  continue;

        // Strip dot 4 from monetary unit at start of translation
        if ((b0 == '@') && has_trailing_space(buf))
        {
            append_text(buf, u[1] + 1);
            return 1;
        }

        // Add '+' to prevent misinterpretation of degree as subscript
        if ((b0 == '0') && (get_end_type(buf) == END_WITH_LOWERED_DIGIT))
            append_char(buf, '+');

        append_text(buf, u[1]);
        return 1;
    }

    // Translate textual unit
    if (get_end_type(buf) == END_WITH_UNIT_MI)
    {
        if (strcmp(tail_of_buffer(buf, 1), "/") != 0)
            append_char(buf, '\'');
    }
    else if (!has_trailing_space(buf))
        append_char(buf, ' ');
    if ((text[0] >= 'a' && text[0] <= 'z') &&
            ((text[1] == '\0') || (text[1] >= 'A' && text[1] <= 'Z')))
    {
        // Append fount for single Latin letter or lower case followed by
        // upper case letter
        append_char(buf, ';');
    }

    // Lowercase Latin fount sign not needed in translation
    set_end_type(buf, END_WITH_OTHER);
    translate_literal_text(buf, NOT_IDENTIFIER, text);
    return 1;
}


/// @brief Translates <math> node
static int
translate_math(StrBuf* buf, mxml_node_t* x)
{
    return translate_children(buf, x);
}


/// @brief Translates <menclose> node
static int
translate_menclose(StrBuf* buf, mxml_node_t* x)
{
    const char* notation = mxmlElementGetAttr(x, "notation");
    int         simple = is_simple_term(x);

    // Retain spaces in <menclose>
    set_unspaced(buf, 0);

    // Dot 5 strikes out the next term
    if (strcmp(notation, "updiagonalstrike") == 0)
        append_char(buf, '"');

    // Translate enclosed items
    if (!simple)  append_char(buf, '<');
    translate_children(buf, x);
    if (!simple)  append_char(buf, '>');

    // Dot 4-1456 encloses previous term in annuity symbol
    if (strcmp(notation, "actuarial") == 0)
        append_text(buf, "@?");
    return END_WITH_OTHER;
}


/// @brief Translates <mrow> node
static int
translate_mrow(StrBuf* buf, mxml_node_t* x)
{
    return translate_children(buf, x);
}


/// @brief Translates <mpadded> node
static int
translate_mpadded(StrBuf* buf, mxml_node_t* x)
{
    return translate_children(buf, x);
}


/// @brief Count the number of columns in a <mtr> node
static void
get_mtr_size(mxml_node_t* x, int* col)
{
    *col = 0;
    for (x = first_child_elem(x);  x;  x = next_elem(x))
        if (is_xml_element(x, "mtd"))  ++*col;
}


/// @brief Count the number of rows and columns in a <mtable> node
static void
get_mtable_size(mxml_node_t* x, int* maxRow, int* maxCol)
{
    *maxRow = 0;
    *maxCol = 0;

    // Count <mtr>
    for (x = first_child_elem(x);  x;  x = next_elem(x))
    {
        if (is_xml_element(x, "mtr"))
        {
            mxml_node_t* y;

            // Count <mtd> in row
            int col = 0;
            get_mtr_size(x, &col);

            // Update dimension
            ++*maxRow;
            if (*maxCol < col)  *maxCol = col;
        }
    }
}


/// @brief Translates the matrix row represented by a <mtr> node
static void
translate_matrix_row(StrBuf* buf, mxml_node_t* x)
{
    StrBuf* col_buf;
    int     count;
    int     old_style;

    // Update math style
    col_buf = create_temporary_buffer(buf);
    update_math_style(col_buf, x);

    // Translate every cell in the row
    count = 0;
    for (x = first_child_elem(x);  x;  x = next_elem(x))
    {
        // Look for <mtd> node
        if (!is_xml_element(x, "mtd"))  continue;

        // Translate cell with minimum number of spaces
        col_buf->mpStr[0] = '\0';
        set_end_type(col_buf, END_WITH_OTHER);
        set_unspaced(col_buf, 1);
        translate_children(col_buf, x);
        strip_trailing_index_terminators(col_buf);

        // Add brackets if there are spaces in the translation
        if (strchr(col_buf->mpStr, ' '))
        {
            prepend_char(col_buf, '<');
            append_char(col_buf, '>');
        }

        // Append content now
        if (count > 0)
        {
            // Append space between cells
            append_char(buf, ' ');
        }
        append_text(buf, col_buf->mpStr);

        // Increment column count
        ++count;
    }
    destroy_buffer(col_buf);
}


/// @brief Translates the matrix represented by a <mtable> node
static void
translate_matrix(StrBuf* buf, mxml_node_t* x,
        const char* openBrl, const char* closeBrl)
{
    // Determine matrix dimension
    int     open_with_bar = (openBrl[strlen(openBrl) - 1] == '_');
    int     max_row = 0;
    int     max_col = 0;
    StrBuf* row_buf;
    int     count;
    int     old_style;

    // Ignore empty matrices
    get_mtable_size(x, &max_row, &max_col);
    if (!max_row)  return;
    if (!max_col)  return;

    // Always enclose single row matrix in dot 123456 matrix symbols
    if (max_row == 1)
    {
        openBrl  = "=";
        closeBrl = "=";
    }

    // Open matrix
    append_text(buf, openBrl);

    // Update math style
    row_buf = create_temporary_buffer(buf);
    update_math_style(row_buf, x);

    // Translate every row in the matrix
    count   = 0;
    for (x = first_child_elem(x);  x;  x = next_elem(x))
    {
        // Look for <mtr> node
        if (!is_xml_element(x, "mtr"))  continue;

        // Translate row content
        row_buf->mpStr[0] = '\0';
        translate_matrix_row(row_buf, x);

        // Append content now
        if (count > 0)
        {
            // Append dot 1256 row separator between rows
            append_char(buf, '\\');
        }
        else if (open_with_bar)
        {
            // Use lowercase Latin fount in first cell if confusion may occur
            char c = row_buf->mpStr[0];
            if ((c >= 'a') && (c <= 'z'))  append_char(buf, ';');
        }
        append_text(buf, row_buf->mpStr);

        // Increment row count
        ++count;
    }
    destroy_buffer(row_buf);

    // Close matrix
    append_text(buf, closeBrl);
}


/// @brief Groups the terms inside a <mfenced> node based on item separators
static void
group_terms_in_set(mxml_node_t* x)
{
    // Group terms based on column markers and insert separators
    mxml_node_t* start = NULL;
    mxml_node_t* end = NULL;
    mxml_node_t* el = first_child_elem(x);
    while (el)
    {
        mxml_node_t* next = next_elem(el);

        if (is_item_separator(el))
        {
            // Group content of current term
            if (start)  group_siblings("TERM", start, end);

            // Prepare for next term
            start = NULL;
            end   = NULL;
        }
        else
        {
            // Update extent of current term
            if (!start)  start = el;
            end = el;
        }

        // Advance to next element
        el = next;
    }

    // Group content of last term
    if (start)  group_siblings("TERM", start, end);
}


/// @brief Translates the terms inside a <mfenced> node
static void
translate_terms_in_set(StrBuf* buf, mxml_node_t* x)
{
    StrBuf*      term_buf;
    mxml_node_t* last_sep;
    mxml_node_t* el;
    int          old_style;

    // Update math style
    term_buf = create_temporary_buffer(buf);
    update_math_style(term_buf, x);

    // Group terms first
    group_terms_in_set(x);

    // Separately translate each term
    for (el = first_child_elem(x);  el;  el = next_elem(el))
    {
        mxml_node_t* sep = prev_elem(el);
        if (!is_xml_element(el, "TERM"))  continue;

        // Translation term with minimum number of spaces
        term_buf->mpStr[0] = '\0';
        set_end_type(term_buf, END_WITH_OTHER);
        set_unspaced(term_buf, 1);
        translate_children(term_buf, el);
        strip_trailing_index_terminators(term_buf);

        if (sep)
        {
            int explicit = 0;

            // USe explicit separator if it is not a comma
            if (!is_operator(sep, ","))  explicit = 1;

            // USe explicit separator if the term starts with dot 56 operator
            if ((term_buf->mpStr[0] == ';') &&
                    strchr("4678-;", term_buf->mpStr[1]))
                explicit = 1;

            // Append explicit separator if necessary
            if (explicit)  translate_math_node(buf, sep);

            // Always delimit terms with a space
            append_char(buf, ' ');
        }

        // Append content
        append_text(buf, term_buf->mpStr);
    }
    destroy_buffer(term_buf);
}


/// @brief Translates <mfenced> node
static int
translate_mfenced(StrBuf* buf, mxml_node_t* x)
{
    const char*  sep = mxmlElementGetAttr(x, "separators");
    const char*  open_brl;
    const char*  close_brl;
    mxml_node_t* child = first_child_elem(x);
    mxml_node_t* el;

    // Retain spaces in <mfenced>
    set_unspaced(buf, 0);

    // Translate opening bracket
    open_brl = bracket_to_brl(mxmlElementGetAttr(x, "open"));
    if (!open_brl)  open_brl = bracket_to_brl("(");

    // Translate closing bracket
    close_brl = bracket_to_brl(mxmlElementGetAttr(x, "close"));
    if (!close_brl)  close_brl = bracket_to_brl(")");

    // Translate matrix, which is a <mtable> inside <mfenced>
    if (is_xml_element(child, "mtable"))
    {
        translate_matrix(buf, child, open_brl, close_brl);
        return END_WITH_OTHER;
    }

    // Translate set, which contains item separators
    if (contains_item_separator(x))
    {
        append_text(buf, open_brl);
        translate_terms_in_set(buf, x);
        append_text(buf, close_brl);
        return END_WITH_OTHER;
    }

    // Translate normal expression
    append_text(buf, open_brl);
    translate_children(buf, x);
    append_text(buf, close_brl);
    return END_WITH_OTHER;
}


/// @brief Translates <mfrac> node
static int
translate_mfrac(StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* num = first_child_elem(x);
    mxml_node_t* denom = num ? next_elem(num) : NULL;
    int          simple;
    int          need_bracket = 1;

    // Retain spaces in <mfrac>
    set_unspaced(buf, 0);

    // Make sure the required child elements are present
    if (!num)  return END_WITH_OTHER;
    if (!denom)  return END_WITH_OTHER;

    // Convert simple fraction
    if (is_numeric_mn(num) && is_numeric_mn(denom))
    {
        const char* value = get_element_text(x);
        append_char(buf, '#');
        translate_normal_digits(buf, get_element_text(num));
        translate_lowered_digits(buf, get_element_text(denom));
        return END_WITH_LOWERED_DIGIT;
    }

    // Determine whether the fraction requires bracket
    if (is_identifier(num, NULL) && is_identifier(denom, NULL) &&
            !is_extendable_ukmaths_node(prev_elem(x)) &&
            !is_extendable_ukmaths_node(next_elem(x)))
        need_bracket = 0;

    // Enclose the fraction in bracket
    if (need_bracket)  append_char(buf, '<');

    // Enclose complex numerator in brackets
    simple = is_simple_term(num);
    if (!simple)  append_char(buf, '<');
    num = group_standalone_mi(num);
    translate_math_node(buf, num);
    if (!simple)  append_char(buf, '>');

    // Dot 456-34 is fraction line
    append_text(buf, "_/");

    // Enclose complex denominator in brackets
    simple = is_simple_term(denom);
    if (!simple)  append_char(buf, '<');
    denom = group_standalone_mi(denom);
    translate_math_node(buf, denom);
    if (!simple)  append_char(buf, '>');

    // Enclose the fraction in bracket
    if (need_bracket)  append_char(buf, '>');
    return END_WITH_OTHER;
}


/// @brief Translates <mroot> node
static int
translate_mroot(StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* index = base ? next_elem(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Dot 145 starts root
    append_char(buf, '%');

    // Translate root index as subscript
    translate_subscript(buf, index, NOT_UNDERSCRIPT);

    // Translate base
    return translate_base(buf, base);
}


/// @brief Translates <msqrt> node
static int
translate_msqrt(StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* next = base ? next_elem(base) : NULL;

    // Make sure there is at least one child element
    if (!base)  return END_WITH_OTHER;

    // Group multiple child elements inside <mrow>
    if (next)
    {
        mxml_node_t* last = next;
        while (next)
        {
            last = next;
            next = next_elem(next);
        }
        base = group_siblings("mrow", base, last);
    }

    // Dot 145 starts root
    append_char(buf, '%');

    // Translate base
    return translate_base(buf, base);
}


/// @brief Translates <msub> or <munder> node
static int
translate_msub_munder(StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* index = base ? next_elem(base) : NULL;
    int          is_underscript = 0;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Check whether to explicitly translate as underscript
    if (is_xml_element(x, "munder"))
        is_underscript = is_simple_term(x);

    // Translate base
    translate_base(buf, base);

    // Translate index as subscript
    if (is_numeric_index(base))  append_char(buf, '*');       
    return translate_subscript(buf, index, is_underscript);
}


/// @brief Translates <msup> or <mover> node
static int
translate_msup_mover(StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* index = base ? next_elem(base) : NULL;
    int          style = get_math_style(buf);
    int          is_overscript = 0;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Check whether to explicitly translate as overscript
    if (is_xml_element(x, "mover"))
        is_overscript = is_simple_term(x);

    // Handle mathematical units with power
    if (is_identifier(base, NULL) && is_numeric_index(index) &&
            (find_math_style(style, base) == STYLE_NORMAL))
    {
        translate_mathematical_unit(buf, base);
        translate_superscript(buf, index, is_overscript);
        return END_WITH_UNIT_MI;
    }

    // Translate base
    translate_base(buf, base);

    // Translate index as superscript
    return translate_superscript(buf, index, is_overscript);
}


/// @brief Translates <msubsup> or <munderover> node
static int
translate_msubsup_munderover(StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* sub = base ? next_elem(base) : NULL;
    mxml_node_t* sup = sub ? next_elem(sub) : NULL;
    int          is_underscript = 0;
    int          is_overscript = 0;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!sub)  return END_WITH_OTHER;
    if (!sup)  return END_WITH_OTHER;

    // Check whether to explicitly translate as underscript and overscript
    if (is_xml_element(x, "munderover"))
    {
        is_underscript = 1;
        is_overscript  = 1;
    }

    // Translate base
    translate_base(buf, base);

    // Translate subscript
    if (is_numeric_index(base))  append_char(buf, '*');       
    translate_subscript(buf, sub, is_underscript);

    // Translate superscript
    return translate_superscript(buf, sup, is_overscript);
}


/// @brief Translates <mmultiscripts> node
static int
translate_mmultiscripts(StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* pre = NULL;
    mxml_node_t* postsub = NULL;
    mxml_node_t* postsup = NULL;
    mxml_node_t* presub = NULL;
    mxml_node_t* presup = NULL;

    if (!base)  return END_WITH_OTHER;
    pre = mxmlGetNextSibling(base);
    if (!is_xml_element(pre, "mprescripts"))
    {
        postsub = pre;
        if (!postsub)  return END_WITH_OTHER;
        postsup = mxmlGetNextSibling(postsub);
        if (!postsup)  return END_WITH_OTHER;
        pre = mxmlGetNextSibling(postsup);
    }
    if (is_xml_element(pre, "mprescripts"))
    {
        presub = mxmlGetNextSibling(pre);
        if (!presub)  return END_WITH_OTHER;
        presup = mxmlGetNextSibling(presub);
        if (!presup)  return END_WITH_OTHER;
    }

    // Make sure the required child elements are present
    if (!postsub && !presub)  return END_WITH_OTHER;

    // Treat <none> indices as non-existent
    if (is_xml_element(presub,  "none"))  presub = NULL;
    if (is_xml_element(presup,  "none"))  presup = NULL;
    if (is_xml_element(postsub, "none"))  postsub = NULL;
    if (is_xml_element(postsup, "none"))  postsup = NULL;

    // Enclose translation in brackets to clarify index ownership
    append_char(buf, '<');

    // Translate pre-superscript
    if (presup)  translate_superscript(buf, presup, NOT_OVERSCRIPT);

    // Translate pre-subscript
    if (presub)
    {
        if (is_numeric_index(presub))  append_char(buf, '*');       
        translate_subscript(buf, presub, NOT_UNDERSCRIPT);
    }

    // Translate base
    translate_base(buf, base);

    // Translate post-subscript
    if (postsub)  translate_subscript(buf, postsub, NOT_UNDERSCRIPT);

    // Translate post-superscript
    if (postsup)  translate_superscript(buf, postsup, NOT_OVERSCRIPT);

    // Enclose translation in brackets to clarify index ownership
    append_char(buf, '>');
    return END_WITH_OTHER;
}


/// @brief Translates <mtext> and <merror> node
static int
translate_mtext_merror(StrBuf* buf, mxml_node_t* x)
{
    const char* str;

    // Append leading space
    if (!has_trailing_space(buf))  append_char(buf, ' ');

    // Append opening quote
    append_text(buf, "8");

    // Lowercase Latin fount sign not needed after opening quote
    set_end_type(buf, END_WITH_OTHER);

    // Translate each of the child text node
    for (x = mxmlGetFirstChild(x);  x;  x = mxmlGetNextSibling(x))
    {
        int         whitespace = 0;
        const char* str = mxmlGetText(x, &whitespace);
        if (!str)  continue;

        // Add sufficient number of spaces
        for (;  whitespace > 0;  --whitespace)  append_char(buf, ' ');

        // Translate actual string
        translate_literal_text(buf, NOT_IDENTIFIER, str);
    }

    // Append closing quote and trailing space
    append_text(buf, "0 ");

    // Remember ending
    return END_WITH_MTEXT;
}


/// @brief Translates <mo> node
static int
translate_mo(StrBuf* buf, mxml_node_t* x)
{
    const char* name = get_element_text(x);

    // Attempt to translate as symbolic operator
    if (translate_symbolic_operator(buf, name))
        return END_WITH_OTHER;

    // Attempt to translate as word operator
    if (translate_word_operator(buf, name))
        return END_WITH_WORD_MO;

    // Return existing style in buffer if translation failed
    return get_math_style(buf);
}


/// @brief Translates <mi> node
static int
translate_mi(StrBuf* buf, mxml_node_t* x)
{
    StrBuf* text;
    int     style = get_math_style(buf);

    // Translate <mi> nodes with "normal" style as mathematical units 
    if (style == STYLE_NORMAL)
    {
        translate_mathematical_unit(buf, x);
        return END_WITH_UNIT_MI;
    }

    // Concatenate text from succeeding <mi> nodes
    text = create_temporary_buffer(buf);
    append_text(text, get_element_text(x));
    for (;;)
    {
        const char*  name;

        // Stop when there is no more succeeding nodes
        mxml_node_t* next = next_elem(x);
        if (!next)  break;

        // Stop if next node is not <mi>
        if (strcmp(mxmlGetElement(next), "mi") != 0)  break;

        // Stop if math style is different
        if (find_math_style(style, next) != style)  break;

        // Concatenate text from next element and delete it
        append_text(text, get_element_text(next));
        mxmlDelete(next);
    }

    // Translate as literal text
    translate_literal_text(buf, IS_IDENTIFIER, text->mpStr);

    // Clean up
    destroy_buffer(text);
    return END_WITH_OTHER;
}


/// @brief Translates <mn> node
static int
translate_mn(StrBuf* buf, mxml_node_t* x)
{
    const char* value = get_element_text(x);
    append_char(buf, '#');
    translate_normal_digits(buf, value);
    return END_WITH_NUM_MN;
}


/// @brief Handler definition
typedef struct
{
    const char* mpTag;
    int(*mpHandler)(StrBuf*, mxml_node_t*);
} HandlerRec;


/// @brief Translates a MathML element
static int
translate_math_node(StrBuf* buf, mxml_node_t* x)
{
    const HandlerRec handlers[] =
    {
        {"math",          translate_math              },
        {"menclose",      translate_menclose          },
        {"mrow",          translate_mrow              },
        {"mpadded",       translate_mpadded           },
        {"mfenced",       translate_mfenced           },
        {"mfrac",         translate_mfrac             },
        {"mroot",         translate_mroot             },
        {"msqrt",         translate_msqrt             },
        {"msub",          translate_msub_munder       },
        {"msup",          translate_msup_mover        },
        {"msubsup",       translate_msubsup_munderover},
        {"munder",        translate_msub_munder       },
        {"mover",         translate_msup_mover        },
        {"munderover",    translate_msubsup_munderover},
        {"mmultiscripts", translate_mmultiscripts     },
        {"mtext",         translate_mtext_merror      },
        {"merror",        translate_mtext_merror      },
        {"mo",            translate_mo                },
        {"mi",            translate_mi                },
        {"mn",            translate_mn                },

        // List terminator
        {NULL}
    };
    const char*       name;
    const HandlerRec* rec;

    // Ignore invalid node
    if (!x)  return get_end_type(buf);

    // Process only DOM elements
    name = mxmlGetElement(x);
    if (!name)  return get_end_type(buf);

    // Check all known handlers
    for (rec = handlers;  rec->mpTag;  ++rec)
    {
        if (strcmp(name, rec->mpTag) == 0)
        {
            // Update private data and invoke handler
            int old_style = update_math_style(buf, x);
            int unspaced = is_unspaced(buf);
            int end_with = rec->mpHandler(buf, x);

            // Restore private data
            set_math_style(buf, old_style);
            set_unspaced(buf, unspaced);
            set_end_type(buf, end_with);
            return end_with;
        }
    }

    return get_end_type(buf);
}


char*
brl2mml_to_ukmaths(const char* mml, int* used)
{
    StrBuf*      buf = create_buffer();
    mxml_node_t* x = parse_mathml(mml);
    int          len;

    // Perform translation and clean up
    translate_children(buf, x);
    strip_trailing_index_terminators(buf);
    mxmlDelete(x);

    // Assume the XML data was fully consumed
    *used = strlen(mml);
    return unwrap_str(buf);
}


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
