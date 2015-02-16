# Declare package name and version number
AC_INIT([Braille to MathML], [0.1.0])

# Update this value for every release! (A:B:C will map to foo.so.(A-C).C.B)
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
SO_VERSION=1:0:0

# Locate the source directory
AC_CONFIG_SRCDIR([src/common.c])

# Search for auxilliary scripts only in current directory
AC_CONFIG_AUX_DIR([.])

# Disable static library by default
AC_DISABLE_STATIC

# Build DLL on win32
AC_LIBTOOL_WIN32_DLL

# Use libtool support building shared library
AC_PROG_LIBTOOL
AC_SUBST(SO_VERSION)

# Test for C language support
AC_LANG_C

# Check for header files
AC_HEADER_STDC

# Check for pthreads
have_pthread=no
PTHREAD_LIBS=""
AC_CHECK_HEADER(pthread.h, AC_DEFINE(HAVE_PTHREAD_H))
if test x$ac_cv_header_pthread_h = xyes; then
    # Check various threading options for the platforms we support
    for flag in -lpthreads -lpthread -pthread; do
        AC_MSG_CHECKING([for pthread_create using $flag])
        SAVELIBS="$LIBS"
        LIBS="$flag $LIBS"
        AC_TRY_LINK([#include <pthread.h>],
            [pthread_create(0, 0, 0, 0);],
            have_pthread=yes)
        AC_MSG_RESULT([$have_pthread])
        LIBS="$SAVELIBS"
        if test $have_pthread = yes; then
            PTHREAD_LIBS="$flag"
            break
        fi
    done
fi
AC_SUBST(PTHREAD_LIBS)

# Initialize automake
AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_PROG_CC_C_O

# Setup automake targets
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])

# Bundled verison of Mini-XML
MXML_VERSION=2.9
AC_SUBST(MXML_VERSION)

# Build Mini-XML as nested package
AC_CONFIG_SUBDIRS([mxml-2.9])

# Generate output files
AC_OUTPUT
