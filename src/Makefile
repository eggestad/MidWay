
CC = gcc

CFLAGS = -g -Wstrict-prototypes

DIRS = liburlencode lib mwd mwadm mwserver mwgwd test


all: 
	@ for dir in $(DIRS); do\
	(cd $$dir; make $@)\
	done;

clean: 
	@ for dir in $(DIRS); do\
	(cd $$dir; make $@)\
	done;

tags:
	@ for dir in $(DIRS); do\
	(cd $$dir; make $@)\
	done;


depend:
	makedepend -Y *.c



