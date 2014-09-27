AM_CFLAGS = -I $(top_builddir)/mxml-2.8

include_HEADERS = brl2mml.h
noinst_HEADERS = private.h

lib_LTLIBRARIES = libbrl2mml.la

libbrl2mml_la_SOURCES = \
    to_ukmaths.c \
    from_ukmaths.c \
    mxml_util.c \
    common.c
libbrl2mml_la_DEPENDENCIES = $(top_builddir)/mxml-2.8/libmxml.a
libbrl2mml_la_LDFLAGS = \
    -Wl,$(top_builddir)/mxml-2.8/libmxml.a \
    -Wl,--retain-symbols-file=brl2mml.sym \
    -export-symbols brl2mml.sym \
    -no-undefined \
    -version-info @SO_VERSION@

MAINTAINERCLEANFILES = Makefile.in

$(top_builddir)/mxml-2.8/libmxml.a:
	$(MAKE) -C $(@D) $(@F)
