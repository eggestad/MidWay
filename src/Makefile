
CC = gcc

CFLAGS = -g -Wstrict-prototypes

DIRS = liburlencode lib mwd mwadm mwserver mwgwd mwcall test


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


depend:
	makedepend -Y *.c



