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


/// @brief MathML styles
enum
{
    STYLE_NORMAL = 0,
    STYLE_BOLD,
    STYLE_ITALIC,
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
    StrBuf** mpExtras;      ///< Extra rows for matrix translation
    int      mExtraCount;     ///< Number of extra rows in matrix
    int      mEndType;      ///< String buffer content identifier
} MathData;


/// Forward declare translate_math_node() for recursive invocation
static long
translate_math_node(int style, StrBuf* buf, mxml_node_t* x);


/// @brief Private data destructor
static void
destroy_private_data(void* v)
{
    MathData* priv = (MathData*)v;

    // Release additional rows
    if (priv->mpExtras)
    {
        int n;
        for (n = priv->mExtraCount;  n--;  )
            destroy_buffer(priv->mpExtras[n]);
        free(priv->mpExtras);
    }

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


/// @brief Gets end type in string buffer
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


/// @brief Gets the number of rows in string buffer
static int
get_row_count(StrBuf* buf)
{
    MathData* priv = get_private_data(buf);
    if (priv)  return priv->mExtraCount + 1;
    else       return 1;
}


/// @brief Ensures a string buffer has at least @p count number of rows
static void
set_minimum_row_count(StrBuf* buf, int count)
{
    // Add extra rows as needed
    MathData* priv = get_private_data(buf);
    if (!priv)  return;

    // Do nothing if zero row is requested
    if (!count)  return;

    // Do nothing if there is sufficient capacity
    if ((count - 1) <= priv->mExtraCount)  return;

    // Expand pointer array
    if (priv->mExtraCount == 0)
        priv->mpExtras = (StrBuf**)calloc(count - 1, sizeof(StrBuf*));
    else
        priv->mpExtras = (StrBuf**)realloc(
                priv->mpExtras, sizeof(StrBuf*) * (count - 1));

    // Create new buffers
    while ((count - 1) > priv->mExtraCount)
    {
        priv->mpExtras[priv->mExtraCount] = create_buffer();
        ++priv->mExtraCount;
    }
}


/// @brief Gets a row in a multi-line string buffer by its zero-based index
static StrBuf*
get_row(StrBuf* buf, int idx)
{
    MathData* priv = get_private_data(buf);

    // Ensure there are sufficient number of rows in buffer
    set_minimum_row_count(buf, idx + 1);

    // row[0] is stored in original string buffer
    if (idx == 0)  return buf;

    // Otherwise return required row from string array
    return priv->mpExtras[idx - 1];
}


/// @brief Pads all rows in buffer to same length
static void
equalize_rows(StrBuf* buf)
{
    int    n;
    size_t len = strlen(get_row(buf, 0)->mpStr);
    for (n = get_row_count(buf);  n--;  )
    {
        StrBuf* s = get_row(buf, n);
        size_t  delta = len - strlen(s->mpStr);
        while (delta--)  append_char(s, ' ');
    }
}


/// @brief Appends multi-row string buffer to a string buffer
static int
append_buffer(StrBuf* buf, StrBuf* suffix)
{
    // Equalize sufficient number of rows in target buffer
    int     count = get_row_count(suffix);
    set_minimum_row_count(buf, count);
    equalize_rows(buf);

    // Append suffix
    while (count--)
    {
        StrBuf* dst = get_row(buf, count);
        StrBuf* src = get_row(suffix, count);
        append_text(dst, src->mpStr);
    }
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
    append_buffer(buf, suffix);
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

        // <msub> or <msup> with simple base and numeric index are simple
        if (is_xml_element(x, "msub") || is_xml_element(x, "msup"))
        {
            x = first_child_elem(x);
            if (is_simple_term(x))
            {
                x = next_elem(x);
                if (is_numeric_index(x))  return 1;
            }
            return 0;
        }

        // <munder> or <mover> with simple base and overhead symbol are simple
        if (is_xml_element(x, "mover") || is_xml_element(x, "munder"))
        {
            x = first_child_elem(x);
            if (is_simple_term(x))
            {
                x = next_elem(x);
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


/// @brief Translates child nodes
static long
translate_children(int style, StrBuf* buf, mxml_node_t* x)
{
    StrBuf* tmp_buf = create_buffer();
    long    end_with = END_WITH_OTHER;

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
    for (x = first_child_elem(x);  x;  x = next_elem(x))
        end_with = translate_math_node(style, tmp_buf, x);

    // Append translated content to buffer
    strip_trailing_space(tmp_buf);
    append_buffer_with_fount(buf, tmp_buf);
    destroy_buffer(tmp_buf);
    return end_with;
}


/// @brief Translates node as a base
static long
translate_base(int style, StrBuf* buf, mxml_node_t* x)
{
    StrBuf* tmp_buf = create_buffer();
    int     simple = is_simple_term(x);
    long    end_with;

    // Protect standalone <mi>
    x = group_standalone_mi(x);

    // Translate node into temporary buffer
    end_with = translate_math_node(style, tmp_buf, x);
    strip_trailing_space(tmp_buf);

    // Prevent arrow from being mis-interpreted as numeric index
    if (tmp_buf->mpStr[0] == '3')  prepend_char(tmp_buf, '@');

    // Surround complex expression with bracket
    if (!simple)
    {
        prepend_char(tmp_buf, '<');
        append_char(tmp_buf, '>');
        end_with = END_WITH_OTHER;
    }

    // Append translated content to buffer
    append_buffer_with_fount(buf, tmp_buf);
    destroy_buffer(tmp_buf);
    return end_with;
}


/// @brief Translates node as an overhead symbol
static int
translate_overhead_symbol(int style, StrBuf* buf, mxml_node_t* x)
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
static long
translate_subscript(int style, StrBuf* buf, mxml_node_t* x)
{
    // Numeric indices are encoded as lowered braille digits
    if (is_numeric_index(x))
    {
        translate_numeric_index(buf, x);
        return END_WITH_LOWERED_DIGIT;
    }

    // Dot 16 and 12456 encloses subscript
    append_char(buf, '*');
    if (!translate_overhead_symbol(style, buf, x))
        translate_base(style, buf, x);
    append_char(buf, ']');
    return END_WITH_OTHER;
}


/// @brief Translates node as a superscript
static long
translate_superscript(int style, StrBuf* buf, mxml_node_t* x)
{
    // Overhead symbols are appended directly
    if (translate_overhead_symbol(style, buf, x))  return END_WITH_OTHER;

    // Dot 346 starts superscript
    append_char(buf, '+');

    // Numeric indices are encoded as lowered braille digits
    if (is_numeric_index(x))
    {
        translate_numeric_index(buf, x);
        return END_WITH_LOWERED_DIGIT;
    }

    // Dot 12456 ends superscript
    translate_base(style, buf, x);
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
translate_symbolic_operator(int style, StrBuf* buf, const char* name)
{
    const SymbolRec symbols[] =
    {
        // Operators without space padding
        {UTF8_MIDDLE_DOT,   ";'",   0, 0},
        {UTF8_DOT_OPERATOR, ";'",   0, 0},
        {"∇", "_0",   0, 0},
        {"∠", "_[",   0, 0},
        {"∫", "!",    0, 0},
        {"∮", "@!",   0, 0},
        {"&", "@&",   0, 0},
        {"∂", "@d",   0, 0},
        {"∞", "=",    0, 0},
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

        // Dot 56 operators with leading space
        {"×", ";8",  1, 0},
        {"÷", ";4",  1, 0},
        {"±", ";6-", 1, 0},
        {"∓", ";-6", 1, 0},
        {"=", ";7",  1, 0},
        {"≜", ";;7", 1, 0},
        {"+", ";6",  1, 0},
        {"−", ";-",  1, 0},
        {"-", ";-",  1, 0},

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

    // Check all known handlers
    for (rec = symbols;  rec->mpName;  ++rec)
    {
        if (strcmp(name, rec->mpName) != 0)  continue;

        // Append leading space if necessary
        if (rec->mLeading && !has_trailing_space(buf))
            append_char(buf, ' ');

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
translate_word_operator(int style, StrBuf* buf, const char* name)
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

        // Symbols
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
translate_literal_text(int style, StrBuf* buf,
        int isIdentifier, const char* str)
{
    const char* prev_fount;
    const char* next_fount;
    const char* next_str;
    const char* next_letter;
    const char* fount;
    const char* letter;

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
translate_mathematical_unit(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* units[] =
    {
        // Units without leading space
        "£",    "@l",
        "$",    "@4",
        "¢",    "@c",
        "€",    "@e",
        "/",    "_/",
        "°",    "0",
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
        if (strcmp(text, u[0]) != 0)  continue;
        if ((u[1][0] == '0') && (get_end_type(buf) == END_WITH_LOWERED_DIGIT))
        {
            // Add '+' to prevent misinterpretation of degree as subscript
            append_char(buf, '+');
        }
        append_text(buf, u[1]);
        return 1;
    }

    // Translate textual unit
    if (get_end_type(buf) == END_WITH_UNIT_MI)
    {
        if (strcmp(tail_of_buffer(buf, 1), "/") != 0)
            append_char(buf, '\'');
    }
    else
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
    translate_literal_text(style, buf, NOT_IDENTIFIER, text);
    return 1;
}


/// @brief Translates <math> node
static long
translate_math(int style, StrBuf* buf, mxml_node_t* x)
{
    return translate_children(style, buf, x);
}


/// @brief Translates <menclose> node
static long
translate_menclose(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* notation = mxmlElementGetAttr(x, "notation");
    int         simple = is_simple_term(x);

    // Dot 5 strikes out the next term
    if (strcmp(notation, "updiagonalstrike") == 0)
        append_char(buf, '"');

    // Translate enclosed items
    if (!simple)  append_char(buf, '<');
    translate_children(style, buf, x);
    if (!simple)  append_char(buf, '>');

    // Dot 4-1456 encloses previous term in annuity symbol
    if (strcmp(notation, "actuarial") == 0)
        append_text(buf, "@?");
    return END_WITH_OTHER;
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
translate_matrix_row(int style, StrBuf* buf, mxml_node_t* x)
{
    StrBuf* col_buf;
    int     col;

    // Translate every cell in the row
    col_buf = create_buffer();
    col = 0;
    for (x = first_child_elem(x);  x;  x = next_elem(x), ++col)
    {
        const char* str;

        // Look for <mtd> node
        if (!is_xml_element(x, "mtd"))  continue;

        // Translate cell content
        col_buf->mpStr[0] = '\0';
        translate_children(find_math_style(style, x), col_buf, x);

        // Add brackets if there are spaces in the translation
        if (strchr(col_buf->mpStr, ' '))
        {
            prepend_char(col_buf, '<');
            append_char(col_buf, '>');
        }

        // Append content now
        if (col > 0)
        {
            // Append space between cells
            append_char(buf, ' ');
        }
        append_text(buf, col_buf->mpStr);

        // Increment column index
        ++col;
    }
    destroy_buffer(col_buf);
}


/// @brief Translates the matrix represented by a <mtable> node
static long
translate_matrix(int style, StrBuf* buf, mxml_node_t* x,
        const char* openBrl, const char* closeBrl)
{
    // Determine matrix dimension
    int     open_with_bar = (openBrl[strlen(openBrl) - 1] == '_');
    int     max_row = 0;
    int     max_col = 0;
    StrBuf* row_buf;
    int     row;

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

    // Translate every row in the matrix
    row_buf = create_buffer();
    row     = 0;
    for (x = first_child_elem(x);  x;  x = next_elem(x))
    {
        // Look for <mtr> node
        if (!is_xml_element(x, "mtr"))  continue;

        // Translate row content
        row_buf->mpStr[0] = '\0';
        translate_matrix_row(find_math_style(style, x), row_buf, x);

        // Append content now
        if (row > 0)
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

        // Increment row index
        ++row;
    }
    destroy_buffer(row_buf);

    // Close matrix
    append_text(buf, closeBrl);
    return END_WITH_OTHER;
}


/// @brief Translates <mrow> node
static long
translate_mrow(int style, StrBuf* buf, mxml_node_t* x)
{
    return translate_children(style, buf, x);
}


/// @brief Translates <mpadded> node
static long
translate_mpadded(int style, StrBuf* buf, mxml_node_t* x)
{
    return translate_children(style, buf, x);
}


/// @brief Translates <mfenced> node
static long
translate_mfenced(int style, StrBuf* buf, mxml_node_t* x)
{
    const char*  sep = mxmlElementGetAttr(x, "separators");
    const char*  open_brl;
    const char*  close_brl;
    mxml_node_t* child = first_child_elem(x);

    // Translate opening bracket
    open_brl = bracket_to_brl(mxmlElementGetAttr(x, "open"));
    if (!open_brl)  open_brl = bracket_to_brl("(");

    // Translate closing bracket
    close_brl = bracket_to_brl(mxmlElementGetAttr(x, "close"));
    if (!close_brl)  close_brl = bracket_to_brl(")");

    // Check for matrices, which appear as <mtable> inside <mfenced>
    if (is_xml_element(child, "mtable"))
    {
        style = find_math_style(style, child);
        translate_matrix(style, buf, child, open_brl, close_brl);
        return;
    }

    // Translate content inside bracket
    append_text(buf, open_brl);
    translate_children(style, buf, x);
    append_text(buf, close_brl);
    return END_WITH_OTHER;
}


/// @brief Translates <mfrac> node
static long
translate_mfrac(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* num = first_child_elem(x);
    mxml_node_t* denom = num ? next_elem(num) : NULL;
    int          simple;

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

    // Enclose the fraction in bracket
    append_char(buf, '<');

    // Enclose complex numerator in brackets
    simple = is_simple_term(num);
    if (!simple)  append_char(buf, '<');
    num = group_standalone_mi(num);
    translate_math_node(style, buf, num);
    if (!simple)  append_char(buf, '>');

    // Dot 456-34 is fraction line
    append_text(buf, "_/");

    // Enclose complex denominator in brackets
    simple = is_simple_term(denom);
    if (!simple)  append_char(buf, '<');
    denom = group_standalone_mi(denom);
    translate_math_node(style, buf, denom);
    if (!simple)  append_char(buf, '>');

    // Enclose the fraction in bracket
    append_char(buf, '>');
    return END_WITH_OTHER;
}


/// @brief Translates <mroot> node
static long
translate_mroot(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* index = base ? next_elem(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Dot 145 starts root
    append_char(buf, '%');

    // Translate root index as subscript
    translate_subscript(style, buf, index);

    // Translate base
    return translate_base(style, buf, base);
}


/// @brief Translates <msqrt> node
static long
translate_msqrt(int style, StrBuf* buf, mxml_node_t* x)
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
    return translate_base(style, buf, base);
}


/// @brief Translates <msub> or <munder> node
static long
translate_msub_munder(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* index = base ? next_elem(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Translate base
    translate_base(style, buf, base);

    // Translate index as subscript
    if (is_numeric_index(base))  append_char(buf, '*');       
    return translate_subscript(style, buf, index);
}


/// @brief Translates <msup> or <mover> node
static long
translate_msup_mover(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* index = base ? next_elem(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Handle mathematical units with power
    if (is_xml_element(base, "mi") && is_numeric_index(index) &&
            (find_math_style(style, base) == STYLE_NORMAL))
    {
        translate_mathematical_unit(style, buf, base);
        translate_superscript(style, buf, index);
        return END_WITH_UNIT_MI;
    }

    // Translate base
    translate_base(style, buf, base);

    // Translate index as superscript
    return translate_superscript(style, buf, index);
}


/// @brief Translates <msubsup> or <munderover> node
static long
translate_msubsup_munderover(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* sub = base ? next_elem(base) : NULL;
    mxml_node_t* sup = sub ? next_elem(sub) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!sub)  return END_WITH_OTHER;
    if (!sup)  return END_WITH_OTHER;

    // Translate base
    translate_base(style, buf, base);

    // Translate subscript
    if (is_numeric_index(base))  append_char(buf, '*');       
    translate_subscript(style, buf, sub);

    // Translate superscript
    return translate_superscript(style, buf, sup);
}


/// @brief Translates <mmultiscripts> node
static long
translate_mmultiscripts(int style, StrBuf* buf, mxml_node_t* x)
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
    if (presup)  translate_superscript(style, buf, presup);

    // Translate pre-subscript
    if (presub)
    {
        if (is_numeric_index(presub))  append_char(buf, '*');       
        translate_subscript(style, buf, presub);
    }

    // Translate base
    translate_base(style, buf, base);

    // Translate post-subscript
    if (postsub)  translate_subscript(style, buf, postsub);

    // Translate post-superscript
    if (postsup)  translate_superscript(style, buf, postsup);

    // Enclose translation in brackets to clarify index ownership
    append_char(buf, '>');
    return END_WITH_OTHER;
}


/// @brief Translates <mtext> and <merror> node
static long
translate_mtext_merror(int style, StrBuf* buf, mxml_node_t* x)
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
        translate_literal_text(style, buf, NOT_IDENTIFIER, str);
    }

    // Append closing quote and trailing space
    append_text(buf, "0 ");

    // Remember ending
    return END_WITH_MTEXT;
}


/// @brief Translates <mo> node
static long
translate_mo(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* name = get_element_text(x);

    // Attempt to translate as symbolic operator
    if (translate_symbolic_operator(style, buf, name))
        return END_WITH_OTHER;

    // Attempt to translate as word operator
    if (translate_word_operator(style, buf, name))
        return END_WITH_WORD_MO;
}


/// @brief Translates <mi> node
static long
translate_mi(int style, StrBuf* buf, mxml_node_t* x)
{
    StrBuf* text;

    // Translate <mi> nodes with "normal" style as mathematical units 
    if (style == STYLE_NORMAL)
    {
        translate_mathematical_unit(style, buf, x);
        return END_WITH_UNIT_MI;
    }

    // Concatenate text from succeeding <mi> nodes
    text = create_buffer();
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
    translate_literal_text(style, buf, IS_IDENTIFIER, text->mpStr);

    // Clean up
    destroy_buffer(text);
    return END_WITH_OTHER;
}


/// @brief Translates <mn> node
static long
translate_mn(int style, StrBuf* buf, mxml_node_t* x)
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
    long(*mpHandler)(int, StrBuf*, mxml_node_t*);
} HandlerRec;


/// @brief Translates a MathML element
static long
translate_math_node(int style, StrBuf* buf, mxml_node_t* x)
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
    if (!x)  return;

    // Process only DOM elements
    name = mxmlGetElement(x);
    if (!name)  return;

    // Update math style
    style = find_math_style(style, x);

    // Check all known handlers
    for (rec = handlers;  rec->mpTag;  ++rec)
    {
        long end_with;
        if (strcmp(name, rec->mpTag) != 0)  continue;
        end_with = rec->mpHandler(style, buf, x);
        set_end_type(buf, end_with);
        return end_with;
    }
}


char*
brl2mml_to_ukmaths(const char* mml, int* used)
{
    StrBuf*      buf = create_buffer();
    mxml_node_t* x = parse_mathml(mml);
    int          n;

    // Perform translation and clean up
    translate_children(STYLE_ITALIC, buf, x);
    mxmlDelete(x);

    // Merge extra rows
    equalize_rows(buf);
    for (n = 1;  n < get_row_count(buf);  ++n)
    {
        StrBuf* s = get_row(buf, n);
        append_text(buf, "\n");
        append_text(buf, s->mpStr);
    }

    // Assume the XML data was fully consumed
    *used = strlen(mml);
    return unwrap_str(buf);
}


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
