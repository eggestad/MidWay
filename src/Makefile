
CC = gcc

CFLAGS = 


all: 
	(cd lib; make)
	(cd mwd; make)
	(cd mwadm; make)
	(cd mwserver; make)
	(cd test; make)

clean: 
	(cd lib; make -i clean)
	(cd mwd; make -i clean)
	(cd mwadm; make -i clean)
	(cd mwserver; make -i clean)
	(cd test; make -i clean)

tags:
	etags TAGS *.c *.h
	(cd lib; make tags)
	(cd mwd; make tags)
	(cd mwadm; make tags)
	(cd mwserver; make tags)


depend:
	makedepend -Y *.c



