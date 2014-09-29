/**********************************************************************
*
*  Filename:    brl2mml.c
*  Description: Test program for libbrl2mml
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
        "USAGE:  brl2mml [options] content...\n"
        "Options:\n"
        "  -e/--encoding <target>    Input braille encoding\n"
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


static int
do_main(int argc, char** argv)
{
    // Parse command line
    parse_args(argc, argv);

    // Print text before output
    switch (gEncoding)
    {
        case TARGET_UKMATHS:
            printf(
                "<?xml version='1.0' encoding='UTF-8'?>\n"
                "<!DOCTYPE html PUBLIC\n"
                "  '-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN'\n"
                "  'http://www.w3.org/TR/MathML2/dtd/xhtml-math11-f.dtd'\n"
                "  [<!ENTITY mathml 'http://www.w3.org/1998/Math/MathML'>]>\n"
                "<html xmlns='http://www.w3.org/1999/xhtml'>\n"
                "<head><title>brl2mml</title></head>\n"
                "<body>\n");
            break;
        default:
            break;
    }

    // Process each argument in turn
    argc = gArgC;
    argv = gArgV;
    for (;  argc > 0;  --argc, ++argv)
    {
        char* in_str = NULL;

        // Obtain input string
        if      (gIsStdin)     in_str = read_stdin();
        else if (gIsFilename)  in_str = read_file_data(*argv);
        else                   in_str = strdup(*argv);

        // Skip current argument if data is not available
        if (!in_str)  continue;

        // Perform actual conversion
        switch (gEncoding)
        {
            case TARGET_UKMATHS:
                convert_now(BRL2MML_FROM_UKMATHS, in_str);
                break;

            default:
                break;
        }

        // Release input string after use
        free(in_str); 
    }

    // Print text after output
    switch (gEncoding)
    {
        case TARGET_UKMATHS:
            printf(
                "</body>\n"
                "</html>\n");
            break;
        default:
            break;
    }

    return EXIT_SUCCESS;
}


int
main(int argc, char** argv)
{
    return do_main(argc, argv);
}


/* vim: set cindent tabstop=8 softtabstop=4 expandtab shiftwidth=4: */
