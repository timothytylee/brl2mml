/**********************************************************************
*
*  Filename:    to_ukmaths.c
*  Description: Source file for UK Maths functions in libbrl2mml
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


#ifdef WIN32
#  define strcasecmp _stricmp
#endif


#define UTF8_MIDDLE_DOT                 "·" // U+00b7
#define UTF8_DOT_OPERATOR               "⋅" // U+22c5


/// @brief MathML style
enum
{
    STYLE_NORMAL = 0,
    STYLE_BOLD,
    STYLE_ITALIC,
    STYLE_FRAKTUR,
};

/// @brief StrBuf content identifier
enum
{
    END_WITH_OTHER = 0,
    END_WITH_MTEXT,
    END_WITH_WORD_MO,
    END_WITH_NUM_MN,
};


/// Forward declare translate_math_node() for recursive invocation
static long
translate_math_node(int style, StrBuf* buf, mxml_node_t* x);


/// @brief Checks whether a string buffer has a trailing space
static int
has_trailing_space(StrBuf* buf)
{
    size_t len = strlen(buf->mpStr);

    // Consider empty string to have trailing space
    if (len == 0)  return 1;

    // Otherwise check for trailing space
    return (strchr(" <", buf->mpStr[len - 1]) != NULL);
}


/// @brief Appends text with optional Latin fount in between if necessary
static void
append_text_with_fount(StrBuf* buf, const char* text)
{
    char c = text[0];
    if ((c >= 'a') && (c <= 'z'))
    {
        // New text starts with Latin character
        size_t len = strlen(buf->mpStr);

        // If buffer ends in alpha character, add fount to avoid confusion
        if (len > 0)
        {
            char c = buf->mpStr[len - 1];
            if ((buf->mpPriv != END_WITH_OTHER) ||
                    (c == '_') || ((c >= 'a') && (c <= 'z')))
                append_char(buf, ';');
        }
    }

    // Append new text now
    append_text(buf, text);
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


/// @brief Checks whether an element is a <mn> node containing only digits
static int
is_numeric_mn(mxml_node_t* x)
{
    const char* value;

    // Check for <mn> element
    if (!is_xml_element(x, "mn"))  return 0;

    // Check every character in the value of <mn>
    value = get_element_text(x);
    while (*value)
    {
        if (strchr("0123456789", *value))
        {
            ++value;
            continue;
        }
        else if ((0 == strncmp(value, UTF8_DOT_OPERATOR,
                               strlen(UTF8_DOT_OPERATOR))) ||
                (0 == strncmp(value, UTF8_MIDDLE_DOT,
                              strlen(UTF8_MIDDLE_DOT))))
        {
            value += strlen(UTF8_DOT_OPERATOR);
            continue;
        }
        return 0;
    }

    // All characters are digits
    return 1;
}


static const char* overheads[] =
{
    "¯",   ":",
    "ˆ",   "@:",
    "ˇ",   "@>",
    "~",   "^:",
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
                x = get_next_element(x);
                if (is_numeric_mn(x))  return 1;
            }
            return 0;
        }

        // <munder> or <mover> with simple base and overhead symbol are simple
        if (is_xml_element(x, "mover") || is_xml_element(x, "munder"))
        {
            x = first_child_elem(x);
            if (is_simple_term(x))
            {
                x = get_next_element(x);
                if (is_overhead_mo(x))  return 1;
            }
            return 0;
        }

        // Check child node
        x = first_child_elem(x);
        if (!x)  break;

        // Not simple if there is a sibling element
        if (get_next_element(x))  return 0;
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
        if (strchr("0123456789", *value))
        {
            append_char(buf, *value);
            ++value;
        }
        else if ((0 == strncmp(value, UTF8_DOT_OPERATOR,
                               strlen(UTF8_DOT_OPERATOR))) ||
                (0 == strncmp(value, UTF8_MIDDLE_DOT,
                              strlen(UTF8_MIDDLE_DOT))))
        {
            // Dot 46-3 is the dot used in statistics
            append_text(buf, ".'");
            value += strlen(UTF8_DOT_OPERATOR);
        }
        else  ++value;
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


/** @brief Looks up the translation of a standalone Latin identifier.
    @return     @p NULL if the node is not a Latin identifier.
    @return     The fount that preceeds the translated letter.
    @param style    Math style.
    @param x        The <mi> node to translate.
    @param letter   The translated letter.
  */
static const char*
lookup_latin(int style, mxml_node_t* x, char* letter)
{
    const char* name;
    char        c;

    // Ignore NULL nodes
    if (!x)  return 0;

    // Latin identifiers contain only one character
    name = get_element_text(x);
    if (strlen(name) != 1)  return 0;

    // Lowercase Latin identifiers are translated as is
    c = name[0];
    if ((c >= 'a') && (c <= 'z'))
    {
        if (letter)  *letter = c;
        switch (style)
        {
            case STYLE_BOLD:  return "@";
            default:          return ";";
        }
    }

    // Uppercase Latin identifiers are shown in lowercase
    if ((c >= 'A') && (c <= 'Z'))
    {
        if (letter)  *letter = c - 'A' + 'a';
        switch (style)
        {
            case STYLE_BOLD:  return "^";
            default:          return ",";
        }
    }

    // Report failed translation
    return NULL;
}


/** @brief Looks up the translation of a standalone Greek identifier.
    @return     @p NULL if the node is not a Greek identifier.
    @return     The fount that preceeds the translated letter.
    @param style    Math style.
    @param x        The <mi> node to translate.
    @param letter   The translated letter.
  */
static const char*
lookup_greek(int style, mxml_node_t* x, char* letter)
{
    const char* greek_letters[] =
    {
        "Α", "α", "a",
        "Β", "β", "b",
        "Γ", "γ", "g",
        "Δ", "δ", "d",
        "Ε", "ε", "e",
        "Ε", "ϵ", "e",  // Alternate form of lowercase epsilon (U+03f5)
        "Ζ", "ζ", "z",
        "Η", "η", ":",
        "Θ", "θ", "?",
        "ϴ", "ϑ", "?",  // Alternate form of theta (U+03f4, U+03d1)
        "Ι", "ι", "i",
        "Κ", "κ", "k",
        "Κ", "ϰ", "k",  // Alternate form of lowercase kappa (U+03f0)
        "Λ", "λ", "l",
        "Μ", "μ", "m",
        "Ν", "ν", "n",
        "Ξ", "ξ", "x",
        "Ο", "ο", "o",
        "Π", "π", "p",
        "Π", "ϖ", "p",  // Alternate form of lowercase pi (U+03d6)
        "Ρ", "ρ", "r",
        "Ρ", "ϱ", "r",  // Alternate form of lowercase rho (U+03f1)
        "Σ", "σ", "s",
        "Ϲ", "ϲ", "s",  // Alternate form of sigma (U+03f9, U+03f2)
        "Τ", "τ", "t",
        "Υ", "υ", "u",
        "ϒ", "υ", "u",  // Alternate form of uppercase upsilon (U+03d2)
        "Φ", "φ", "f",
        "Φ", "ϕ", "f",  // Alternate form of lowercase phi (U+03d5)
        "Χ", "χ", "&",
        "Ψ", "ψ", "y",
        "Ω", "ω", "w",

        // List terminator
        NULL
    };

    const char*  name;
    const char** greek;

    // Ignore NULL nodes
    if (!x)  return 0;

    // Check all known Greek identifiers 
    name = get_element_text(x);
    for (greek = greek_letters;  *greek;  greek += 3)
    {
        // Test for uppercase identifier
        if (strcmp(name, greek[0]) == 0)
        {
            if (letter)  *letter = greek[2][0];
            switch (style)
            {
                case STYLE_BOLD:  return "^_";
                default:          return "_";
            }
        }

        // Test for lowercase identifier
        if (strcmp(name, greek[1]) == 0)
        {
            if (letter)  *letter = greek[2][0];
            switch (style)
            {
                case STYLE_BOLD:  return "@.";
                default:          return ".";
            }
        }
    }

    // Report failed translation
    return NULL;
}


/** @brief Obtains fount for previous element.
    @return     @p NULL if previous element does not have a fount.
  */
static const char*
get_previous_fount(mxml_node_t* x)
{
    const char*  attr = NULL;
    mxml_node_t* prev = get_prev_element(x);
    if (prev)  attr = mxmlElementGetAttr(prev, "fount");
    return attr;
}


/// @brief Clears fount sign stored in previous element.
static void
clear_previous_fount(mxml_node_t* x)
{
    mxml_node_t* prev = get_prev_element(x);
    if (prev)  mxmlElementDeleteAttr(prev, "fount");
}


/// @brief Translates child nodes
static long
translate_children(int style, StrBuf* buf, mxml_node_t* x)
{
    StrBuf* tmp_buf = create_buffer();
    long    end_with = END_WITH_OTHER;

    // Translate each of the child node
    for (x = first_child_elem(x);  x;  x = get_next_element(x))
        end_with = translate_math_node(style, tmp_buf, x);

    // Append translated content to buffer
    strip_trailing_space(tmp_buf);
    append_text_with_fount(buf, tmp_buf->mpStr);
    destroy_buffer(tmp_buf);
    return end_with;
}


/// @brief Translates node as a base
static long
translate_base(int style, StrBuf* buf, mxml_node_t* x)
{
    // Translate node into temporary buffer
    StrBuf* tmp_buf = create_buffer();
    long    end_with;
    clear_previous_fount(x);
    end_with = translate_math_node(style, tmp_buf, x);
    strip_trailing_space(tmp_buf);

    // Prevent arrow from being mis-interpreted as numeric index
    if (tmp_buf->mpStr[0] == '3')  prepend_char(tmp_buf, '@');

    // Surround complex expression with bracket
    if (!is_simple_term(x))
    {
        prepend_char(tmp_buf, '<');
        append_char(tmp_buf, '>');
        end_with = END_WITH_OTHER;
    }

    // Append translated content to buffer
    append_text_with_fount(buf, tmp_buf->mpStr);
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
static void
translate_subscript(int style, StrBuf* buf, mxml_node_t* x)
{
    // Numeric indices are encoded as lowered braille digits
    if (is_numeric_mn(x))
    {
        translate_lowered_digits(buf, get_element_text(x));
        return;
    }

    // Dot 16 and 12456 encloses subscript
    append_char(buf, '*');
    if (!translate_overhead_symbol(style, buf, x))
        translate_base(style, buf, x);
    append_char(buf, ']');
}


/// @brief Translates node as a superscript
static void
translate_superscript(int style, StrBuf* buf, mxml_node_t* x)
{
    // Overhead symbols are appended directly
    if (translate_overhead_symbol(style, buf, x))  return;

    // Dot 346 starts superscript
    append_char(buf, '+');

    // Numeric indices are encoded as lowered braille digits
    if (is_numeric_mn(x))
    {
        translate_lowered_digits(buf, get_element_text(x));
        return;
    }

    // Dot 12456 ends superscript
    translate_base(style, buf, x);
    append_char(buf, ']');
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
        "sin",    "s",
        "cos",    "c",
        "tan",    "t",
        "sec",    "-",
        "cosec",  "<",
        "cot",    "\\",
        "sinh",   "hs",
        "cosh",   "hc",
        "tanh",   "ht",
        "sech",   "h-",
        "cosech", "h<",
        "coth",   "h\\",
        "log",    "l",
        "colog",  "^l",
        "grad",   "g",
        "curl",   "%",
        "div",    "?",

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


/// @brief Translates string as literal text
static void
translate_literal_text(StrBuf* buf, const char* str)
{
    if (!str)  return;
    for (;  *str;  ++str)
    {
        char c = *str;
        switch (c)
        {
            case '(':   append_text(buf, "7");   break;
            case ')':   append_text(buf, ",7");  break;
            case '!':   append_text(buf, "6");   break;
            case '.':   append_text(buf, "4");   break;
            case ':':   append_text(buf, "3");   break;
            case ';':   append_text(buf, "2");   break;
            case ',':   append_text(buf, "1");   break;
            case '\'':  append_text(buf, "'");   break;
            case ' ':   append_text(buf, " ");   break;
            default:
                if ((c >= 'a') && (c <= 'z'))
                    append_char(buf, c);
                else if ((c >= 'A') && (c <= 'Z'))
                {
                    append_char(buf, ',');
                    append_char(buf, c - 'A' + 'a');
                }
                break;
        }
    }
}


/** @brief Translates <mi> containing Latin identifier.
    @return     Non-zero value if translation was successful.
  */
static int
translate_latin_identifier(int style, StrBuf* buf, mxml_node_t* x)
{
    char        letter;
    const char* prev_fount;
    const char* fount = lookup_latin(style, x, &letter);
    if (!fount)  return 0;

    // Remember fount to support processing of consecutive identifiers
    mxmlElementSetAttr(x, "fount", fount);

    // Handle consecutive sequence
    prev_fount = get_previous_fount(x);
    if (!prev_fount || (strcmp(prev_fount, fount) != 0))
    {
        // Start of new fount
        const char*  next_fount;
        mxml_node_t* y = get_next_element(x);
        if (!is_xml_element(y, "mi"))  y = NULL;
        next_fount = lookup_latin(style, y, NULL);
        if (next_fount && strcmp(fount, next_fount) == 0)
        {
            // Start of multiple identifiers with same fount
            if      (strcmp(fount, ",") == 0)  fount = ",,";
            else if (strcmp(fount, "@") == 0)  fount = "@@";
            else if (strcmp(fount, "^") == 0)  fount = "^^";
        }

        // Drop lowercase fount if possible
        while (strcmp(fount, ";") == 0)
        {
            mxml_node_t* prev;
            size_t       len;
            char         last = '\0';

            // Retain fount for 'o' to distinguish from right bracket
            if (letter == 'o')  break;

            // Retain fount if confusion may arise
            if (buf->mpPriv != END_WITH_OTHER)  break;

            // Retain fount if previous fount is different
            if (prev_fount)  break;

            // Drop redundant fount
            fount = NULL;
            break;
        }
    }
    else
    {
        // Same fount as previous one
        switch (letter)
        {
            case 'o':
                // 'o' should always have fount sign
                mxmlElementDeleteAttr(x, "fount");
                break;

            default:
                fount = NULL;
                break;
        }
    }

    // Append translation
    if (fount)
    {
        append_text(buf, fount);
        append_char(buf, letter);
    }
    else if (prev_fount)
    {
        append_char(buf, letter);
    }
    else
    {
        char text[2] = {letter, '\0'};
        append_text_with_fount(buf, text);
    }
    return 1;
}


/** @brief Translates <mi> containing Greek identifier.
    @return     Non-zero value if translation was successful.
  */
static int
translate_greek_identifier(int style, StrBuf* buf, mxml_node_t* x)
{
    char        letter;
    const char* prev_fount;
    const char* fount = lookup_greek(style, x, &letter);
    if (!fount)  return 0;

    // Remember fount to support processing of consecutive identifiers
    mxmlElementSetAttr(x, "fount", fount);

    // Handle consecutive sequence
    prev_fount = get_previous_fount(x);
    if (!prev_fount || (strcmp(prev_fount, fount) != 0))
    {
        // Start of new fount
        const char*  next_fount;
        mxml_node_t* y = get_next_element(x);
        if (!is_xml_element(y, "mi"))  y = NULL;
        next_fount = lookup_greek(style, y, NULL);
        if (next_fount && strcmp(fount, next_fount) == 0)
        {
            // Start of multiple identifiers with same fount
            if      (strcmp(fount, ".") == 0)  fount = "..";
            else if (strcmp(fount, "_") == 0)  fount = ",_";
        }
    }
    else
    {
        // Same fount as previous one
        switch (letter)
        {
            case ':':
                // 'η' should always have fount sign
                mxmlElementDeleteAttr(x, "fount");
                break;

            default:
                fount = "";
                break;
        }
    }

    // Append translation
    append_text(buf, fount);
    append_char(buf, letter);
    return 1;
}


/** @brief Translates <mi> containing symbolic identifier.
    @return     Non-zero value if translation was successful.
  */
static int
translate_symbolic_identifier(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* name = get_element_text(x);
    const char* symbols[] =
    {
        "?", "--",
        "…", "'''",

        // List terminator
        NULL
    };
    const char** sym;

    // Check all known symbolic identifiers 
    for (sym = symbols;  *sym;  sym += 2)
    {
        if (strcmp(name, sym[0]) != 0)  continue;

        // Found symbolic identifier, translate now
        if (strcmp(sym[1], "--") == 0)
        {
            // Escape dashes that follow a minus sign
            int len = strlen(buf->mpStr);
            if ((len >= 2) && (strcmp(buf->mpStr + len - 2, ";-") == 0))
                append_char(buf, '#');
        }
        append_text(buf, sym[1]);

        // Report successful translation
        return 1;
    }

    // Report failed translation
    return 0;
}


/// @brief Translates <math> node
static long
translate_math(int style, StrBuf* buf, mxml_node_t* x)
{
    return translate_children(style, buf, x);
}


/// @brief Translates <mstyle> node
static long
translate_mstyle(int style, StrBuf* buf, mxml_node_t* x)
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


/// @brief Translates <mtable> node
static long
translate_mtable(int style, StrBuf* buf, mxml_node_t* x)
{
    // TODO: Implement it
    return END_WITH_OTHER;
}


/// @brief Translates <mrow> node
static long
translate_mrow(int style, StrBuf* buf, mxml_node_t* x)
{
    return translate_children(style, buf, x);
}


/// @brief Translates <mfenced> node
static long
translate_mfenced(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* open_attr = mxmlElementGetAttr(x, "open");
    const char* close_attr = mxmlElementGetAttr(x, "close");
    const char* sep = mxmlElementGetAttr(x, "separators");

    // Append opening bracket
    if      (!open_attr)                    append_text(buf, "<");
    else if (strcmp(open_attr, "(" ) == 0)  append_text(buf, "<");
    else if (strcmp(open_attr, "[" ) == 0)  append_text(buf, "(");
    else if (strcmp(open_attr, "]" ) == 0)  append_text(buf, ")");
    else if (strcmp(open_attr, "{" ) == 0)  append_text(buf, "[");
    else if (strcmp(open_attr, "〈") == 0)  append_text(buf, ".(");

    // Translate content inside bracket
    translate_children(style, buf, x);

    // Append closing bracket
    if      (!close_attr)                    append_text(buf, ">");
    else if (strcmp(close_attr, ")" ) == 0)  append_text(buf, ">");
    else if (strcmp(close_attr, "]" ) == 0)  append_text(buf, ")");
    else if (strcmp(close_attr, "[" ) == 0)  append_text(buf, "(");
    else if (strcmp(close_attr, "}" ) == 0)  append_text(buf, "o");
    else if (strcmp(close_attr, "〉") == 0)  append_text(buf, ".)");
    return END_WITH_OTHER;
}


/// @brief Translates <mfrac> node
static long
translate_mfrac(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* num = first_child_elem(x);
    mxml_node_t* denom = num ? get_next_element(num) : NULL;
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
        return END_WITH_OTHER;
    }

    // Enclose the fraction in bracket
    append_char(buf, '<');

    // Enclose numerator in brackets
    simple = is_simple_term(num);
    if (!simple)  append_char(buf, '<');
    translate_math_node(style, buf, num);
    if (!simple)  append_char(buf, '>');

    // Dot 456-34 is fraction line
    append_text(buf, "_/");

    // Enclose denominator in brackets
    simple = is_simple_term(denom);
    if (!simple)  append_char(buf, '<');
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
    mxml_node_t* index = base ? get_next_element(base) : NULL;

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
    mxml_node_t* next = base ? get_next_element(base) : NULL;

    // Make sure there is at least one child element
    if (!base)  return END_WITH_OTHER;

    // Group multiple child elements inside <mrow>
    if (next)
    {
        mxml_node_t* last = next;
        while (next)
        {
            last = next;
            next = get_next_element(next);
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
    mxml_node_t* index = base ? get_next_element(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Translate base
    translate_base(style, buf, base);

    // Translate index as subscript
    if (is_numeric_mn(base))  append_char(buf, '*');       
    translate_subscript(style, buf, index);
    return END_WITH_OTHER;
}


/// @brief Translates <msup> or <mover> node
static long
translate_msup_mover(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* index = base ? get_next_element(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!index)  return END_WITH_OTHER;

    // Translate base
    translate_base(style, buf, base);

    // Translate index as superscript
    translate_superscript(style, buf, index);
    return END_WITH_OTHER;
}


/// @brief Translates <msubsup> or <munderover> node
static long
translate_msubsup_munderover(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = first_child_elem(x);
    mxml_node_t* sub = base ? get_next_element(base) : NULL;
    mxml_node_t* sup = sub ? get_next_element(sub) : NULL;

    // Make sure the required child elements are present
    if (!base)  return END_WITH_OTHER;
    if (!sub)  return END_WITH_OTHER;
    if (!sup)  return END_WITH_OTHER;

    // Translate base
    translate_base(style, buf, base);

    // Translate subscript
    if (is_numeric_mn(base))  append_char(buf, '*');       
    translate_subscript(style, buf, sub);

    // Translate superscript
    translate_superscript(style, buf, sup);
    return END_WITH_OTHER;
}


/// @brief Checks whether a node is a <node> element
static int
is_none_element(mxml_node_t* x)
{
    const char* name;
    if (!x)  return;
    name = mxmlGetElement(x);
    return (name && (strcmp(name, "none") == 0));
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
        if (is_numeric_mn(presub))  append_char(buf, '*');       
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


/// @brief Translates <mtext> node
static long
translate_mtext(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* str;

    // Append leading space
    if (!has_trailing_space(buf))  append_char(buf, ' ');

    // Append opening quote
    append_text(buf, "8");

    // Translate each of the child text node
    for (x = mxmlGetFirstChild(x);  x;  x = mxmlGetNextSibling(x))
    {
        int         whitespace = 0;
        const char* str = mxmlGetText(x, &whitespace);
        if (!str)  continue;

        // Add sufficient number of spaces
        for (;  whitespace > 0;  --whitespace)  append_char(buf, ' ');

        // Translate actual string
        translate_literal_text(buf, str);
    }

    // Append closing quote and trailing space
    append_text(buf, "0 ");

    // Remember ending
    return END_WITH_MTEXT;
}


/// @brief Translates <mspace> node
static long
translate_mspace(int style, StrBuf* buf, mxml_node_t* x)
{
    // Ignore <mspace>
    return (long)buf->mpPriv;
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
    const char* name = get_element_text(x);

    // Attempt to translate as Latin identifier
    if (translate_latin_identifier(style, buf, x))
        return END_WITH_OTHER;

    // Attempt to translate as Greek identifier
    if (translate_greek_identifier(style, buf, x))
        return END_WITH_OTHER;

    // Attempt to translate as symbolic identifier
    if (translate_symbolic_identifier(style, buf, x))
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
        {"mstyle",        translate_mstyle            },
        {"menclose",      translate_menclose          },
        {"mtable",        translate_mtable            },
        {"mrow",          translate_mrow              },
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
        {"mtext",         translate_mtext             },
        {"mspace",        translate_mspace            },
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
        buf->mpPriv = (void*)end_with;
        return end_with;
    }
}


char*
brl2mml_to_ukmaths(const char* mml, int* used)
{
    mxml_node_t* x;
    mxml_node_t* elem;
    StrBuf*      buf = create_buffer();

    // Parse XML into DOM tree
    x = mxmlNewElement(MXML_NO_PARENT, "xml");
    mxmlLoadString(x, mml, MXML_NO_CALLBACK);

    // Translate parsed elements
    for (elem = first_child_elem(x);  elem;  elem = get_next_element(elem))
        translate_math_node(STYLE_NORMAL, buf, elem);

    // Clean up
    mxmlDelete(x);

    // Assume the XML data was fully consumed
    *used = strlen(mml);
    return unwrap_str(buf);
}


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
