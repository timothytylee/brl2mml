AM_CFLAGS = -I $(top_builddir)/src
LDADD = $(top_builddir)/src/libbrl2mml.la

noinst_HEADERS = test_util.h

bin_PROGRAMS = brl2mml mml2brl

brl2mml_SOURCES = brl2mml.c test_util.c
brl2mml_DEPENDENCIES = $(top_builddir)/mxml-@MXML_VERSION@/libmxml.a

mml2brl_SOURCES = mml2brl.c test_util.c
mml2brl_DEPENDENCIES = $(top_builddir)/mxml-@MXML_VERSION@/libmxml.a
mml2brl_CFLAGS = -I $(top_builddir)/mxml-@MXML_VERSION@ $(AM_CFLAGS)
mml2brl_LDADD = $(top_builddir)/mxml-@MXML_VERSION@/libmxml.a $(LDADD)

MAINTAINERCLEANFILES = Makefile.in

$(top_builddir)/mxml-@MXML_VERSION@/libmxml.a:
	$(MAKE) -C $(@D) $(@F)
