
CC = gcc
INSTALL = /usr/bin/install

CFLAGS = -g -Wstrict-prototypes

DIRS = liburlencode lib mwd mwadm mwserver mwgwd mwcall 

INSTALLDIR=/usr/local
export INSTALLDIR

all: 
	@ for dir in $(DIRS); do\
	(cd $$dir; make $@)\
	done;

clean: 
	@ for dir in $(DIRS); do\
	(cd $$dir; make $@)\
	done;

tags:
	rm TAGS
	@ for dir in $(DIRS); do\
	(cd $$dir; make $@)\
	done;
	etags -a -o TAGS include/*.h

test:
	(cd $@; make $@)

depend:
	makedepend -Y *.c


install:
	@ for dir in $(DIRS); do\
	(cd $$dir; make  $@)\
	done;
	$(INSTALL) ../include/MidWay.h $(INSTALLDIR)/include/MidWay.h

uninstall:
	@ for dir in $(DIRS); do\
	(cd $$dir; make $@)\
	done;

