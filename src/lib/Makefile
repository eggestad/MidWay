
CC = gcc
 
INCLUDES = -I../include 

CFLAGS =  $(INCLUDES) -g

IPCOBJS = mwclientipcapi.o ipctables.o ipcmessages.o mwserverapi.o shmalloc.o osdep.o

COMMONOBJS = mwlog.o version.o address.o

LIBS = libMWipc.a libMWipc.so

all: $(LIBS) testmiddleway

clean: 
	rm $(LIBS) $(IPCOBJS) $(COMMONOBJS) testmiddleway core 

tags:
	etags -a -o ../TAGS *.c *.h

libMWipc.a: $(IPCOBJS) $(COMMONOBJS)
	ar qcs $@ $(IPCOBJS) $(COMMONOBJS)

libMWipc.so: $(IPCOBJS) $(COMMONOBJS)
	gcc -shared -o $@ $(IPCOBJS) $(COMMONOBJS)

#	ld -Bshareable -g -o $@ $(IPCOBJS) $(COMMONOBJS)

address.o: address.c

ipctables.o: ipctables.c ../include/MiddleWay.h ../include/ipctables.h 

ipcmessages.o: ipcmessages.c ../include/MiddleWay.h ../include/ipcmessages.h 

shmalloc.o: shmalloc.c ../include/shmalloc.h ../include/MiddleWay.h  
	gcc -o shmalloc-m.o -c  shmalloc.c $(CFLAGS) 
	ld -i -o shmalloc.o shmalloc-m.o -lm

mwlog.o: mwlog.c ../include/MiddleWay.h

mwclientipcapi.o: mwclientipcapi.c ../include/MiddleWay.h 
mwclientipcapi.o: ../include/ipctables.h  ../include/ipcmessages.h ../include/address.h 
	gcc -o mwclientipcapi-m.o -c mwclientipcapi.c $(CFLAGS) 
	ld -i -o mwclientipcapi.o mwclientipcapi-m.o -lm

mwserverapi.o: mwserverapi.c ../include/MiddleWay.h 
mwserverapi.o: ../include/ipctables.h  ../include/ipcmessages.h 

version.o: version.c

testmiddleway: testmiddleway.c ../include/MiddleWay.h libMWipc.a
	gcc -o $@ testmiddleway.c -I../include -L. -lMWipc -lm 


