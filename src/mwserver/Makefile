
CC = gcc
 
INCLUDES = -I../include 

CFLAGS =  $(INCLUDES) -g

all: mwserver testservices.so

clean: 
	rm mwserver *.o *.so core

test: 
	./mwserver -s test1:servicehandler1 -s servicehandler1 -s sleep1 testservices.so

tags:
	etags -a -o ../TAGS *.c *.h

testservices.o: testservices.c ../include/MiddleWay.h
	gcc -fPIC $(CFLAGS) -c $<

testservices.so: testservices.o
	gcc -shared -g -o testservices.so testservices.o

mwserver.o: ../include/MiddleWay.h ./shared_lib_services.h

shared_lib_services.o: ../include/MiddleWay.h ./shared_lib_services.h

mwserver: mwserver.o shared_lib_services.o ../lib/libMWipc.so
	gcc -o $@ mwserver.o shared_lib_services.o -rdynamic $(CFLAGS) -L../lib -lMWipc  -ldl


