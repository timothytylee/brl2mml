/**********************************************************************
*
*  Filename:    mml2brl.c
*  Description: Test program for libbrl2mml
*  Version:     $Revision: 1.6 $
*  Date:        $Date: 2014/09/27 12:43:11 $
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

#include "mxml.h"
#include "test_util.h"


static int gIsStdin = 0;
static int gIsFilename = 0;
static int gEncoding = TARGET_UNKNOWN;
static int gArgC;
static char** gArgV;


static void
show_help()
{
    fprintf(stderr,
        "USAGE:  mml2brl [options] content...\n"
        "Options:\n"
        "  -e/--encoding <target>    Output braille encoding\n"
        "  -f/--file                 Treats content as filenames\n"
        "  -h/--help                 Display this information\n"
        "Supported braille targets:\n"
        "  ukmaths                   UK Maths Braille\n");
    exit(EXIT_SUCCESS);
}


static void
parse_args(int argc, char** argv)
{
    for (argc--, argv++;  argc;  --argc, ++argv)
    {
        if (match_option(*argv, "-h", "--h"))
        {
            show_help();
        }
        else if (match_option(*argv, "-f", "--file"))
        {
            gIsFilename = 1;
        }
        else if (match_option(*argv, "-e", "--encoding"))
        {
            --argc;  ++argv;
            if (!argc)  show_help();
            switch (tolower(argv[0][0]))
            {
                case 'u':  gEncoding = TARGET_UKMATHS;  break;
                default:   show_help();
            }
        }
        else break;
    }

    // Determine input source
    if (!argc)
    {
        if (gIsFilename)
        {
            // At least one filename must be given
            show_help();
        }
        else
        {
            // Use stdin if there are no more command line parameters
            gIsStdin = 1;
            argc     = 1;
        }
    }

    // Make sure braille encoding has been specified
    if (!gEncoding)  show_help();

    // Save final argc and argv
    gArgC = argc;
    gArgV = argv;
}


/// @brief Searches for <math> node in child elements
static char*
find_math(mxml_node_t* x)
{
    char*        xml;
    mxml_node_t* elem;

    // Look through child nodes
    for (elem = mxmlGetFirstChild(x);  elem;  elem = mxmlGetNextSibling(elem))
    {
        // Process only DOM elements
        const char* name = mxmlGetElement(elem);
        if (!name)  continue;

        // Recurse into non-MathML elements
        if (strcmp(name, "math") != 0)
        {
            find_math(elem);
            continue;
        }

        // Found <math> node, perform actual conversion
        xml = mxmlSaveAllocString(elem, MXML_NO_CALLBACK);
        switch (gEncoding)
        {
            case TARGET_UKMATHS:
                convert_now(BRL2MML_TO_UKMATHS, xml);
                break;

            default:
                break;
        }

        // Clean up
        free(xml);

        // Separate consecutive equations with a blank line
        puts("");
    }
}


static int
do_main(int argc, char** argv)
{
    // Parse command line
    parse_args(argc, argv);

    // Process each argument in turn
    argc = gArgC;
    argv = gArgV;
    for (;  argc > 0;  --argc, ++argv)
    {
        char*        in_str = NULL;
        mxml_node_t* x;

        // Obtain input string
        if      (gIsStdin)     in_str = read_stdin();
        else if (gIsFilename)  in_str = read_file_data(*argv);
        else                   in_str = strdup(*argv);

        // Skip current argument if data is not available
        if (!in_str)  continue;

        // Parse XML into DOM tree
        x = mxmlNewElement(MXML_NO_PARENT, "xml");
        mxmlLoadString(x, in_str, MXML_NO_CALLBACK);
        free(in_str); 

        // Traverse the DOM tree looking for <math> nodes
        find_math(x);

        // Release DOM tree after use
        mxmlDelete(x);
    }

    return EXIT_SUCCESS;
}


int
main(int argc, char** argv)
{
    return do_main(argc, argv);
}


/* vim: set cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
