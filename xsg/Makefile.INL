#
# DO NOT EDIT- INL/config/system.mk file should set everything up
# See specifically X11LIB and X11INCLUDE
#
CONFIG = system.mk

include ../config/$(CONFIG)

LOCAL_DEFINES = -DSHRINKFRAME -DREALLYFAST

TARGET = xsg

HEADERS = 		Wlib.h bitmaps.h copyright.h copyright2.h data.h \
			defs.h oldbitmaps.h patchlevel.h struct.h version.h

SRCS =			colors.c data.c defaults.c dmessage.c getship.c	\
			inform.c input.c main.c modify.c newwin.c option.c \
			planetlist.c planets.c playerlist.c redraw.c \
			robotwin.c shmem.c sintab.c smessage.c stats.c \
			sysdefaults.c util.c war.c warning.c x11window.c

OBJS =			colors.o data.o defaults.o dmessage.o getship.o	\
			inform.o input.o main.o modify.o newwin.o option.o \
			planetlist.o planets.o playerlist.o redraw.o \
			robotwin.o shmem.o sintab.o smessage.o stats.o \
			sysdefaults.o util.o war.o warning.o x11window.o

LIBS = $(X11LIB) $(EXTRALIBS)
INCS = $(EXTRAINCS) $(X11INCLUDE)

# for people with braindead 'cc's that don't like -c foo.c -o foo.o
# and a hosed make
.c.o: ; $(CC) $(CFLAGS) $(INCS) -c $*.c

CFLAGS = $(EXTRACFLAGS) $(OPTS) $(LOCAL_DEFINES)

all: xsg

xsg: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	/bin/rm -f *.o make.out core
        
reallyclean:  clean 
	/bin/rm -f cor

install: xsg
	$(INSTALL) $(INSTALLOPTS) xsg $(BINDIR)/xsg

uninstall:
	-cd $(BINDIR); rm -f xsg 

depend:
	makedepend $(CFLAGS) -f Makefile $(INCS) $(SRCS)
