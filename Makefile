include Makefile.sets

# Building for Windows (opt/xmingw is for Gentoo):
#HOST=/usr/local/mingw32/bin/i586-mingw32msvc-
#HOST=/opt/xmingw/bin/i386-mingw32msvc-
##LDOPTS = -L/usr/local/mingw32/lib
#LDOPTS += -L/opt/xmingw/lib

# Building for native:
#HOST=
#LDFLAGS += -pthread

# Исправлено: Собрали все флаги линковки вместе.
# Убедитесь, что в системе установлен пакет 'mold'
LDFLAGS=-static -static-libgcc -Wl,--gc-sections -fuse-ld=mold

CXX=$(HOST)g++
CC=$(HOST)gcc
CPP=$(HOST)gcc

# Исправлено: Обновили предварительный c++1y до официального c++14
CXXFLAGS += -std=c++23
CPPFLAGS += -I.
VERSION=1.2.4

ARCHFILES=COPYING Makefile.sets progdesc.php \
          main.cc \
          libregex.cc libregex.hh \
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

# Исправлено: Поставили LDFLAGS перед файлами объектов ($^),
# чтобы избежать ошибок линковки при использовании -static
regex-opt: main.o libregex.a
	$(CXX) $(CXXFLAGS) -g $(LDFLAGS) -o $@ $^

libregex.a: libregex.o
	ar -rc $@ $^

clean: FORCE
	rm -f *.o $(PROGS) libregex.a
distclean: clean
	rm -f *~ .depend
realclean: distclean

include depfun.mak

.PHONY: all clean distclean realclean

FORCE: ;
