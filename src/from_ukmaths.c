/**********************************************************************
*
*  Filename:    from_ukmaths.c
*  Description: Implementation of UK Maths to MathML translation
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


#define BASE_EXPR                       0
#define INDEX_EXPR                      1

#define MSPACE_WIDTH                    "0.25em"

#define is_same(a,b)                    ((a && b) || (!a && !b))


/// @brief Letter founts defined under UK Maths
enum
{
    FOUNT_LATIN_SMALL,
    FOUNT_LATIN_CAPITAL,
    FOUNT_GREEK_SMALL,
    FOUNT_GREEK_CAPITAL,
    FOUNT_BOLD_LATIN_SMALL,
    FOUNT_BOLD_LATIN_CAPITAL,
    FOUNT_BOLD_GREEK_SMALL,
    FOUNT_BOLD_GREEK_CAPITAL,
    FOUNT_BOLD_NUMERAL_1,
    FOUNT_BOLD_NUMERAL_2,
    FOUNT_CUSTOM_1_SMALL,
    FOUNT_CUSTOM_1_CAPITAL,
    FOUNT_CUSTOM_2_SMALL,
    FOUNT_CUSTOM_2_CAPITAL,
    FOUNT_CUSTOM_3_SMALL,
    FOUNT_CUSTOM_3_CAPITAL,
    FOUNT_CUSTOM_4_SMALL,
    FOUNT_CUSTOM_4_CAPITAL,
};


/// Forward declare parse_expr() for recursive invocation
static int
parse_expr(mxml_node_t* x, const char* brl, size_t len,
        int isIndex, const char* closeBrl);


/// @brief Checks if a string contains only Latin characters.
static int
is_latin_word(const char* str)
{
    if (!str)  return 0;
    for (;  *str;  ++str)
    {
        char c = *str;
        if ((c >= 'a') && (c <= 'z'))  continue;
        if ((c >= 'A') && (c <= 'Z'))  continue;
        if (c == '-')  continue;
        if (c == '.')  continue;

        // Contains non-Latin character, check failed
        return 0;
    }

    // Successfully checked the whole string
    return 1;
}


/// @brief Checks if a string is a trigonometric operator.
static int
is_trigonometric_operator(const char* str)
{
    const char* operators[] =
    {
        "sin",  "cos",   "tan",  "sec",  "cosec",  "cot",
        "sinh", "cosh",  "tanh", "sech", "cosech", "coth",
        "log",  "colog", "grad", "curl", "div",
        "ln",   "exp",  

        // List terminator
        NULL
    };
    const char** op;

    // Check all known trigonometric operators
    for (op = operators;  *op;  ++op)
        if (strcasecmp(str, *op) == 0)  return strlen(*op);

    // Match failed
    return 0;
}


/// @brief Checks for end-of-line and return its length
static int
is_brl_eol(const char* brl, size_t len)
{
    // UNIX-style end-of-line
    if (starts_with(brl, len, "\n"))  return 1;

    // DOS-style end-of-line
    if (starts_with(brl, len, "\r\n"))  return 2;

    // Nothing matched
    return 0;
}


/// @brief Checks for mathematical hyphen and return its length
static int
is_brl_math_hyphen(const char* brl, size_t len)
{
    // Dot 5 before end-of-line is mathematical hyphen
    if (*brl == '"')
    {
        size_t used = is_brl_eol(brl + 1, len - 1);
        if (used)  return 1 + used;
    }

    // Nothing matched
    return 0;
}


/** @brief Checks for un-shifted numerical braille.
    @return     Non-zero only if the braille is an un-shifted digit.
    @param c    Braille character to check.
  */
static int
is_brl_normal_digit(char c)
{
    if (!c)  return 0;
    return (strchr("abcdefghij", c) != NULL);
}


/** @brief Checks for lowered numerical braille.
    @return     Non-zero only if the braille is a lowered digit.
    @param c    Braille character to check.
  */
static int
is_brl_lowered_digit(char c)
{
    if (!c)  return 0;
    return (strchr("0123456789", c) != NULL);
}


/** @brief Checks whether braille is a Latin letter.
    @return     Non-zero only if the braille is a Latin letter.
    @param c    Braille character to check.
  */
static int
is_brl_latin(char c)
{
    return ((c >= 'a') && (c <= 'z'));
}


/** @brief Checks whether braille is a Greek letter.
    @return     Non-zero only if the braille is a Greek letter.
    @param c    Braille character to check.
  */
static int
is_brl_greek(char c)
{
    if (!c)  return 0;
    return (strchr("abgdez:?iklmnxoprstuf&yw", c) != NULL);
}


/** @brief Converts braille to ASCII digit.
    @return     The ASCII digit corresponding to the braille digit.
    @param c    Braille digit to convert.
  */
static char
brl_to_digit(char c)
{
    if ((c >= '0') && (c <= '9'))
        return c;
    else if ((c >= 'a') && (c <= 'i'))
        return c - 'a' + '1';
    else if (c == 'j')
        return '0';
    else
        return '\0';
}


/** @brief Converts braille to small Greek letter.
    @return     The small Greek letter in UTF-8.
    @param c    Braille Greek letter to convert.
  */
static const char*
brl_to_greek_small(char c)
{
    switch (c)
    {
        case 'a':  return "α";  // U+03b1
        case 'b':  return "β";  // U+03b2
        case 'g':  return "γ";  // U+03b3
        case 'd':  return "δ";  // U+03b4
        case 'e':  return "ε";  // U+03b5
        case 'z':  return "ζ";  // U+03b6
        case ':':  return "η";  // U+03b7
        case '?':  return "θ";  // U+03b8
        case 'i':  return "ι";  // U+03b9
        case 'k':  return "κ";  // U+03ba
        case 'l':  return "λ";  // U+03bb
        case 'm':  return "μ";  // U+03bc
        case 'n':  return "ν";  // U+03bd
        case 'x':  return "ξ";  // U+03be
        case 'o':  return "ο";  // U+03bf
        case 'p':  return "π";  // U+03c0
        case 'r':  return "ρ";  // U+03c1
        case 's':  return "σ";  // U+03c3
        case 't':  return "τ";  // U+03c4
        case 'u':  return "υ";  // U+03c5
        case 'f':  return "ϕ";  // U+03d5
        case '&':  return "χ";  // U+03c7
        case 'y':  return "ψ";  // U+03c8
        case 'w':  return "ω";  // U+03c9
        default:   return "";
    }
}


/** @brief Converts braille to capital Greek letter
    @return     The capital Greek letter in UTF-8.
    @param c    Braille Greek letter to convert.
  */
static const char*
brl_to_greek_capital(char c)
{
    switch (c)
    {
        case 'a':  return "Α";  // U+0391
        case 'b':  return "Β";  // U+0392
        case 'g':  return "Γ";  // U+0393
        case 'd':  return "Δ";  // U+0394
        case 'e':  return "Ε";  // U+0395
        case 'z':  return "Ζ";  // U+0396
        case ':':  return "Η";  // U+0397
        case '?':  return "Θ";  // U+0398
        case 'i':  return "Ι";  // U+0399
        case 'k':  return "Κ";  // U+039a
        case 'l':  return "Λ";  // U+039b
        case 'm':  return "Μ";  // U+039c
        case 'n':  return "Ν";  // U+039d
        case 'x':  return "Ξ";  // U+039e
        case 'o':  return "Ο";  // U+039f
        case 'p':  return "Π";  // U+03a0
        case 'r':  return "Ρ";  // U+03a1
        case 's':  return "Σ";  // U+03a3
        case 't':  return "Τ";  // U+03a4
        case 'u':  return "Υ";  // U+03a5
        case 'f':  return "Φ";  // U+03a6
        case '&':  return "Χ";  // U+03a7
        case 'y':  return "Ψ";  // U+03a8
        case 'w':  return "Ω";  // U+03a9
        default:   return "";
    }
}


static int
starts_with_brl_latin(const char* brl, size_t len)
{
    if (!len)  return 0;
    return is_brl_latin(*brl);
}


static int
starts_with_brl_greek(const char* brl, size_t len)
{
    if (!len)  return 0;
    return is_brl_greek(*brl);
}


/// @brief Parses lowered digits
static int
parse_lowered_digits(mxml_node_t* x, const char* brl, size_t len)
{
    StrBuf* buf = create_buffer();
    size_t  org_len = len;

    while (len)
    {
        size_t used = 0;
        char   c = *brl;

        // Make sure next braille character is a lowered digit
        if (is_brl_lowered_digit(c))
        {
            append_char(buf, brl_to_digit(c));
            used = 1;
        }
        else if (starts_with(brl, len, ".'"))
        {
            // Dot 46-3 is the dot used in statistics
            append_text(buf, "⋅");
            used = 2;
        }

        // Stop parsing when there is no valid braille
        if (!used)  break;

        // Parse next braille segment
        brl += used;
        len -= used;
    }

    // Create <mn> node
    if (org_len > len)  mxmlNewText(mxmlNewElement(x, "mn"), 0, buf->mpStr);
    destroy_buffer(buf);

    // Report number of bytes used
    return (org_len - len);
}


/// @brief Parses a number
static int
parse_number(mxml_node_t* x, const char* brl, size_t len)
{
    mxml_node_t* mn;
    StrBuf*      buf;
    size_t       org_len;
    int          recur_pos = -1;
    int          recur_count = 0;
    int          bar_pos = -1;

    // Dot 3456 starts number
    if (*brl != '#')  return 0;
    ++brl;
    --len;

    // Dot 36-36 is question mark
    if (starts_with(brl, len, "--"))  return 1;

    // Make sure first braille following numeral sign is valid
    if (len < 1)  return 0;
    if (!strchr("abcdefghij^1", *brl))  return 0;

    // Initialize state variables
    mn      = mxmlNewElement(x, "mn");
    buf     = create_buffer();
    org_len = len;

    // Parse digits
    while (len)
    {
        size_t used = 0;
        char   c = *brl;
        char   c1 = ((len > 1) ? brl[1] : '\0');
        char   c2 = ((len > 2) ? brl[2] : '\0');

        if (is_brl_normal_digit(c))
        {
            append_char(buf, brl_to_digit(c));
            if (recur_pos >= 0)  ++recur_count;
            used = 1;
        }
        else if ((c == '0'))
        {
            // Dot 356 is degree symbol
            break;
        }
        else if ((c == '1') && is_brl_normal_digit(c1))
        {
            // Dot 2 followed by digit is decimal point
            append_char(buf, '.');
            used = 1;
        }
        else if (c == '\'')
        {
            // Dot 3 is comma
            append_char(buf, ',');
            used = 1;
        }
        else if ((c == '"') && is_brl_normal_digit(c1))
        {
            // Dot 5 followed by digit starts recurring sequence
            recur_pos = strlen(buf->mpStr) + 1;
            used = 1;
        }
        else if (starts_with(brl, len, "1\"") && is_brl_normal_digit(c2))
        {
            // Dot 2-5 followed by digit is decimal point before recurring
            // sequence
            append_char(buf, '.');
            recur_pos = strlen(buf->mpStr) + 1;
            used = 2;
        }
        else if ((c == '^') && (len == org_len) && is_brl_normal_digit(c1))
        {
            // Dot 45 followed by digit is negative number with bar above
            bar_pos = strlen(buf->mpStr) + 1;
            used = 1;
        }
        else if (is_brl_lowered_digit(c))
        {
            // Lowered digits indicate simple fraction, wrap <mn> in <mfrac>
            mxml_node_t* mfrac = mxmlNewElement(x, "mfrac");
            mxmlAdd(mfrac, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mn);

            // Terminate recurring sequence in numerator
            if (recur_count > 1)  append_text(buf, UTF8_COMBINING_OVERDOT);
            recur_count = 0;

            // Parse lowered digits
            used = parse_lowered_digits(mfrac, brl, len); 
            brl += used;
            len -= used;

            // Check for superscript degree
            if (starts_with(brl, len, "+0"))
            {
                ++brl;
                --len;
            }

            break;
        }
        else
        {
            // Attempt to skip over mathematical hyphen
            used = is_brl_math_hyphen(brl, len);

            // We've reached end of number
            if (!used)  break;
        }

        // Append combining overdot for recurring sequence 
        if (strlen(buf->mpStr) == recur_pos)
            append_text(buf, UTF8_COMBINING_OVERDOT);

        // Append combining overline for negative number
        if (strlen(buf->mpStr) == bar_pos)
            append_text(buf, UTF8_COMBINING_OVERLINE);

        // Parse next braille segment
        brl += used;
        len -= used;
    }

    // Append final combining overdot for recurring sequence 
    if (recur_count > 1)  append_text(buf, UTF8_COMBINING_OVERDOT);

    // Update <mn> node
    mxmlNewText(mn, 0, buf->mpStr);
    destroy_buffer(buf);

    // Delete node if parsing has failed
    if (org_len == len)
    {
        mxmlDelete(mn);
        return 0;
    }

    // Parse degree, minute, second and radian sign
    if (starts_with(brl, len, "0"))
    {
        // Dot 356 is degree sign
        new_unit_element(x, "°");
        --len;
    }
    else if (starts_with(brl, len, ".") &&
            !starts_with_brl_greek(brl + 1, len - 1))
    {
        // Dot 46 is minute sign
        new_unit_element(x, "′");
        --len;
    }
    else if (starts_with(brl, len, "_") &&
            !starts_with(brl + 1, len - 1, "/") &&
            !starts_with_brl_greek(brl + 1, len - 1))
    {
        // Dot 456 is second sign
        new_unit_element(x, "″");
        --len;
    } 
    else if (starts_with(brl, len, "-") &&
            !starts_with(brl + 1, len - 1, "-"))
    {
        // Dot 36 is radian sign
        new_unit_element(x, "㎭");
        --len;
    }

    // Report number of bytes used
    return 1 + (org_len - len);
}


/// @brief Merges <msub> and <msup> into <msubsup>
static int
merge_msub_msup(mxml_node_t* x0)
{
    mxml_node_t* x1 = mxmlGetFirstChild(x0);
    const char*  name0 = mxmlGetElement(x0);
    const char*  name1 = mxmlGetElement(x1);
    if ((strcmp(name0, "msub") == 0) && (strcmp(name1, "msup") == 0))
    {
        mxmlSetElement(x0, "msubsup");
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x0));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlDelete(mxmlGetLastChild(x0));
    }
    else if ((strcmp(name0, "msup") == 0) && (strcmp(name1, "msub") == 0))
    {
        mxmlSetElement(x0, "msubsup");
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x0));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlDelete(mxmlGetLastChild(x0));
    }
}


/// @brief Merges <munder> and <mover> into <munderover>
static int
merge_munder_mover(mxml_node_t* x0)
{
    mxml_node_t* x1 = mxmlGetFirstChild(x0);
    const char*  name0 = mxmlGetElement(x0);
    const char*  name1 = mxmlGetElement(x1);
    if ((strcmp(name0, "munder") == 0) && (strcmp(name1, "mover") == 0))
    {
        mxmlSetElement(x0, "munderover");
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x0));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlDelete(mxmlGetLastChild(x0));
    }
    else if ((strcmp(name0, "mover") == 0) && (strcmp(name1, "munder") == 0))
    {
        mxmlSetElement(x0, "munderover");
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x0));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlAdd(x0, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT,
                mxmlGetLastChild(x1));
        mxmlDelete(mxmlGetLastChild(x0));
    }
}


/// @brief Obtains base expression for indices, creating one if necessary
static mxml_node_t*
get_base_expr(mxml_node_t* x)
{
    mxml_node_t* expr = mxmlGetLastChild(x);
    if (!expr)  expr = mxmlNewElement(x, "LEFT_INDEX");
    return expr;
}


/// @brief Obtains base expression for vector
static mxml_node_t*
get_vector_expr(mxml_node_t* x)
{
    mxml_node_t* last = mxmlGetLastChild(x);
    mxml_node_t* first = NULL;
    if (last)
    {
        first = mxmlGetPrevSibling(last);
        if (!first || !is_identifier(first, NULL))  first = last;
    }
    if (first && last && first != last)
        last = group_siblings("mrow", first, last);
    return last;
}


/// @brief Parses one or more primes
static int
parse_primes(mxml_node_t* x, const char* brl, size_t len)
{
    size_t org_len = len;

    // Dot 4-35 starts prime
    if (starts_with(brl, len, "@9"))
    {
        mxml_node_t* mo;
        StrBuf*      buf = create_buffer();

        // Search for end of consecutive primes
        append_text(buf, "′");
        for (brl += 2, len -= 2;  len > 0;  ++brl, --len)
        {
            if (*brl != '9')  break;
            append_text(buf, "′");
        }

        // Create <mo> node
        mo = mxmlNewElement(x, "mo");
        if (strcmp(buf->mpStr, "′′′") == 0)
            mxmlNewText(mo, 0, "‴");
        else if (strcmp(buf->mpStr, "′′") == 0)
            mxmlNewText(mo, 0, "″");
        else
            mxmlNewText(mo, 0, buf->mpStr);
        destroy_buffer(buf);
    }

    // Return number of bytes used
    return org_len - len;
}


/// @brief Parses one or more asterisks
static int
parse_asterisks(mxml_node_t* x, const char* brl, size_t len)
{
    size_t org_len = len;

    // Dot 4-26 starts superscript asterisk
    if (starts_with(brl, len, "@5"))
    {
        StrBuf*      buf = create_buffer();
        mxml_node_t* base = mxmlGetLastChild(x);
        mxml_node_t* mo;

        // Search for end of consecutive asterisks
        append_text(buf, "∗");
        for (brl += 2, len -= 2;  len > 0;  ++brl, --len)
        {
            if (*brl != '5')  break;
            append_text(buf, "∗");
        }

        // Create <mo> node
        if (base)
            apply_operator(base, "msup", buf->mpStr);
        else
            mxmlNewText(mxmlNewElement(x, "mo"), 0, buf->mpStr);
        destroy_buffer(buf);
    }

    // Return number of bytes used
    return org_len - len;
}


/// @brief Parses overhead symbol
static int
parse_overhead_symbol(mxml_node_t* x, const char* brl, size_t len)
{
    const char* overheads[] =
    {
        ":",    "¯",    // Dot 156
        "@:",   "ˆ",    // Dot 4-156
        "@>",   "ˇ",    // Dot 4-345
        "^:",   "~",    // Dot 45-156
        "-'",   "···",  // Dot 36-3
        "-",    "··",   // Dot 36
        "'",    "·",    // Dot 3

        // List terminator
        NULL
    };
    const char** oh;
    size_t       org_len = len;

    // Dot 36-36 is question mark
    if (starts_with(brl, len, "--"))  return 0;

    // Dot 3-3-3 is ellipsis
    if (starts_with(brl, len, "'''"))  return 0;

    // Check all known overhead symbols 
    for (oh = overheads;  *oh;  oh += 2)
    {
        // Look for symbol
        if (starts_with(brl, len, oh[0]))
        {
            mxml_node_t* base = mxmlGetLastChild(x);
            if (base)
                apply_operator(base, "mover", oh[1]);
            else
                mxmlNewText(mxmlNewElement(x, "mo"), 0, oh[1]);

            // Report number of bytes used
            return strlen(oh[0]);
        }
    }

    // Nothing matched
    return 0;
}


/// @brief Parses numeric index
static int
parse_numeric_index(mxml_node_t* x, const char* brl, size_t len,
        const char* name)
{
    char c = *brl;
    char c1 = ((len > 1) ? brl[1] : '\0');
    char c2 = ((len > 2) ? brl[2] : '\0');

    // Lowered digit starts numerical subscript
    if (is_brl_lowered_digit(c))
    {
        mxml_node_t* expr = get_base_expr(x);
        size_t       used = parse_lowered_digits(x, brl, len);
        mxml_node_t* script = mxmlGetLastChild(x);
        group_siblings(name, expr, script);
        return used;
    }

    // Negative sign followed by lowered digit starts numerical subscript
    if (starts_with(brl, len, ";-") && is_brl_lowered_digit(c2))
    {
        mxml_node_t* expr = get_base_expr(x);
        mxml_node_t* minus = mxmlNewElement(x, "mo");
        size_t       used = 2 + parse_lowered_digits(x, brl + 2, len - 2);
        mxml_node_t* script = mxmlGetLastChild(x);
        mxmlNewText(minus, 0, "−");
        script = group_siblings("mrow", minus, script);
        group_siblings(name, expr, script);
        return used;
    }

    // Nothing matched
    return 0;
}


/// @brief Parses index expression
static int
parse_index_expr(mxml_node_t* x, const char* brl, size_t len,
        const char* name, const char* closeBrl)
{
    mxml_node_t* mrow = mxmlNewElement(MXML_NO_PARENT, "mrow");
    size_t       used = parse_expr(mrow, brl, len, INDEX_EXPR, closeBrl);
    if (!mxmlGetFirstChild(mrow))  mxmlDelete(mrow);
    else
    {
        mxml_node_t* expr = get_base_expr(x);
        mxmlAdd(x, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mrow);
        mrow = simplify_single_child_mrow(mrow);
        remove_round_bracket(mrow);
        group_siblings(name, expr, mrow);
    }
    return used;
}


/// @brief Checks whether a string is an arrow
static int
is_arrow(const char* str)
{
    const char* arrows[] =
    {
        // Arrows
        "→", "←", "⟶", "⟵",
        "↦",
        "↣", "↢", "↠", "↞",
        "⇒", "⇐", "↔", "⇔",
        "⇀", "↼", "⇁",
        "⇌", "⥄", "⥂",
        "↑", "↓",

        // Overline, overbar, overdash, etc
        "¯", "ˆ", "ˇ", "~",
        "···", "··", "·",

        // List terminator
        NULL
    };
    const char** arr;

    // Check all known arrows
    for (arr = arrows;  *arr;  ++arr)
        if (strcmp(str, *arr) == 0)  return strlen(*arr);

    // Match failed
    return 0;
}


/// @brief Converts arrows in index into overscript or underscript
static void
fix_arrows_in_index(mxml_node_t* x)
{
    // Ignore outer <mstyle>
    const char* name = mxmlGetElement(x);
    while (strcmp(name, "mstyle") == 0)
    {
        x = mxmlGetFirstChild(x);
        name = mxmlGetElement(x);
    }

    // Fix only applicable to <msub> and <msup>
    if ((strcmp(name, "msub") != 0) &&
            (strcmp(name, "msup") != 0))
        return;

    // Change to <munder> or <mover> as appropriate
    if (is_arrow(get_element_text(mxmlGetLastChild(x))))
    {
        if (strcmp(name, "msub") == 0)
            mxmlSetElement(x, "munder");
        else if (strcmp(name, "msup") == 0)
            mxmlSetElement(x, "mover");
        remove_round_bracket(mxmlGetFirstChild(x));
    }
}


/// @brief Converts indices of an arrow into overscript or underscript
static void
fix_indices_of_arrow(mxml_node_t* x)
{
    // Ignore outer <mstyle>
    const char* name = mxmlGetElement(x);
    while (strcmp(name, "mstyle") == 0)
    {
        x = mxmlGetFirstChild(x);
        name = mxmlGetElement(x);
    }

    // Fix only applicable to <msub> and <msup>
    if ((strcmp(name, "msub") != 0) &&
            (strcmp(name, "msup") != 0))
        return;

    // Change to <munder> or <mover> as appropriate
    if (is_arrow(get_element_text(mxmlGetFirstChild(x))))
    {
        if (strcmp(name, "msub") == 0)
            mxmlSetElement(x, "munder");
        else if (strcmp(name, "msup") == 0)
            mxmlSetElement(x, "mover");
        remove_round_bracket(mxmlGetLastChild(x));
    }
}


/// @brief Parses superscript
static int
parse_superscript(mxml_node_t* x, const char* brl, size_t len,
        const char* closeBrl)
{
    char   c = *brl;
    size_t used = 0;

    // Try to parse superscript asterisk
    used = parse_asterisks(x, brl, len);

    // Dot 346 starts superscript
    if (!used && (c == '+'))
    {
        used = parse_numeric_index(x, brl + 1, len + 1, "msup");
        if (!used)
            used = parse_index_expr(x, brl + 1, len - 1, "msup", closeBrl);
        if (used)
            ++used;
    }

    // Perform postprocessing
    if (used)
    {
        x = mxmlGetLastChild(x);

        // Adjust position of arrow in index
        fix_arrows_in_index(x);

        // Adjust position of index belonging to an arrow
        fix_indices_of_arrow(x);

        // Merge subscript and superscript into <msubsup>
        merge_msub_msup(x);
    }

    // Nothing matched
    return used;
}


/// @brief Parses subscript
static int
parse_subscript(mxml_node_t* x, const char* brl, size_t len,
        const char* closeBrl, char prevChar)
{
    char   c = *brl;
    size_t used = 0;

    // Dot 16 starts subscript
    if (c == '*')
    {
        used = parse_numeric_index(x, brl + 1, len + 1, "msub");
        if (!used)
            used = parse_index_expr(x, brl + 1, len - 1, "msub", closeBrl);
        if (used)
            ++used;
    }

    // Try to parse numeric subscript
    if (!used && prevChar)
    {
        mxml_node_t* base = bypass_style_and_indices(mxmlGetLastChild(x));
        if (base)
        {
            const char* text = get_element_text(base);
            if (!is_operator(base, NULL) ||
                    (strcmp(text, "∫") == 0) ||
                    (strcmp(text, "∮") == 0) ||
                    (strcmp(text, "∑") == 0) ||
                    (strcmp(text, "∏") == 0) ||
                    (strcmp(text, "∐") == 0) ||
                    is_latin_word(text))
            {
                if (strcmp(text, "…") != 0)
                    used = parse_numeric_index(x, brl, len, "msub");
            }
        }
    }

    // Perform postprocessing
    if (used)
    {
        x = mxmlGetLastChild(x);

        // Adjust position of arrow in index
        fix_arrows_in_index(x);

        // Adjust position of index belonging to an arrow
        fix_indices_of_arrow(x);

        // Merge subscript and superscript into <msubsup>
        merge_msub_msup(x);
    }

    // Nothing matched
    return used;
}


/// @brief Parses overscript
static int
parse_overscript(mxml_node_t* x, const char* brl, size_t len,
        const char* closeBrl)
{
    char   c = *brl;
    size_t used;

    used = parse_overhead_symbol(x, brl, len);
    if (used)  return used;

    // Dot 46-25-135 is overhead right arrow
    if (starts_with(brl, len, ".3o"))
    {
        mxml_node_t* expr = get_vector_expr(x);
        if (expr)
        {
            apply_operator(expr, "mover", "→");
            return 3;
        }
    }

    // Dot 46-246-25 is overhead right arrow
    if (starts_with(brl, len, ".[3"))
    {
        mxml_node_t* expr = get_vector_expr(x);
        if (expr)
        {
            apply_operator(expr, "mover", "←");
            return 3;
        }
    }

    // Dot 4-346 starts overscript
    if (starts_with(brl, len, "@+"))
    {
        used = parse_numeric_index(x, brl + 2, len + 2, "mover");
        if (!used)
            used = parse_index_expr(x, brl + 2, len - 2, "mover", closeBrl);
        if (used)
            used += 2;
    }

    // Merge underscript and overscript into <munderover>
    if (used)  merge_munder_mover(mxmlGetLastChild(x));

    // Nothing matched
    return used;
}


/// @brief Parses underscript
static int
parse_underscript(mxml_node_t* x, const char* brl, size_t len,
        const char* closeBrl)
{
    char   c = *brl;
    size_t used = 0;

    // Dot 4-16 starts underscript
    if (starts_with(brl, len, "@*"))
    {
        used = parse_numeric_index(x, brl + 2, len + 2, "munder");
        if (!used)
            used = parse_index_expr(x, brl + 2, len - 2, "munder", closeBrl);
        if (used)
            used += 2;
    }

    // Merge underscript and overscript into <munderover>
    if (used)  merge_munder_mover(mxmlGetLastChild(x));

    // Nothing matched
    return used;
}


/// @brief Parses root
static int
parse_root(mxml_node_t* x, const char* brl, size_t len,
        const char* closeBrl)
{
    char   c = *brl;
    size_t used;

    // Dot 145 starts root
    if (len < 2)  return 0;
    if (*brl != '%')  return 0;

    // Add a dummy term to own subscript
    mxmlNewElement(x, "ROOT_INDEX");

    // Dot 16 starts subscript
    if (brl[1] == '*')
    {
        used = parse_numeric_index(x, brl + 2, len - 2, "mroot");
        if (!used)
            used = parse_index_expr(x, brl + 2, len - 2, "mroot", closeBrl);
        if (used)
            return 2 + used;
    }

    // Try to parse numeric subscript
    used = parse_numeric_index(x, brl + 1, len - 1, "mroot");
    if (used)  return 1 + used;

    // If there is no subscript, it is a square root
    x = mxmlGetLastChild(x);
    group_siblings("msqrt", x, x);
    return 1;
}


/// @brief Parses consecutive operators starting with dot 56
static int
parse_dot_56_operators(mxml_node_t* x, const char* brl, size_t len)
{
    const char* operators[] =
    {
        "8",   "×",
        "4",   "÷",
        "6-",  "±",
        "-6",  "∓",
        "7",   "=",
        ";7",  "≜",
        "6",   "+",
        "-",   "−",

        // List terminator
        NULL
    };
    const char** op;
    size_t       org_len = len;
    int          count = 0;
    int          is_bold = 0;

    while (len > 0)
    {
        mxml_node_t* mo = NULL;
        size_t       used = 0;

        // Check for dot 56 before first sign
        if (count == 0)
        {
            if (starts_with(brl, len, ";@"))
            {
                is_bold = 1;
                used    = 2;
            }
            else if (starts_with(brl, len, ";"))
            {
                used = 1;
            }
            else break;
        }

        // Dot 36-36 is question mark
        if (starts_with(brl + used, len - used, "--"))  break;

        // Check for all known operators
        for (op = operators;  *op;  op += 2)
        {
            int op_len = strlen(op[0]);

            // Look for operator
            if (!starts_with(brl + used, len - used, op[0]))  continue;

            // Perform special check for question mark following ±
            if ((op_len > 1) && (op[0][1] == '-') &&
                    starts_with(brl + used + 1, len - used + 1, "--"))
                continue;

            // Create <mo> node
            mo = mxmlNewElement(x, "mo");
            mxmlNewText(mo, 0, op[1]);

            // Create outer <mstyle> for bold symbol
            if (is_bold)
            {
                mxml_node_t* mstyle = group_siblings("mstyle", mo, mo);
                mxmlElementSetAttr(mstyle, "mathvariant", "bold");
            }

            // Remember number of bytes used
            used += op_len;
            break;
        }

        // Stop when none of the operators matches
        if (!mo)  break;

        // Parse next braille segment
        brl += used;
        len -= used;
        ++count;
    }

    // Report number of bytes used
    return (org_len - len);
}


static int
is_part_of_word(char c)
{
    return (is_brl_latin(c) || strchr("-0", c));
}


/// @brief Parses word operator
static int
parse_word_operator(mxml_node_t* x, const char* brl, size_t len)
{
    const char* abbreviations[] =
    {
        // Trigonometric functions
        "s",    "sin",
        "@s",   "arcsin",
        "c",    "cos",
        "@c",   "arccos",
        "t",    "tan",
        "@t",   "arctan",
        "-",    "sec",
        "@-",   "arcsec",
        "<",    "cosec",
        "@<",   "arccosec",
        "\\",   "cot",
        "@\\",  "arccot",
        "hs",   "sinh",
        "8s",   "arcsinh",
        "hc",   "cosh",
        "8c",   "arccosh",
        "ht",   "tanh",
        "8t",   "arctanh",
        "h-",   "sech",
        "8-",   "arcsech",
        "h<",   "cosech",
        "8<",   "arccosech",
        "h\\",  "coth",
        "8\\",  "arccoth",

        // Other functions
        "l",    "log",
        "@l",   "antilog",
        "^l",   "colog",
        "g",    "grad",
        "%",    "curl",
        "?",    "div",

        // Symbols
        "3",    "○",
        "d",    "△",
        "q",    "□",

        // List terminator
        NULL
    };
    const char** ab;
    size_t       org_len = len;
    StrBuf*      buf;

    // Dot 1246 starts word operator
    if (brl[0] != '$')  return 0;
    ++brl;
    --len;

    // Create buffer to hold words
    buf = create_buffer();

    // Look for one or more space delimited words
    while (len > 0)
    {
        // Empty buffer
        buf->mpStr[0] = '\0';

        // Check all known abbreviations 
        for (ab = abbreviations;  *ab;  ab += 2)
        {
            const char* abbrev = ab[0];
            const char* full = ab[1];
            int ab_len = strlen(abbrev);

            // Check for un-modified shorthand
            if (starts_with(brl, len, abbrev) &&
                    ((len == ab_len) || !is_part_of_word(brl[ab_len])))
            {
                append_text(buf, full);
                brl += ab_len;
                len -= ab_len;
                break;
            }

            // Check for capitalized shorthand
            ++ab_len;
            if (!is_brl_latin(full[0]))  continue;
            if (len < ab_len)  continue;
            if (strchr("@8^", abbrev[0]))
            {
                // Dot 6 comes after non-latin abbreviation
                if (brl[0] != abbrev[0])  continue;
                if (brl[1] != ',')  continue;
            }
            else
            {
                if (brl[0] != ',')  continue;
                if (brl[1] != abbrev[0])  continue;
            }
            if ((ab_len > 2) && !starts_with(brl + 2, len - 2, abbrev + 1))
                continue;
            if ((len > ab_len) && is_part_of_word(brl[ab_len]))  continue;
            append_char(buf, full[0] - 'a' + 'A');
            append_text(buf, full + 1);
            brl += ab_len;
            len -= ab_len;
        }

        // Not an abbreviation, so parse space delimited word
        if (!buf->mpStr[0])
        {
            int cap = 0;

            // Dot 6 means initial capitalization
            if (*brl == ',')
            {
                cap = 1;
                ++brl;
                --len;
            }

            // Look for end of word
            for (;  len > 0;  ++brl, --len)
            {
                char c = *brl;
                if (is_brl_latin(c))
                {
                    // Capitalize initial letter if necessary
                    if (cap)
                    {
                        cap = 0;
                        c   = c - 'a' + 'A';
                    }
                }
                else if (c == '0')
                {
                    // Dot 256 is full stop
                    c = '.';
                }
                else if (!strchr("-", c))  break;
                append_char(buf, c);
            }
        }

        // Create <mo> node
        if (buf->mpStr[0])
        {
            mxml_node_t* mo;
            int          op_len = strlen(buf->mpStr);
            int          inv = 0;

            // Create <mo> node
            mo = mxmlNewElement(x, "mo");
            mxmlNewText(mo, 0, buf->mpStr);
        }

        // Stop when there is no more braille to parse
        if (len == 0)  break;

        // Stop when there is no trialing space
        if (*brl != ' ')  break;

        // Consume trailing spaces
        for (;  len > 0;  ++brl, --len)
            if (*brl != ' ')  break;
    }

    // Clean up
    destroy_buffer(buf);

    // Report number of bytes used
    return org_len - len;
}


/// @brief Parses unspaced operator
static int
parse_unspaced_operator(mxml_node_t* x, const char* brl, size_t len)
{
    const char* operators[] =
    {
        ";'",   "⋅",
        "_0",   "∇",
        "_[",   "∠",
        "!",    "∫",
        "@!",   "∮",
        "@&",   "&",
        "@d",   "∂",
        "=",    "∞",
        "_8",   "#",
        "_v",   "∐",
        "__",   "‖",
        "_",    "|",

        // List terminator
        NULL
    };
    const char** op;

    // Choose single bar if what follows is a valid sign
    if (starts_with(brl, len, "__") && (len > 2) && strchr("/0[8v", brl[2]))
    {
        mxmlNewText(mxmlNewElement(x, "mo"), 0, "|");
        return 1;
    }

    // Check all known signs 
    for (op = operators;  *op;  op += 2)
    {
        // Look for operator
        if (!starts_with(brl, len, op[0]))  continue;

        // Create <mo> node
        mxmlNewText(mxmlNewElement(x, "mo"), 0, op[1]);

        // Report number of bytes used
        return strlen(op[0]);
    }

    // Nothing matched
    return 0;
}


/// @brief Parses operator that requires leading space
static int
parse_pre_spaced_operator(mxml_node_t* x, const char* brl, size_t len)
{
    const char* operators[] =
    {
        // Prefixed by dot 45 
        "^7",   "⌅",
        "^8",   "∧",
        "^6",   "∨",

        // Prefixed by dot 1256 
        "\\=",  "∗",
        "\\7",  "∘",
        "\\0",  "∅",
        "\\8",  "∩",
        "\\6",  "∪",
        "\\*",  "∖",
        "\\[7", "⊆",
        "\\o7", "⊇",
        "\\[",  "⊂",
        "\\o",  "⊃",
        "\\9",  "∈",
        "\\)",  "∋",
        "\\1",  "∀",
        "\\5",  "∃",
        "\\+7", "⊴",
        "\\u7", "⊵",
        "\\+",  "⊲",
        "\\u",  "⊳",
        "\\\"[7","⊈",
        "\\\"[o","⊉",
        "\\\"[","⊄",
        "\\\"o","⊅",
        "\\\"9","∉",
        "\\\"5","∄",

        // Relation signs
        "[7",   "≤",
        "o7",   "≥",
        "77",   "≡",
        "_7",   "≈",
        ".7",   "≃",
        "-7",   "≅",
        "[_7",  "≲",
        "o_7",  "≳",
        "37",   "∝",
        "#'",   "⟂",
        "\"7",  "≠",
        "\"[7", "≰",
        "\"o7", "≱",
        "\"77", "≢",
        "\"_7", "≉",
        "\".7", "≄",
        "\"-7", "≇",
        "\"[_7","≴",
        "\"o_7","≵",
        "\"__", "∦",
        "[\"7", "≨",
        "o\"7", "≩",

        // Arrows
        "3o",   "→",
        "[3",   "←",
        "33o",  "⟶",
        "[33",  "⟵",
        "^3o",  "↦",
        "o3o",  "↣",
        "[3[",  "↢",
        "3oo",  "↠",
        "[[3",  "↞",
        "\\3o", "⇒",
        "\\[3", "⇐",
        "[3o",  "↔",
        "\\[3o","⇔",
        "3e",   "⇀",
        "33",   "↼",
        "39",   "⇁",
        "53e",  "⇌",
        "553e", "⥄",
        "53ee", "⥂",
        "3i",   "↑",
        "35",   "↓",
        "\"3o", "↛",
        "\"[3", "↚",
        
        // Other signs
        "3;7",  "≔",
        "7",    "(",

        // List terminator
        NULL
    };
    const char** op;
    int          is_bold = 0;

    // Check all known signs 
    for (op = operators;  *op;  op += 2)
    {
        mxml_node_t* mo;
        size_t       op_len = strlen(op[0]);
        char         init_c = op[0][0];

        if ((init_c == '3') && (*brl == '@'))
        {
            // Look for dot 4 before signs starting with dot 25
            if (!starts_with(brl + 1, len - 1, op[0]))  continue;
            ++op_len;
        }
        else if (init_c == '^' && starts_with(brl, len, "^@"))
        {
            // Bold form of dot 45 signs start with dot 45-4
            if (!starts_with(brl + 2, len - 2, op[0] + 1))  continue;
            ++op_len;
            is_bold = 1;
        }
        else if (init_c == '\\' && starts_with(brl, len, "\\@"))
        {
            // Bold form of dot 1256 signs start with dot 1256-4
            if (!starts_with(brl + 2, len - 2, op[0] + 1))  continue;
            ++op_len;
            is_bold = 1;
        }
        else
        {
            // Otherwise look for operator on its own
            if (!starts_with(brl, len, op[0]))  continue;
        }

        // Create <mo> node
        mo = mxmlNewElement(x, "mo");
        mxmlNewText(mo, 0, op[1]);

        // Create outer <mstyle> for bold symbol
        if (is_bold)
        {
            mxml_node_t* mstyle = group_siblings("mstyle", mo, mo);
            mxmlElementSetAttr(mstyle, "mathvariant", "bold");
        }

        // Report number of bytes used
        return op_len;
    }

    // Nothing matched
    return 0;
}


/// @brief Parses operator that requires leading and trailing spaces
static int
parse_pre_post_spaced_operator(mxml_node_t* x, const char* brl, size_t len,
        const char* closeBrl)
{
    const char* operators[] =
    {
        "[[",   "≪",
        "oo",   "≫",
        "[",    "<",
        "o",    ">",
        ",*",   "∴",
        "@/",   "∵",
        "_1",   "⊢",
        "\"l",  "⊣",
        "_3",   "⊨",
        "3l",   "⫤",
        "==",   "∎",
        "\"[",  "≮",
        "\"o",  "≯",
        "\"_1", "⊬",
        "\"_3", "⊭",

        // List terminator
        NULL
    };
    const char** op;

    // Check all known signs 
    for (op = operators;  *op;  op += 2)
    {
        size_t op_len;

        // Look for operator
        if (!starts_with(brl, len, op[0]))  continue;

        // Make sure it is followed by a space or a closing bracket
        op_len = strlen(op[0]);
        while (len > op_len)
        {
            char c = brl[op_len];

            // Look for trailing space
            if (c == ' ')
            {
                op_len += 1;
                break;
            }

            // Look for closing bracket
            if (starts_with(brl + op_len, len - op_len, closeBrl))
            {
                op_len += strlen(closeBrl);
                break;
            }

            // Match failed
            op_len = 0;
            break;
        }
        if (!op_len)  continue;

        // Create <mo> node
        mxmlNewText(mxmlNewElement(x, "mo"), 0, op[1]);

        // Report number of bytes used
        return op_len;
    }

    // Nothing matched
    return 0;
}


/// @brief Parses operator that requires optional leading space
static int
parse_optional_pre_spaced_operator(mxml_node_t* x, const char* brl, size_t len)
{
    const char* operators[] =
    {
        "\\-",  "¬",
        "\"-",  "∼",

        // List terminator
        NULL
    };
    const char** op;

    // Check all known signs 
    for (op = operators;  *op;  op += 2)
    {
        // Look for operator
        if (!starts_with(brl, len, op[0]))  continue;

        // Create <mo> node
        mxmlNewText(mxmlNewElement(x, "mo"), 0, op[1]);

        // Report number of bytes used
        return strlen(op[0]);
    }

    // Nothing matched
    return 0;


}


/// @brief Parses operators
static int
parse_operators(mxml_node_t* x, const char* brl, size_t len,
        int spc, const char* closeBrl, int* extra_spc)
{
    size_t org_len = len;
    int    count = 0;
    while (len > 0)
    {
        size_t used = 0;

        // Dot 456 followed by Greek braille is capital Greek
        if ((brl[0] == '_') && starts_with_brl_greek(brl + 1, len - 1))
            break;

        // Dot 456-456 followed by Greek braille is vertical bar and Greek
        if (starts_with(brl, len, "__") &&
                starts_with_brl_greek(brl + 2, len - 1))
        {
            mxmlNewText(mxmlNewElement(x, "mo"), 0, "|");
            --len;
            break;
        }

        // Dot 456-34 is fraction line
        if (starts_with(brl, len, "_/"))
        {
            mxmlNewElement(x, "FRACTION");
            used = 2;
        }

        // Dot 34-26 is continued fraction line
        if (starts_with(brl, len, "/5"))
        {
            mxmlNewElement(x, "CONT_FRAC");
            used = 2;
        }

        // Check for consecutive operators starting with dot 56
        if (!used && (spc || (count > 0)))
            used = parse_dot_56_operators(x, brl, len);

        // Check for operators with optional leading space
        if (!used && (spc || (count > 0)))
            used = parse_optional_pre_spaced_operator(x, brl, len);

        // Check for operators that require leading and trailing spaces
        if (!used && spc)
            used = parse_pre_post_spaced_operator(x, brl, len, closeBrl);

        // Check for operators that require leading space
        if (!used && spc)
            used = parse_pre_spaced_operator(x, brl, len);

        // Check for primes
        if (!used)  used = parse_primes(x, brl, len);

        // Check for unspaced operators
        if (!used)
        {
            used = parse_unspaced_operator(x, brl, len);
            if (used && !count)  *extra_spc = spc;
        }

        // Check for word operators
        if (!used)
        {
            used = parse_word_operator(x, brl, len);
            if (used && !count)  *extra_spc = spc;
        }

        // Stop when none of the operators matches
        if (!used)  break;

        // Determine whether next match has leading space
        spc = (brl[used - 1] == ' ');
        ++count;

        // Parse next braille segment
        brl += used;
        len -= used;
    }

    // Report number of bytes used
    return (org_len - len);
}


/// @brief Parses a letter
static int
parse_letter(mxml_node_t* x, const char* brl, size_t len,
        int fount)
{
    const char* punctuations[] =
    {
        // Escaped punctuations
        ",7",   ")",
        ",6",   "!",
        ",4",   ".",
        ",3",   ":",
        ",2",   ";",
        ",1",   ",",

        // Unescaped punctuations
        "4",    ".",
        "3",    ":",
        "2",    ";",
        "1",    ",",

        // List terminator
        NULL
    };

    const char* identifiers[] =
    {
        "'''",  "…",
        "--",   "?",

        // List terminator
        NULL
    };

    const char** pct;
    const char** id;
    mxml_node_t* mi;
    int          is_bold = 0;
    const char*  text = NULL;
    char         buf[2] = {*brl, 0};
    char         c = *brl;

    // Check all known punctuations 
    for (pct = punctuations;  *pct;  pct += 2)
    {
        // Look for punctuation mark
        if (!starts_with(brl, len, pct[0]))  continue;

        // Create <mo> node
        mxmlNewText(mxmlNewElement(x, "mo"), 0, pct[1]);

        // Report number of bytes used
        return strlen(pct[0]);
    }

    // Check all known identifiers 
    for (id = identifiers;  *id;  id += 2)
    {
        // Look for special identifier
        if (!starts_with(brl, len, id[0]))  continue;

        // Create <mo> node
        mxmlNewText(mxmlNewElement(x, "mi"), 0, id[1]);

        // Report number of bytes used
        return strlen(id[0]);
    }

    // Convert to a letter under active fount
    switch (fount)
    {
        case FOUNT_BOLD_GREEK_CAPITAL:
            is_bold = 1;  // Fall through
        case FOUNT_GREEK_CAPITAL:
            if (!is_brl_greek(c))  return 0;
            text = brl_to_greek_capital(c);
            break;

        case FOUNT_BOLD_GREEK_SMALL:
            is_bold = 1;  // Fall through
        case FOUNT_GREEK_SMALL:
            if (!is_brl_greek(c))  return 0;
            text = brl_to_greek_small(c);
            break;

        case FOUNT_BOLD_LATIN_CAPITAL:
            is_bold = 1;  // Fall through
        case FOUNT_LATIN_CAPITAL:
            if (c < ' ')  return 0;
            if (is_brl_latin(c))  buf[0] = c - 'a' + 'A';
            text = buf;
            break;

        case FOUNT_BOLD_LATIN_SMALL:
            is_bold = 1;  // Fall through
        case FOUNT_LATIN_SMALL:
        default:
            if (c <= ' ')  return 0;
            text = buf;
            break;
    }

    // Create <mi> node
    mi = mxmlNewElement(x, "mi");
    mxmlNewText(mi, 0, text);

    // Create outer <mstyle> for bold letter
    if (is_bold)
    {
        mxml_node_t* mstyle = group_siblings("mstyle", mi, mi);
        mxmlElementSetAttr(mstyle, "mathvariant", "bold");
    }

    return 1;
}


/// @brief Parses next letter in unit
static int
parse_next_unit_letter(StrBuf* buf, const char* brl, size_t len, int* fount)
{
    char c = *brl;

    // Dot 256 is full stop
    if (c == '4')
    {
        append_char(buf, '.');
        return 1;
    }

    // Dot 56 reset back to small Latin letter
    if (c == ';')
    {
        *fount == FOUNT_LATIN_SMALL;
        return 1;
    }

    // Use existing fount if prefix is omitted
    if (is_brl_latin(c))
    {
        if (*fount == FOUNT_LATIN_CAPITAL)  c = c - 'a' + 'A';
        append_char(buf, c);
        return 1;
    }

    // Capital Latin letter is prefixed by dot 6
    if ((c == ',') && starts_with_brl_latin(brl + 1, len - 1))
    {
        *fount = FOUNT_LATIN_SMALL;
        append_char(buf, brl[1] - 'a' + 'A');
        return 2;
    }

    // Small Greek letter is prefixed by dot 46
    if ((c == '.') && starts_with_brl_greek(brl + 1, len - 1))
    {
        *fount = FOUNT_LATIN_SMALL;
        append_text(buf, brl_to_greek_small(brl[1]));
        return 2;
    }

    // Capital Greek letter will be prefixed by dot 456
    if ((c == '_') && starts_with_brl_greek(brl + 1, len - 1))
    {
        *fount = FOUNT_LATIN_SMALL;
        append_text(buf, brl_to_greek_capital(brl[1]));
        return 2;
    }

    // Capital Latin sequence is prefixed by dot 6-6
    if (starts_with(brl, len, ",,") &&
            starts_with_brl_latin(brl + 2, len - 2))
    {
        *fount = FOUNT_LATIN_CAPITAL;
        append_char(buf, brl[2] - 'a' + 'A');
        return 3;
    }

    // No match found
    return 0;
}


/// @brief Parses next mathematical units
static int
parse_next_unit(mxml_node_t* x, const char* brl, size_t len, int count)
{
    const char* units[] =
    {
        "^a",   "Å",
        "3p",   "%",
        "-",    "㎭",

        // List terminator
        NULL
    };
    const char** unit;
    mxml_node_t* base;
    size_t       org_len = len;
    StrBuf*      buf = NULL;
    int          fount = FOUNT_LATIN_SMALL;
    int          used;

    // Check all known units 
    for (unit = units;  *unit;  unit += 2)
    {
        // Look for unit
        if (!starts_with(brl, len, unit[0]))  continue;

        // Create <mi> node
        new_unit_element(x, unit[1]);

        // Report number of bytes used
        return strlen(unit[0]);
    }

    // Perform check for first unit
    while (count == 0)
    {
        // Textual unit must have two or more letters, or escaped with dot 56
        if (starts_with_brl_latin(brl, len) &&
                starts_with_brl_latin(brl + 1, len - 1))
            break;
        if (starts_with_brl_latin(brl, len) &&
                starts_with(brl + 1, len - 1, "4"))
            break;
        if (starts_with(brl, len, ";") &&
                starts_with_brl_latin(brl + 1, len - 1))
            break;
        if (starts_with(brl, len, ",") &&
                starts_with_brl_latin(brl + 1, len - 1))
            break;
        if (starts_with(brl, len, ".") &&
                starts_with_brl_greek(brl + 1, len - 1))
            break;
        if (starts_with(brl, len, "_") &&
                starts_with_brl_greek(brl + 1, len - 1))
            break;
        if (starts_with_brl_latin(brl, len) &&
                starts_with(brl + 1, len - 1, ",") &&
                starts_with_brl_latin(brl + 2, len - 2))
            break;
        if (starts_with(brl, len, ";,") &&
                starts_with_brl_latin(brl + 2, len - 2))
            break;
        if (starts_with(brl, len, ",,") &&
                starts_with_brl_latin(brl + 2, len - 2))
            break;

        // Not a valid textual unit
        return 0;
    }

    // Look for end of text string
    buf = create_buffer();
    do
    {
        used = parse_next_unit_letter(buf, brl, len, &fount);
        brl += used;
        len -= used;
    }  while (used);

    // Create <mi> node
    new_unit_element(x, buf->mpStr);
    destroy_buffer(buf);

    // Dot 346 starts superscript
    if (starts_with(brl, len, "+"))
    {
        used = parse_numeric_index(x, brl + 1, len - 1, "msup");
        if (used)
        {
            ++used;
            brl += used;
            len -= used;
        }
    }

    // Report number of bytes used
    return org_len - len;
}


/// @brief Parses one or more mathematical units
static int
parse_units(mxml_node_t* x, const char* brl, size_t len)
{
    mxml_node_t* base;
    size_t       org_len = len;
    int          used = 0;
    int          count = 0;

    // Dot 123 followed by digit is pound sterling sign
    if (starts_with(brl, len, "l#") || starts_with(brl, len, "@l#") ||
            (starts_with(brl, len, "l;") &&
             starts_with_brl_latin(brl + 2, len - 2)))
    {
        new_unit_element(x, "£");
        return 1 + (strchr(brl, 'l') - brl);
    }

    // Dot 256 followed by digit is dollar sign
    if (starts_with(brl, len, "4#") || starts_with(brl, len, "@4#"))
    {
        new_unit_element(x, "$");
        return 1 + (strchr(brl, '4') - brl);
    }

    // Previous term should be a numerical value
    base = mxmlGetLastChild(x);
    if (!base)  return 0;
    while (mxmlGetElement(mxmlGetFirstChild(base)))
        base = mxmlGetFirstChild(base);
    if ((strcmp(mxmlGetElement(base), "mn") != 0) &&
            !is_identifier(base, "π") &&
            !is_identifier(base, "e"))
        return 0;

    // Dot 4-15 is euro sign
    if (starts_with(brl, len, "@e"))
    {
        new_unit_element(x, "€");
        return 2;
    }

    // Dot 4-14 is cent sign
    if (starts_with(brl, len, "@c"))
    {
        new_unit_element(x, "¢");
        return 2;
    }

    // Dot 36 is radian sign
    if (starts_with(brl, len, "-") &&
            !starts_with(brl + 1, len - 1, "-"))
    {
        new_unit_element(x, "㎭");
        return 1;
    }

    // Other units should start with a leading space
    if (*brl != ' ')  return 0;
    ++brl;
    --len;

    // Mathematical units must contain at least two characters
    if (*brl && ((brl[1] == '\0') || (brl[1] == ' ')))  return 0;

    // Parse consecutive units
    for (;  len > 0;  ++count)
    {
        used = parse_next_unit(x, brl, len, count);

        // Stop parsing when there is no more matches
        if (!used)  break;
        brl += used;
        len -= used;

        // Dot 3 is unit separation sign
        if (starts_with(brl, len, "'"))
        {
            ++brl;
            --len;
            continue;
        }

        // Dot 456-34 is stroke
        if (starts_with(brl, len, "_/"))
        {
            new_unit_element(x, "/");
            brl += 2;
            len -= 2;
            continue;
        }

        // Mathematical hyphen continues unit to next line
        used = is_brl_math_hyphen(brl, len);
        if (used)
        {
            brl += used;
            len -= used;
            continue;
        }

        // End of units, stop parsing
        break;
    }

    // Report number of bytes used
    used = org_len - len;
    if (used == 1)  used = 0;
    return used;
}


/** @brief Checks if a node can be concatenated to numerator or denominator.
    @return     Non-zero if the node can be concatenated.
  */
static int
is_extendable_node(mxml_node_t* x)
{
    const char* name = mxmlGetElement(x);

    // Ignore style and indices
    x = bypass_style_and_indices(x);
    if (!x)  return 0;
    name = mxmlGetElement(x);

    // Do actual check now
    if (strcmp(name, "mroot") == 0)
    {
        // Roots cannot be concatenated
        return 0;
    }
    else if (strcmp(name, "msqrt") == 0)
    {
        // Square roots cannot be concatenated
        return 0;
    }
    else if (strcmp(name, "mo") != 0)
    {
        // Nodes which are not operators can be concatenated
        return 1;
    }
    else
    {
        const char* mo = get_element_text(x);

        // Dot operator can be concatenated
        if (strcmp(mo, "⋅") == 0)  return 1;

        // Factorial operator can be concatenated
        if (strcmp(mo, "!") == 0)  return 1;

        // Partial differential operator can be concatenated
        if (strcmp(mo, "∂") == 0)  return 1;

        // Sum operator can be concatenated
        if (strcmp(mo, "∑") == 0)  return 1;

        // Product operator can be concatenated
        if (strcmp(mo, "∏") == 0)  return 1;

        // Trigonometric operators can be concatenated
        if (is_trigonometric_operator(mo))  return 1;

        // Worded operators can be concatenated
        if (is_latin_word(mo))  return 1;

        // All other operators cannot be concatenated
        return 0;
    }
}


/// @brief Extends numerator by concatenating preceeding nodes
static mxml_node_t*
extend_numerator(mxml_node_t* num)
{
    // Find all extendable nodes
    int          is_bar = is_operator(num, "|");
    mxml_node_t* last = num;
    mxml_node_t* x = mxmlGetPrevSibling(num);
    for (;  x;  num = x, x = mxmlGetPrevSibling(x))
    {
        if (is_bar)
        {
            // Look for matching modulus bar
            if (is_operator(x, "|"))
            {
                num = x;
                break;
            }
        }
        else
        {
            if (!is_extendable_node(x))  break;
        }
    }

    // Group multiple nodes into <mrow>
    if (num != last)  num = group_siblings("mrow", num, last);

    return num;
}


/// @brief Extends denominator by concatenating succeeding nodes
static mxml_node_t*
extend_denominator(mxml_node_t* denom)
{
    // Find all extendable nodes
    int          is_bar = is_operator(denom, "|");
    mxml_node_t* first = denom;
    mxml_node_t* x = mxmlGetNextSibling(denom);
    for (;  x;  denom = x, x = mxmlGetNextSibling(x))
    {
        if (is_bar)
        {
            // Look for matching modulus bar
            if (is_operator(x, "|"))
            {
                denom = x;
                break;
            }
        }
        else
        {
            if (!is_extendable_node(x))  break;
        }
    }

    // Group multiple nodes into <mrow>
    if (first != denom)  denom = group_siblings("mrow", first, denom);

    return denom;
}


/// @brief Checks whether a denominator is a product starting with d or ∂
static int
is_differential_denominator(mxml_node_t* x)
{
    int is_d = 0;
    int is_del = 0; 

    // Elements should be inside an <mrow>
    mxml_node_t* elem = mxmlGetFirstChild(x);
    if (!elem)  return 0;
    if (!mxmlGetElement(elem))  return 0;

    // First term should be "d" or "∂"
    if      (is_identifier(elem, "d"))  is_d = 1;
    else if (is_operator(elem, "∂"))    is_del = 1;
    else                                return 0;

    // There should be at least two terms in the product
    elem = mxmlGetNextSibling(elem);
    if (!elem)  return 0;

    // All remaining terms should be "∂" operator or identifier
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        if (is_identifier(elem, NULL))  continue;
        if (is_del && is_operator(elem, "∂"))  continue;
        return 0;
    }

    if (is_d)    return 1;
    if (is_del)  return 2;
}


/// @brief Extends differential numerator up to and including d or ∂
static mxml_node_t*
extend_differential_numerator(mxml_node_t* num, int diffType)
{
    mxml_node_t* last = num;
    mxml_node_t* x = num;
    for (;  x;  num = x, x = mxmlGetPrevSibling(x))
    {
        if ((diffType == 1) && is_identifier(x, "d"))
        {
            num = x;
            break;
        }
        if ((diffType == 2) && is_operator(x, "∂"))
        {
            num = x;
            break;
        }
    }

    // Group multiple nodes into <mrow>
    if (num != last)  num = group_siblings("mrow", num, last);

    return num;
}


/// @brief Converts <FRACTION> and surrounding nodes into <mfrac>
static void
fix_fraction(mxml_node_t* x)
{
    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* num;
        mxml_node_t* denom;
        mxml_node_t* mfrac;
        int          diff_type = 0;

        // Process only <FRACTION> nodes
        if (strcmp(mxmlGetElement(elem), "FRACTION") != 0)  continue;

        // Find denominator
        denom = mxmlGetNextSibling(elem);
        if (!denom)  continue;
        denom = extend_denominator(denom);
        remove_round_bracket(denom);
        diff_type = is_differential_denominator(denom);

        // Find numerator
        num = mxmlGetPrevSibling(elem);
        if (!num)  continue;
        if (diff_type)  num = extend_differential_numerator(num, diff_type);
        else            num = extend_numerator(num);
        remove_round_bracket(num);

        // Re-arrange DOM tree
        mfrac = mxmlNewElement(MXML_NO_PARENT, "mfrac");
        mxmlAdd(x, MXML_ADD_BEFORE, elem, mfrac);
        mxmlAdd(mfrac, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, num);
        mxmlAdd(mfrac, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, denom);
        mxmlDelete(elem);
        elem = mfrac;
    }
}


/// @brief Extends denominator of continued fraction
static mxml_node_t*
extend_continued_denominator(mxml_node_t* denom)
{
    // Find all extendable nodes
    mxml_node_t* first = denom;
    mxml_node_t* x = mxmlGetNextSibling(denom);
    for (;  x;  denom = x, x = mxmlGetNextSibling(x))
    {
        // End continued fraction at equal signs
        if (is_operator(x, "="))  break;
    }

    // Group multiple nodes into <mrow>
    if (first != denom)  denom = group_siblings("mrow", first, denom);

    return denom;
}


/// @brief Converts <CONT_FRAC> and surrounding nodes into <mfrac>
static void
fix_continued_fraction(mxml_node_t* x)
{
    mxml_node_t* elem = mxmlGetLastChild(x);
    for (;  elem;  elem = mxmlGetPrevSibling(elem))
    {
        mxml_node_t* num;
        mxml_node_t* denom;
        mxml_node_t* last;
        mxml_node_t* mfrac;

        // Process only <CONT_FRAC> nodes
        if (strcmp(mxmlGetElement(elem), "CONT_FRAC") != 0)  continue;

        // Find numerator
        num = mxmlGetPrevSibling(elem);
        if (!num)  continue;
        num = extend_numerator(num);
        remove_round_bracket(num);

        // Denominator is all nodes following the fraction line
        denom = mxmlGetNextSibling(elem);
        if (!denom)  continue;
        denom = extend_continued_denominator(denom);
        remove_round_bracket(denom);

        // Re-arrange DOM tree
        mfrac = mxmlNewElement(MXML_NO_PARENT, "mfrac");
        mxmlAdd(x, MXML_ADD_BEFORE, elem, mfrac);
        mxmlAdd(mfrac, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, num);
        mxmlAdd(mfrac, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, denom);
        mxmlDelete(elem);
        elem = mfrac;
    }
}


/// @brief Applies <STRUCK_OUT> to the term on the right
static void
fix_struck_out(mxml_node_t* x)
{
    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* expr;

        // Process only <STRUCK_OUT> nodes
        if (strcmp(mxmlGetElement(elem), "STRUCK_OUT") != 0)  continue;

        // Find expression to strike out
        expr = mxmlGetNextSibling(elem);
        if (expr)
        {
            mxml_node_t* menclose;

            // Find inner most item to strike out
            expr = bypass_style_and_indices(expr);

            // Perform conversion
            menclose = group_siblings("menclose", expr, expr);
            mxmlElementSetAttr(menclose, "notation", "updiagonalstrike");
        }

        // Delete <STRUCK_OUT> node
        expr = elem;
        elem = mxmlGetNextSibling(elem);
        mxmlDelete(expr);
    }
}


/// @brief Moves first item following <mroot> and <msqrt> into element
static void
fix_root(mxml_node_t* x)
{
    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* arg;

        // Process only <mroot> and <msqrt> nodes
        if ((strcmp(mxmlGetElement(elem), "mroot") != 0) &&
                (strcmp(mxmlGetElement(elem), "msqrt") != 0))
            continue;

        // Find argument
        arg = mxmlGetNextSibling(elem);
        if (!arg)  continue;
        remove_round_bracket(arg);

        // Re-arrange DOM tree
        mxmlDelete(mxmlGetFirstChild(elem));
        mxmlAdd(elem, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT, arg);
    }
}


/// @brief Fixes sum and product into <mo> and change index position
static void
fix_sum_product(mxml_node_t* x)
{
    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* arg = elem;
        const char*  name = mxmlGetElement(elem);
        const char*  mi;

        // Locate actual argument
        while (mxmlGetElement(mxmlGetFirstChild(arg)))
            arg = mxmlGetFirstChild(arg);
        name = mxmlGetElement(arg);

        // Process only <mi> argument
        if (strcmp(mxmlGetElement(arg), "mi") != 0)  continue;
        mi = get_element_text(arg);

        // Convert only summation and product
        if (strcmp(mi, "Σ") == 0)
        {
            // Change from Greek letter to mathematical operator
            mxmlSetText(mxmlGetFirstChild(arg), 0, "∑");
        }
        else if (strcmp(mi, "Π") == 0)
        {
            // Change from Greek letter to mathematical operator
            mxmlSetText(mxmlGetFirstChild(arg), 0, "∏");
        }
        else  continue;

        // Convert to operator
        mxmlSetElement(arg, "mo");
    }
}


/// @brief Changes index position for operators
static void
fix_operator_scripts(mxml_node_t* x)
{
    const char* operators[] =
    {
        // Operators that uses superscript and subscript
        "+", "−", "×", "÷",
        "±", "∓", "=", "≜",
        "∇", "∂", "!", "|",
        "∫", "∮",

        // List terminator
        NULL
    };
    const char** op;

    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* arg = elem;
        const char*  name = mxmlGetElement(elem);
        const char*  mo;
        int          skip_op = 0;

        // Locate actual argument
        while (mxmlGetElement(mxmlGetFirstChild(arg)))
            arg = mxmlGetFirstChild(arg);
        name = mxmlGetElement(arg);

        // Process only <mo> argument
        if (strcmp(mxmlGetElement(arg), "mo") != 0)  continue;
        mo = get_element_text(arg);

        // Ignore trigonometric operator
        if (is_trigonometric_operator(mo))  continue;

        // Skip operators that uses subscript and superscript
        for (op = operators;  !skip_op && *op;  ++op)
            if (strcmp(mo, *op) == 0)  skip_op = 1;
        if (skip_op)  continue;

        // Convert nodes
        while (arg != elem)
        {
            arg  = mxmlGetParent(arg);
            name = mxmlGetElement(arg);
            if (strcmp(name, "msub") == 0)
                mxmlSetElement(arg, "munder");
            else if (strcmp(name, "msup") == 0)
                mxmlSetElement(arg, "mover");
            else if (strcmp(name, "msubsup") == 0)
                mxmlSetElement(arg, "munderover");
        }
    }
}


/// @brief Attaches indices of <LEFT_INDEX> to term on the right
static void
fix_left_indices(mxml_node_t* x)
{
    int          fixed = 0;
    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* arg = elem;
        mxml_node_t* owner;
        mxml_node_t* presub;
        mxml_node_t* presup;
        const char*  name = mxmlGetElement(elem);
        int          whitespace = 0;
        const char*  mo;

        // Process only <msub>, <msup> and <msubsup> nodes
        if ((strcmp(name, "msub") != 0) &&
                (strcmp(name, "msup") != 0) &&
                (strcmp(name, "msubsup") != 0))
            continue;

        // Locate actual argument
        while ((strcmp(name, "msub") == 0) ||
                (strcmp(name, "msup") == 0) ||
                (strcmp(name, "msubsup") == 0))
        {
            arg  = mxmlGetFirstChild(arg);
            name = mxmlGetElement(arg);
        }

        // Process only <LEFT_INDEX> argument
        if (strcmp(mxmlGetElement(arg), "LEFT_INDEX") != 0)  continue;

        // Find the new owner of the indices
        owner = mxmlGetNextSibling(elem);
        if (!owner)
        {
            // No more sibling, delete the indices
            mxmlDelete(elem);
            break;
        }

        // Convert owner to <mmultiscripts>
        name = mxmlGetElement(owner);
        if (strcmp(name, "msub") == 0)
            mxmlNewElement(owner, "none");
        else if (strcmp(name, "msup") == 0)
            mxmlAdd(owner, MXML_ADD_BEFORE, mxmlGetLastChild(owner),
                    mxmlNewElement(owner, "none"));
        else if (strcmp(name, "msubsup") != 0)
            owner = group_siblings("mmultiscripts", owner, owner);
        mxmlSetElement(owner, "mmultiscripts");
        mxmlNewElement(owner, "mprescripts");
        presub  = mxmlNewElement(owner, "none");
        presup  = mxmlNewElement(owner, "none");

        // Attach presubscript and postsuperscript to owner
        while (arg != elem)
        {
            arg  = mxmlGetParent(arg);
            name = mxmlGetElement(arg);
            if (strcmp(name, "msub") == 0)
            {
                mxmlAdd(owner, MXML_ADD_BEFORE, presub, mxmlGetLastChild(arg));
                mxmlDelete(presub);
            }
            else if (strcmp(name, "msup") == 0)
            {
                mxmlAdd(owner, MXML_ADD_BEFORE, presup, mxmlGetLastChild(arg));
                mxmlDelete(presup);
            }
            else if (strcmp(name, "msubsup") == 0)
            {
                mxmlAdd(owner, MXML_ADD_BEFORE, presup, mxmlGetLastChild(arg));
                mxmlAdd(owner, MXML_ADD_BEFORE, presub, mxmlGetLastChild(arg));
                mxmlDelete(presup);
                mxmlDelete(presub);
            }
        }

        // Advance to next term
        mxmlDelete(elem);
        elem  = owner;
        fixed = 1;
    }

    // Remove round bracket around single item
    if (fixed && (mxmlGetFirstChild(x) == mxmlGetLastChild(x)))
        remove_round_bracket(x);
}


/// @brief Checks whether an element contains an <mfrac>
static int
is_mfrac(mxml_node_t* x)
{
    for (;  x;  x = mxmlGetFirstChild(x))
    {
        const char* name = mxmlGetElement(x);
        if (!name)  break;
        if (strcmp(name, "mfrac") == 0)  return 1;
    }
    return 0;
}


/// @brief Replaces implicit separators with actual ones
static void
fix_implicit_separators(mxml_node_t* x)
{
    const char*  sep;

    // Make sure it is a <mfenced> node
    if (strcmp(mxmlGetElement(x), "mfenced") != 0)  return;

    // Insert explicit separators
    sep = mxmlElementGetAttr(x, "separators");
    if (strlen(sep) > 0)
    {
        mxml_node_t* elem = mxmlGetFirstChild(x);
        for (;  elem;  elem = mxmlGetNextSibling(elem))
        {
            mxml_node_t* next;

            // Do nothing when current node is already an operator
            if (is_operator(elem, NULL))  continue;

            // Do nothing when next node is already an operator
            next = mxmlGetNextSibling(elem);
            if (!next)  break;
            if (is_operator(next, NULL))  continue;

            // Otherwise insert a separator between current node and next node
            mxml_node_t* mo = mxmlNewElement(MXML_NO_PARENT, "mo");
            mxmlNewText(mo, 0, sep);
            mxmlAdd(x, MXML_ADD_AFTER, elem, mo);
        }

        // Turn off implicit separator
        mxmlElementSetAttr(x, "separators", "");
    }
}


/// @brief Pads <mtext> with <mspace>
static void
fix_text_spacing(mxml_node_t* x)
{
    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* sibling;

        // Pad only <mtext>
        if (strcmp(mxmlGetElement(elem), "mtext") != 0)  continue;

        // Check for <mspace> before current element
        sibling = mxmlGetPrevSibling(elem);
        if (sibling && strcmp(mxmlGetElement(sibling), "mspace") != 0)
        {
            // Insert <mspace> before current element
            mxml_node_t* mspace = mxmlNewElement(MXML_NO_PARENT, "mspace");
            mxmlElementSetAttr(mspace, "width", MSPACE_WIDTH);
            mxmlAdd(x, MXML_ADD_BEFORE, elem, mspace);
        }

        // Check for <mspace> after current element
        sibling = mxmlGetNextSibling(elem);
        if (sibling && strcmp(mxmlGetElement(sibling), "mspace") != 0)
        {
            // Insert <mspace> after current element
            mxml_node_t* mspace = mxmlNewElement(MXML_NO_PARENT, "mspace");
            mxmlElementSetAttr(mspace, "width", MSPACE_WIDTH);
            mxmlAdd(x, MXML_ADD_AFTER, elem, mspace);
        }
    }
}


/// @brief Removes unneeded round brackets from expression
static void
remove_unneeded_brackets(mxml_node_t* x)
{
    mxml_node_t* elem = mxmlGetFirstChild(x);
    for (;  elem;  elem = mxmlGetNextSibling(elem))
    {
        mxml_node_t* prev;
        mxml_node_t* next;
        int          is_fraction;

        // Process only <mfenced>
        if (strcmp(mxmlGetElement(elem), "mfenced") != 0)  continue;

        // Make sure it is enclosed in round brackets
        if (strcmp(mxmlElementGetAttr(elem, "open"), "(") != 0)  continue;
        if (strcmp(mxmlElementGetAttr(elem, "close"), ")") != 0)  continue;

        // Retain bracket if there are multiple terms
        if (mxmlGetFirstChild(elem) != mxmlGetLastChild(elem))  continue;

        // Check for fraction in current term
        is_fraction = is_mfrac(elem);

        // Find previous and next element 
        prev = mxmlGetPrevSibling(elem);
        next = mxmlGetNextSibling(elem);

        // Retain bracket if previous term is similar to current term
        if (prev && is_same(is_mfrac(prev), is_fraction))  continue;

        // Retain bracket if next term is similar to current term
        if (next && is_same(is_mfrac(next), is_fraction))  continue;

        // Remove redundant bracket now
        remove_round_bracket(elem);
        elem = simplify_single_child_mrow(elem);
    }
}


/// @brief Parses annuity symbol
static int
parse_annuity(mxml_node_t* x, const char* brl, size_t len)
{
    // Dot 4-1456 encloses previous term in annuity symbol
    if (starts_with(brl, len, "@?"))
    {
        mxml_node_t* arg = mxmlGetLastChild(x);
        if (arg)
        {
            mxml_node_t* menclose = group_siblings("menclose", arg, arg);
            mxmlElementSetAttr(menclose, "notation", "actuarial");
            remove_round_bracket(arg);
            return 2;
        }
    }

    return 0;
}


/// @brief Letter fount sign definition
typedef struct
{
    const char* mpBrl;
    int(*mpCharTest)(char);
    int         mIsSeq;
    int         mFount;
} FountRec;


/// @brief Parses letter fount
static int
parse_fount(mxml_node_t* x, const char* brl, size_t len,
        int* fount)
{
    const FountRec founts[] =
    {
        // Double letter fount sign for character sequence
        {",,",  is_brl_latin, 1, FOUNT_LATIN_CAPITAL     },  // Dot 6-6
        {"@@",  is_brl_latin, 1, FOUNT_BOLD_LATIN_SMALL  },  // Dot 4-4
        {"^^",  is_brl_latin, 1, FOUNT_BOLD_LATIN_CAPITAL},  // Dot 45-45
        {"..",  is_brl_greek, 1, FOUNT_GREEK_SMALL       },  // Dot 46-46
        {",_",  is_brl_greek, 1, FOUNT_GREEK_CAPITAL     },  // Dot 6-456

        // Single letter fount sign
        {";",   is_brl_latin, 0, FOUNT_LATIN_SMALL       },  // Dot 56
        {",",   is_brl_latin, 0, FOUNT_LATIN_CAPITAL     },  // Dot 6
        {";,",  is_brl_latin, 0, FOUNT_LATIN_CAPITAL     },  // Dot 56-6
        {"@",   is_brl_latin, 0, FOUNT_BOLD_LATIN_SMALL  },  // Dot 4
        {"^",   is_brl_latin, 0, FOUNT_BOLD_LATIN_CAPITAL},  // Dot 45
        {".",   is_brl_greek, 0, FOUNT_GREEK_SMALL       },  // Dot 46
        {"_",   is_brl_greek, 0, FOUNT_GREEK_CAPITAL     },  // Dot 456
        {"@.",  is_brl_greek, 0, FOUNT_BOLD_GREEK_SMALL  },  // Dot 4-46
        {"^_",  is_brl_greek, 0, FOUNT_BOLD_GREEK_CAPITAL},  // Dot 45-456
        {";\"", is_brl_latin, 0, FOUNT_CUSTOM_1_SMALL    },  // Dot 56-5
        {";\"", is_brl_latin, 0, FOUNT_CUSTOM_1_CAPITAL  },  // Dot 6-5
        {"@\"", is_brl_latin, 0, FOUNT_CUSTOM_2_SMALL    },  // Dot 4-5
        {"^\"", is_brl_latin, 0, FOUNT_CUSTOM_2_CAPITAL  },  // Dot 45-5
        {";@",  is_brl_latin, 0, FOUNT_CUSTOM_3_SMALL    },  // Dot 56-4
        {",^",  is_brl_latin, 0, FOUNT_CUSTOM_3_CAPITAL  },  // Dot 6-45
        {"@;",  is_brl_latin, 0, FOUNT_CUSTOM_4_SMALL    },  // Dot 4-56
        {"^,",  is_brl_latin, 0, FOUNT_CUSTOM_4_CAPITAL  },  // Dot 45-6

        // List terminator
        {NULL}
    };

    // Check all known founts
    const FountRec* rec;
    for (rec = founts;  rec->mpBrl;  ++rec)
    {
        int  fount_len = strlen(rec->mpBrl);
        char c;
        if (len <= fount_len)  continue;
        if (memcmp(rec->mpBrl, brl, fount_len) != 0)  continue;
        c = brl[fount_len];
        if (!rec->mpCharTest(c))  continue;
        *fount = (rec->mIsSeq ? rec->mFount : FOUNT_LATIN_SMALL);
        return fount_len + parse_letter(x, &c, 1, rec->mFount);
    }

    // Nothing matched
    return 0;
}


/// @brief Parses struck out letter or sign
static int
parse_struck_out(mxml_node_t* x, const char* brl, size_t len)
{
    mxml_node_t* elem;

    // Look for dot 5
    if (*brl != '"')  return 0;

    // Convert <FRACTION> first
    fix_fraction(x);
    
    // Dot 5 following <mfrac> is a differential operator separation sign
    elem = mxmlGetLastChild(x);
    if (elem && (strcmp(mxmlGetElement(elem), "mfrac") == 0))  return 1;

    // Other it is a struck out code
    mxmlNewElement(x, "STRUCK_OUT");
    return 1;
}


/// @brief Parses plus or minus index
static int
parse_plus_minus_index(mxml_node_t* x, const char* brl, size_t len,
        const char* closeBrl)
{
    mxml_node_t* base = mxmlGetLastChild(x);
    size_t       org_len = len;
    const char*  index = NULL;

    // Make sure there is a base expression
    if (!base)  return 0;

    if (starts_with(brl, len, ";6"))
    {
        // Dot 56-235 is plus
        index = "+";
        brl  += 2;
        len  -= 2;
    }
    else if (starts_with(brl, len, ";-"))
    {
        // Dot 56-36 is minus
        index = "−";
        brl  += 2;
        len  -= 2;
    }
    else
    {
        // Stop when there is no match
        return 0;
    }

    if ((len > 0) && (*brl == ']'))
    {
        // Consume index terminator
        --len;
    }
    else if (len && !is_brl_eol(brl, len) && !starts_with(brl, len, closeBrl))
    {
        // Plus or minus index should not be followed by anything else
        return 0;
    }

    // Add superscript
    apply_operator(base, "msup", index);

    // Report number of bytes used
    return (org_len - len);
}


/// @brief Parses a bracketed expression
static int
parse_bracket_expr(mxml_node_t* x, const char* brl, size_t len,
        const char* openAttr, const char* closeAttr, const char* closeBrl)
{
    mxml_node_t* mfenced = mxmlNewElement(x, "mfenced");
    mxmlElementSetAttr(mfenced, "open", openAttr);
    mxmlElementSetAttr(mfenced, "close", closeAttr);
    mxmlElementSetAttr(mfenced, "separators", "");
    return parse_expr(mfenced, brl, len, BASE_EXPR, closeBrl);
}


/// @brief Parses bracket
static int
parse_bracket(mxml_node_t* x, const char* brl, size_t len)
{
    const char* symbols[] =
    {
        // Symbols in enclosed in circle
        "<;6>", "⊕",
        "<;->", "⊖",
        "<;8>", "⊗",
        "<;4>", "⊘",
        "<;'>", "⊙",
        "<;7>", "⊜",

        // Symbols in enclosed in square
        "(;6)", "⊞",
        "(;-)", "⊟",
        "(;8)", "⊠",
        "(;')", "⊡",

        // List terminator
        NULL
    };
    const char** sym;

    // Look for enclosed symbols
    for (sym = symbols;  *sym;  sym += 2)
    {
        // Check braille sign
        if (!starts_with(brl, len, sym[0]))  continue;

        // Create <mo> node
        mxmlNewText(mxmlNewElement(x, "mo"), 0, sym[1]);

        // Report number of bytes used
        return strlen(sym[0]);
    }

    // Dot 126 and 345 enclose round bracket expression
    if (*brl == '<')
        return 1 + parse_bracket_expr(x, brl + 1, len - 1, "(", ")", ">");

    // Dot 12356 and 23456 enclose square bracket expression
    if (*brl == '(')
        return 1 + parse_bracket_expr(x, brl + 1, len - 1, "[", "]", ")");

    // Dot 23456 and 12356 enclose outward-facing square bracket expression
    if (*brl == ')')
        return 1 + parse_bracket_expr(x, brl + 1, len - 1, "]", "[", "(");

    // Dot 246 and 135 enclose curly bracket expression
    if (*brl == '[')
        return 1 + parse_bracket_expr(x, brl + 1, len - 1, "{", "}", "o");

    // Dot 46-12356 and 46-23456 enclose angle bracket expression
    if (starts_with(brl, len, ".("))
        return 2 + parse_bracket_expr(x, brl + 2, len - 2, "〈", "〉", ".)");

    // Nothing matched
    return 0;
}


/// @brief Parses literal text surrounded by dot 236 and dot 356
static int
parse_literal_text(mxml_node_t* x, const char* brl, size_t len)
{
    const char* punctuations[] =
    {
        // Standard punctuations
        "7",    "(",
        ",7",   ")",
        "6",    "!",
        "4",    ".",
        "3",    ":",
        "2",    ";",
        "1",    ",",

        // Line feeds
        "\n",   " ",
        "\r\n", " ",

        // List terminator
        NULL
    };

    const char** pct;
    size_t       org_len = len;
    StrBuf*      buf;
    const char*  text;

    // Dot 236 starts literal text
    if (*brl != '8')  return 0;
    ++brl;
    --len;

    // Find end of literal text
    buf  = create_buffer();
    text = brl;
    while (len > 0)
    {
        char c = *brl;
        int  used = 0;

        // Dot 356 ends literal text
        if (c == '0')
        {
            --len;
            break;
        }

        // Dot 6 is capital Latin letter
        if ((c == ',') && starts_with_brl_latin(brl + 1, len - 1))
        {
            append_char(buf, brl[1] - 'a' + 'A');
            used = 2;
        }

        // Check all known punctuations 
        for (pct = punctuations;  !used && *pct;  pct += 2)
        {
            if (!starts_with(brl, len, pct[0]))  continue;
            append_text(buf, pct[1]);
            used = strlen(pct[0]);
        }

        // Ignore lowercase Latin fount
        if (c == ';')  used = 1;

        // Append all other characters literally
        if (!used)
        {
            append_char(buf, c);
            used = 1;
        }

        // Advance to next character
        brl += used;
        len -= used;
    }

    // Create <mtext> node
    mxmlNewText(mxmlNewElement(x, "mtext"), 0, buf->mpStr);
    destroy_buffer(buf);

    // Report number of bytes used
    return org_len - len;
}


/// @brief Post-process an expression after parsing
static void
postprocess_expr(mxml_node_t* x)
{
    // Apply <STRUCK_OUT> to the term on the right
    fix_struck_out(x);

    // Re-arrange DOM for <mroot> and <msqrt>
    fix_root(x);

    // Convert sum and product from <mi> to <mo>
    fix_sum_product(x);

    // Convert operators' <msub>, <msup> into <munder>, <mover>
    fix_operator_scripts(x);

    // Attach indices of <LEFT_INDEX> to the term on the right
    fix_left_indices(x);

    // Convert <FRACTION> into <mfrac>
    fix_fraction(x);

    // Convert <CONT_FRAC> into <mfrac>
    fix_continued_fraction(x);

    // Replace implicit separators with actual ones
    fix_implicit_separators(x);

    // Pad <mtext> with <mspace>
    fix_text_spacing(x);

    // Remove unneeded round brackets from expression
    remove_unneeded_brackets(x);
}


/** @brief Ends current term in bracket.
    @return     Pointer to element containing @p last.
    @param x        Pointer to outer element.
    @param first    Predecessor to first node in current term.  @p NULL means
                    first child.
    @param last     Last node in current term.  @p NULL means last child.
  */
static mxml_node_t*
end_term_in_bracket(mxml_node_t* x, mxml_node_t* first, mxml_node_t* last)
{
    mxml_node_t* result;

    // Implicit separator only supported inside <mfenced>
    if (strcmp(mxmlGetElement(x), "mfenced") != 0)  return NULL;

    // Turn on automatic separators
    mxmlElementSetAttr(x, "separators", ",");

    // If last node is punctuation, exclude it from group
    if (!last)  last = mxmlGetLastChild(x);
    result = last;
    if (is_operator(last, ",") ||
            is_operator(last, ";") ||
            is_operator(last, ":") ||
            is_operator(last, "."))
        last = mxmlGetPrevSibling(last);

    // Group the nodes since previous comma into <mrow>
    if (!first)  first = mxmlGetFirstChild(x);
    else         first = mxmlGetNextSibling(first);
    if (first && last)
    {
        mxml_node_t* mrow = group_siblings("mrow", first, last);
        postprocess_expr(mrow);
        if (result == last)  result = mrow;
    }

    // Return last element of enclosed terms
    return result;
}


/// @brief Parses an expression
static int
parse_expr(mxml_node_t* x, const char* brl, size_t len,
        int isIndex, const char* closeBrl)
{
    size_t       org_len = len;
    int          spc = 1;
    int          fount = FOUNT_LATIN_SMALL;
    int          is_bracketed = (strcmp(mxmlGetElement(x), "mfenced") == 0);
    mxml_node_t* prev_term = NULL;

    // Do nothing if braille is empty
    if (!len)  return 0;

    while (len)
    {
        size_t used = 0;
        char   c = *brl;

        // Check for end of superscript or subscript
        if (isIndex)
        {
            // Space terminates superscript and subscript
            if (c == ' ')  break;

            // Dot 12456 terminates superscript and subscript
            if (c == ']')
            {
                --len;
                break;
            }

            // Explicit comma terminates superscript and subscript
            if (starts_with(brl, len, ",1 "))  break;

            // Closing bracket terminates superscript and subscript
            if (starts_with(brl, len, closeBrl))  break;
        }

        // Found close bracket, consume it and stop parsing
        if (starts_with(brl, len, closeBrl))
        {
            len -= strlen(closeBrl);
            break;
        }

        // Handle end-of-line
        used = is_brl_eol(brl, len);
        if (used)
        {
            // End-of-line terminates an open expression
            if (!is_bracketed)  break;

            // Otherwise it ends current term inside a set
            prev_term = end_term_in_bracket(x, prev_term, NULL);
            spc  = 1;
        }

        // Space resets fount back to small Latin
        if (c == ' ')  fount = FOUNT_LATIN_SMALL;

        // Try to parse mathematical hyphen
        if (!used)
        {
            used = is_brl_math_hyphen(brl, len);
            if (used)  spc = 1;
        }

        // Try to parse number
        if (!used)  used = parse_number(x, brl, len);

        // Try to parse unit
        if (!used)  used = parse_units(x, brl, len);

        // Try to parse superscript
        if (!used)  used = parse_superscript(x, brl, len, closeBrl);

        // Try to parse subscript
        if (!used)
        {
            char prev_char = '\0';
            if (org_len > len)  prev_char = *(brl - 1);
            if (strchr(" \n\r", prev_char))  prev_char = '\0';
            used = parse_subscript(x, brl, len, closeBrl, prev_char);
        }

        // Try to parse overscript
        if (!used)  used = parse_overscript(x, brl, len, closeBrl);

        // Try to parse underscript
        if (!used)  used = parse_underscript(x, brl, len, closeBrl);

        // Try to parse root
        if (!used)  used = parse_root(x, brl, len, closeBrl);

        // Try to parse annuity symbol
        if (!used)  used = parse_annuity(x, brl, len);

        // Try to parse bracket
        if (!used)  used = parse_bracket(x, brl, len);

        // Try to parse + or - index
        if (!used && !isIndex)
            used = parse_plus_minus_index(x, brl, len, closeBrl);

        // Try to parse operator
        if (!used)
        {
            int          adj = 0;
            int          extra_spc = 0;
            mxml_node_t* last = mxmlGetLastChild(x);

            // Check for space at start of braille
            if (c == ' ')
            {
                spc = 1;
                adj = 1;
            }

            // Parse operators in index as if there were a leading space
            if (isIndex)  spc = 1;

            // Try to parse operator
            used = parse_operators(x, brl + adj, len - adj,
                    spc, closeBrl, &extra_spc);
            if (used)  used += adj;

            // Standalone space ends current term
            if (adj && (!used || extra_spc))
                prev_term = end_term_in_bracket(x, prev_term, last);
        }

        // Try to parse letter fount
        if (!used)  used = parse_fount(x, brl, len, &fount);

        // Try to parse struck out letter or sign
        if (!used)  used = parse_struck_out(x, brl, len);

        // Try to parse literal text
        if (!used)  used = parse_literal_text(x, brl, len);

        // Try to parse letter for current fount
        if (!used)  used = parse_letter(x, brl, len, fount);

        // Skip unrecognized braille
        if (!used)  used = 1;

        // Parse next braille segment
        brl += used;
        len -= used;
    }

    // Remove orphaned <STRUCK_OUT> element
    if (mxmlGetLastChild(x))
    {
        mxml_node_t* last = mxmlGetLastChild(x);
        if (strcmp(mxmlGetElement(last), "STRUCK_OUT") == 0)
            mxmlDelete(last);
    }

    // For <mfenced> with implicit separator, group last term
    if (prev_term)  end_term_in_bracket(x, prev_term, NULL);

    // Perform post-processing
    postprocess_expr(x);

    // Report number of bytes used
    return (org_len - len);
}


char*
brl2mml_from_ukmaths(const char* brl, int* used)
{
    char*  mml = NULL;
    size_t len = strlen(brl);

    // Parse UK Maths braille and convert to <math> node
    mxml_node_t* math = mxmlNewElement(MXML_NO_PARENT, "math");
    mxml_node_t* mrow = mxmlNewElement(math, "mrow");
    mxmlElementSetAttr(math, "xmlns", "http://www.w3.org/1998/Math/MathML");
    mxmlElementSetAttr(math, "display", "block");
    *used = parse_expr(mrow, brl, len, BASE_EXPR, "");

    // Consume end-of-line
    *used += is_brl_eol(brl + *used, len - *used); 

    // Build XML string
    if (mxmlGetFirstChild(mrow) != NULL)
        mml = mxmlSaveAllocString(math, MXML_NO_CALLBACK);

    // Clean up
    mxmlDelete(math);

    // Return XML string to caller
    return mml;
}


/* vim: set nowrap cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
