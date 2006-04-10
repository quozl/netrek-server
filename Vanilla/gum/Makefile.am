## Process this file with automake to produce Makefile.in

SUBDIRS =

INCLUDES = \
	@GTK_CFLAGS@ -I../include -I${srcdir}/../include

bin_PROGRAMS = gum

gum_SOURCES = \
	support.c support.h \
	main.c main.h \
	signals.c signals.h \
	id.c xmalloc.c

gum_LDADD = @GTK_LIBS@ ../ntserv/data.o 

AM_CFLAGS = @AM_CFLAGS@ -DPACKAGE_DATA_DIR=\"@prefix@\"

install-data-local:
	test -d ${srcdir}/pixmaps \
	&& install -d -m 0755 $(DESTDIR)$(pkgdatadir)/pixmaps \
	&& install -m 0644 ${srcdir}/pixmaps/*.xpm $(DESTDIR)$(pkgdatadir)/pixmaps

dist-hook:
	test -d pixmaps \
	&& mkdir $(distdir)/pixmaps \
	&& cp -p ${srcdir}/pixmaps/*.xpm $(distdir)/pixmaps

