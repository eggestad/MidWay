
CC = gcc
 
INCLUDES = -I../include 

CFLAGS =  $(INCLUDES) -g

all: testsuiteserver.so testsuiteclient

clean: 
	rm mwserver *.o *.so

test: 
	runtestsuite.pl

tags:
	etags -a -o ../TAGS *.c *.h

testsuiteserver.o: testsuiteserver.c ../include/MidWay.h testsuite.h
	gcc -fPIC $(CFLAGS) -c $<

testsuiteserver.so: testsuiteserver.o
	gcc -shared -g -o $@ $<


testsuitclient.o: testsuiteclient.c ../include/MidWay.h testsuite.h
	gcc $(CFLAGS) -c $<

testsuiteclient: testsuiteclient.o
	gcc -o $@ testsuiteclient.o -L../lib -lMWipc

