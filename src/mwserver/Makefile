
CC = gcc
 
INCLUDES = -I../include 

LIBS = -L../lib -lMidWay

CFLAGS =  $(INCLUDES) -g 

all: mwserver testservices.so

clean: 
	rm -f mwserver *.o *.so core

test: 
	./mwserver -s test1:servicehandler1 -s servicehandler1 -s sleep1 testservices.so

tags:
	etags -a -o ../TAGS *.c *.h

testservices.o: testservices.c ../include/MidWay.h
	gcc -fPIC $(CFLAGS) -c $<

testservices.so: testservices.o
	gcc -shared -g -o testservices.so testservices.o

mwserver.o: ../include/MidWay.h ./shared_lib_services.h

shared_lib_services.o: ../include/MidWay.h ./shared_lib_services.h

mwserver: mwserver.o shared_lib_services.o ../lib/libMidWay.so
	gcc -o $@ mwserver.o shared_lib_services.o -rdynamic $(CFLAGS) $(LIBS) -ldl


