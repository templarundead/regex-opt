include Makefile.sets

# Building for Windows (opt/xmingw is for Gentoo):
#HOST=/usr/local/mingw32/bin/i586-mingw32msvc-
#HOST=/opt/xmingw/bin/i386-mingw32msvc-
##LDOPTS = -L/usr/local/mingw32/lib
#LDOPTS += -L/opt/xmingw/lib

# Building for native:
#HOST=
#LDFLAGS += -pthread 

CXX=$(HOST)g++
CC=$(HOST)gcc
CPP=$(HOST)gcc

OPTIM=-O3

CPPFLAGS += -I.

VERSION=1.1.1
ARCHFILES=COPYING Makefile.sets progdesc.php \
          regex.cc \
          autoptr \
          range.hh range.tcc \
          rangeset.hh rangeset.tcc

PREFIX=/usr/local
BINDIR=$(PREFIX)/bin

ARCHNAME=regex-opt-$(VERSION)
ARCHDIR=archives/

PROGS=regex-opt

INSTALLPROGS=regex-opt
INSTALL=install

all: $(PROGS)

regex-opt: regex.o
	$(CXX) $(CXXFLAGS) -g -o $@ $^ \
		$(LDFLAGS)

clean: FORCE
	rm -f *.o $(PROGS)
distclean: clean
	rm -f *~ .depend
realclean: distclean


include depfun.mak

.PHONY: all clean distclean realclean

FORCE: ;
