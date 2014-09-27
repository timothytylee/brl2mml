/**********************************************************************
*
*  Filename:    to_ukmaths.c
*  Description: Source file for UK Maths functions in libbrl2mml
*  Version:     $Revision: 1.16 $
*  Date:        $Date: 2014/09/27 12:43:11 $
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


enum
{
    STYLE_NORMAL = 0,
    STYLE_BOLD,
    STYLE_ITALIC,
    STYLE_FRAKTUR,
};


/// Forward declare translate_math_node() for recursive invocation
static void
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


/// @brief Translates digit to un-shifted numerical braille
static char
digit_to_normal_brl(char c)
{
    if (c == '0')  return 'j';
    else           return (c - '1' + 'a');
}


/// @brief Translates string as normal braille digits
static void
translate_as_normal_digits(StrBuf* buf, const char* value)
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
translate_as_lowered_digits(StrBuf* buf, const char* value)
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


/// @brief Translates child nodes
static void
translate_children(int style, StrBuf* buf, mxml_node_t* x)
{
    StrBuf* tmp_buf = create_buffer();

    // Translate each of the child node
    for (x = get_first_element(x);  x;  x = get_next_element(x))
        translate_math_node(style, tmp_buf, x);

    // Append translated content to buffer
    strip_trailing_space(tmp_buf);
    append_text(buf, tmp_buf->mpStr);
    destroy_buffer(tmp_buf);
}


/// @brief Translates node as a base
static void
translate_as_base(int style, StrBuf* buf, mxml_node_t* x)
{
    // Translate node into temporary buffer
    StrBuf* tmp_buf = create_buffer();
    translate_math_node(style, tmp_buf, x);
    strip_trailing_space(tmp_buf);

    // Prevent arrow from being mis-interpreted as numeric index
    if (tmp_buf->mpStr[0] == '3')  prepend_char(tmp_buf, '@');

    // Surround complex expression with bracket
    if (!is_xml_element(x, "mn") &&
            !is_xml_element(x, "mi") &&
            !is_xml_element(x, "mo") &&
            !is_xml_element(x, "mfenced"))
    {
        prepend_char(tmp_buf, '<');
        append_char(tmp_buf, '>');
    }

    // Append translated content to buffer
    append_text(buf, tmp_buf->mpStr);
    destroy_buffer(tmp_buf);
}


/// @brief Translates node as a subscript
static void
translate_as_subscript(int style, StrBuf* buf, mxml_node_t* x)
{
    // Numeric indices are encoded as lowered braille digits
    if (is_numeric_mn(x))
    {
        translate_as_lowered_digits(buf, get_element_text(x));
        return;
    }

    // Dot 16 and 12456 encloses subscript
    append_char(buf, '*');
    translate_as_base(style, buf, x);
    append_char(buf, ']');
}


/// @brief Translates node as a superscript
static void
translate_as_superscript(int style, StrBuf* buf, mxml_node_t* x)
{
    // Dot 346 starts superscript
    append_char(buf, '+');

    // Numeric indices are encoded as lowered braille digits
    if (is_numeric_mn(x))
    {
        translate_as_lowered_digits(buf, get_element_text(x));
        return;
    }

    // Dot 12456 ends superscript
    translate_as_base(style, buf, x);
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
translate_as_symbolic_operator(int style, StrBuf* buf, const char* name)
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
        {"∗", "@5",   0, 0},

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
translate_as_word_operator(int style, StrBuf* buf, const char* name)
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
translate_as_literal_text(StrBuf* buf, const char* str)
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


/** @brief Translates string as Latin identifier.
    @return     Non-zero value if translation was successful.
  */
static int
translate_as_latin_identifier(int style, StrBuf* buf, const char* name)
{
    int  is_cap;
    char c = *name;

    // Latin identifier contains only one character
    if (strlen(name) != 1)
        return 0;
    else if ((c >= 'a') && (c <= 'z'))
        is_cap = 0;
    else if ((c >= 'A') && (c <= 'Z'))
    {
        is_cap = 1;
        c = c - 'A' + 'a';
    }
    else
        return 0;

    // Translate identifier now
    if (is_cap)
    {
        if (style == STYLE_BOLD)  append_text(buf, "^");
        else                      append_text(buf, ",");
    }
    else
    {
        if (style == STYLE_BOLD)  append_text(buf, "@");
        else                      append_text(buf, ";");
    }
    append_char(buf, c);

    // Report successful translation
    return 1;
}


/** @brief Translates string as Greek identifier.
    @return     Non-zero value if translation was successful.
  */
static int
translate_as_greek_identifier(int style, StrBuf* buf, const char* name)
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
    const char** greek;
    int          is_cap;

    // Check all known Greek identifiers 
    for (greek = greek_letters;  *greek;  greek += 3)
    {
        if      (strcmp(name, greek[0]) == 0)  is_cap = 1;
        else if (strcmp(name, greek[1]) == 0)  is_cap = 0;
        else                                   continue;

        // Found Greek identifier, translate now
        if (is_cap)
        {
            if (style == STYLE_BOLD)  append_text(buf, "^_");
            else                      append_text(buf, "_");
        }
        else
        {
            if (style == STYLE_BOLD)  append_text(buf, "@.");
            else                      append_text(buf, ".");
        }
        append_text(buf, greek[2]);

        // Report successful translation
        return 1;
    }

    // Report failed translation
    return 0;
}


/** @brief Translates string as symbolic identifier.
    @return     Non-zero value if translation was successful.
  */
static int
translate_as_symbolic_identifier(int style, StrBuf* buf, const char* name)
{
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
static void
translate_math(int style, StrBuf* buf, mxml_node_t* x)
{
    translate_children(style, buf, x);
}


/// @brief Translates <mstyle> node
static void
translate_mstyle(int style, StrBuf* buf, mxml_node_t* x)
{
    translate_children(style, buf, x);
}


/// @brief Translates <menclose> node
static void
translate_menclose(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* notation = mxmlElementGetAttr(x, "notation");

    // Dot 5 strikes out the next term
    if (strcmp(notation, "updiagonalstrike") == 0)
        append_char(buf, '"');

    // Translate a single term
    translate_math_node(style, buf, get_first_element(x));

    // Dot 4-1456 encloses previous term in annuity symbol
    if (strcmp(notation, "actuarial") == 0)
        append_text(buf, "@?");
}


/// @brief Translates <mtable> node
static void
translate_mtable(int style, StrBuf* buf, mxml_node_t* x)
{
    // TODO: Implement it
}


/// @brief Translates <mrow> node
static void
translate_mrow(int style, StrBuf* buf, mxml_node_t* x)
{
    translate_children(style, buf, x);
}


/// @brief Translates <mfenced> node
static void
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
}


/// @brief Translates <mfrac> node
static void
translate_mfrac(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* num = get_first_element(x);
    mxml_node_t* denom = num ? get_next_element(num) : NULL;

    // Make sure the required child elements are present
    if (!num)  return;
    if (!denom)  return;

    // Convert simple fraction
    if (is_numeric_mn(num) && is_numeric_mn(denom))
    {
        const char* value = get_element_text(x);
        append_char(buf, '#');
        translate_as_normal_digits(buf, get_element_text(num));
        translate_as_lowered_digits(buf, get_element_text(denom));
        return;
    }

    // Enclose the fraction in bracket
    append_char(buf, '<');

    // Enclose numerator in brackets
    append_char(buf, '<');
    translate_math_node(style, buf, num);
    append_char(buf, '>');

    // Dot 456-34 is fraction line
    append_text(buf, "_/");

    // Enclose denominator in brackets
    append_char(buf, '<');
    translate_math_node(style, buf, denom);
    append_char(buf, '>');

    // Enclose the fraction in bracket
    append_char(buf, '>');
}


/// @brief Translates <mroot> node
static void
translate_mroot(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = get_first_element(x);
    mxml_node_t* index = base ? get_next_element(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return;
    if (!index)  return;

    // Dot 145 starts root
    append_char(buf, '%');

    // Translate root index as subscript
    translate_as_subscript(style, buf, index);

    // Translate base
    translate_as_base(style, buf, base);
}


/// @brief Translates <msqrt> node
static void
translate_msqrt(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = get_first_element(x);
    mxml_node_t* next = base ? get_next_element(base) : NULL;

    // Make sure there is at least one child element
    if (!base)  return;

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
    translate_as_base(style, buf, base);
}


/// @brief Translates <msub> or <munder> node
static void
translate_msub_munder(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = get_first_element(x);
    mxml_node_t* index = base ? get_next_element(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return;
    if (!index)  return;

    // Translate base
    translate_as_base(style, buf, base);

    // Translate index as subscript
    if (is_numeric_mn(base))  append_char(buf, '*');       
    translate_as_subscript(style, buf, index);
}


/// @brief Translates <msup> or <mover> node
static void
translate_msup_mover(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = get_first_element(x);
    mxml_node_t* index = base ? get_next_element(base) : NULL;

    // Make sure the required child elements are present
    if (!base)  return;
    if (!index)  return;

    // Translate base
    translate_as_base(style, buf, base);

    // Translate index as superscript
    translate_as_superscript(style, buf, index);
}


/// @brief Translates <msubsup> or <munderover> node
static void
translate_msubsup_munderover(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = get_first_element(x);
    mxml_node_t* sub = base ? get_next_element(base) : NULL;
    mxml_node_t* sup = sub ? get_next_element(sub) : NULL;

    // Make sure the required child elements are present
    if (!base)  return;
    if (!sub)  return;
    if (!sup)  return;

    // Translate base
    translate_as_base(style, buf, base);

    // Translate subscript
    if (is_numeric_mn(base))  append_char(buf, '*');       
    translate_as_subscript(style, buf, sub);

    // Translate superscript
    translate_as_superscript(style, buf, sup);
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
static void
translate_mmultiscripts(int style, StrBuf* buf, mxml_node_t* x)
{
    mxml_node_t* base = get_first_element(x);
    mxml_node_t* pre = NULL;
    mxml_node_t* postsub = NULL;
    mxml_node_t* postsup = NULL;
    mxml_node_t* presub = NULL;
    mxml_node_t* presup = NULL;

    if (!base)  return;
    pre = mxmlGetNextSibling(base);
    if (!is_xml_element(pre, "mprescripts"))
    {
        postsub = pre;
        if (!postsub)  return;
        postsup = mxmlGetNextSibling(postsub);
        if (!postsup)  return;
        pre = mxmlGetNextSibling(postsup);
    }
    if (is_xml_element(pre, "mprescripts"))
    {
        presub = mxmlGetNextSibling(pre);
        if (!presub)  return;
        presup = mxmlGetNextSibling(presub);
        if (!presup)  return;
    }

    // Make sure the required child elements are present
    if (!postsub && !presub)  return;

    // Treat <none> indices as non-existent
    if (is_xml_element(presub,  "none"))  presub = NULL;
    if (is_xml_element(presup,  "none"))  presup = NULL;
    if (is_xml_element(postsub, "none"))  postsub = NULL;
    if (is_xml_element(postsup, "none"))  postsup = NULL;

    // Enclose translation in brackets to clarify index ownership
    append_char(buf, '<');

    // Translate pre-superscript
    if (presup)  translate_as_superscript(style, buf, presup);

    // Translate pre-subscript
    if (presub)
    {
        if (is_numeric_mn(presub))  append_char(buf, '*');       
        translate_as_subscript(style, buf, presub);
    }

    // Translate base
    translate_as_base(style, buf, base);

    // Translate post-subscript
    if (postsub)  translate_as_subscript(style, buf, postsub);

    // Translate post-superscript
    if (postsup)  translate_as_superscript(style, buf, postsup);

    // Enclose translation in brackets to clarify index ownership
    append_char(buf, '>');
}


/// @brief Translates <mtext> node
static void
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
        translate_as_literal_text(buf, str);
    }

    // Append closing quote and trailing space
    append_text(buf, "0 ");
}


/// @brief Translates <mspace> node
static void
translate_mspace(int style, StrBuf* buf, mxml_node_t* x)
{
    // Ignore <mspace>
}


/// @brief Translates <mo> node
static void
translate_mo(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* name = get_element_text(x);

    // Attempt to translate as symbolic operator
    if (translate_as_symbolic_operator(style, buf, name))  return;

    // Attempt to translate as word operator
    if (translate_as_word_operator(style, buf, name))  return;
}


/// @brief Translates <mi> node
static void
translate_mi(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* name = get_element_text(x);

    // Attempt to translate as Latin identifier
    if (translate_as_latin_identifier(style, buf, name))  return;

    // Attempt to translate as Greek identifier
    if (translate_as_greek_identifier(style, buf, name))  return;

    // Attempt to translate as symbolic identifier
    if (translate_as_symbolic_identifier(style, buf, name))  return;
}


/// @brief Translates <mn> node
static void
translate_mn(int style, StrBuf* buf, mxml_node_t* x)
{
    const char* value = get_element_text(x);
    append_char(buf, '#');
    translate_as_normal_digits(buf, value);
}


/// @brief Handler definition
typedef struct
{
    const char* mpTag;
    void(*mpHandler)(int, StrBuf*, mxml_node_t*);
} HandlerRec;


/// @brief Translates a MathML element
static void
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
        if (strcmp(name, rec->mpTag) != 0)  continue;
        rec->mpHandler(style, buf, x);
        return;
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
    for (elem = get_first_element(x);  elem;  elem = get_next_element(elem))
        translate_math_node(STYLE_NORMAL, buf, elem);

    // Clean up
    mxmlDelete(x);

    // Assume the XML data was fully consumed
    *used = strlen(mml);
    return unwrap_str(buf);
}


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
