
MDHOME = ../..

CC = gcc
 
LIBS = -lMWipc -lm

CFLAGS = -I../include -g # -Wstrict-prototypes

OBJS = mwd.o shmallocfmt.o requestparse.o tables.o watchdog.o


EXE = mwd

all: $(EXE)

clean:
	rm $(EXE) $(OBJS) core

mwd.o: mwd.c ../include/MiddleWay.h ../include/ipctables.h tables.h mwd.h 
mwd.o: ../include/ipcmessages.h utils.h

requestparse.o: requestparse.c ../include/MiddleWay.h  tables.h mwd.h 
requestparse.o: ../include/ipcmessages.h ../include/ipctables.h 

shmallocfmt.o: shmallocfmt.c ../include/shmalloc.h
shmallocfmt.o: ../include/MiddleWay.h ../include/ipctables.h 

tables.o: tables.c tables.h ../include/MiddleWay.h ../include/ipctables.h 

watchdog.o: watchdog.c watchdog.h 
watchdog.o: ../include/ipctables.h ../include/MiddleWay.h tables.h mwd.h

mwd: ../lib/libMWipc.a $(OBJS)
	gcc -o $@ -g $(OBJS) -L../lib $(LIBS) 

tags:
	etags -a -o ../TAGS *.c *.h

depend:
	makedepend -Y *.c

# DO NOT DELETE

