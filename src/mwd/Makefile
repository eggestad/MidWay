
MDHOME = ../..

CC = gcc
 
LIBS = -lMidWay -lm

CFLAGS = -I../include -I../liburl -g  -Wstrict-prototypes -DINTERVAL=1200

OBJS = mwd.o shmallocfmt.o requestparse.o tables.o watchdog.o


EXE = mwd

all: $(EXE) 

clean:
	rm -f $(EXE) $(OBJS) core

mwd: ../lib/libMidWay.a $(OBJS)
	$(CC) -o $@ -g $(OBJS) -L../lib $(LIBS) 

tags:
	etags -a -o ../TAGS *.c *.h

depend:
	makedepend -Y *.c

# DO NOT DELETE

mwd.o: mwd.c ../include/MidWay.h ../include/ipctables.h tables.h mwd.h 
mwd.o: ../include/ipcmessages.h utils.h

requestparse.o: requestparse.c ../include/MidWay.h  tables.h mwd.h 
requestparse.o: ../include/ipcmessages.h ../include/ipctables.h 

shmallocfmt.o: shmallocfmt.c ../include/shmalloc.h
shmallocfmt.o: ../include/MidWay.h ../include/ipctables.h 

tables.o: tables.c tables.h ../include/MidWay.h ../include/ipctables.h 

watchdog.o: watchdog.c watchdog.h 
watchdog.o: ../include/ipctables.h ../include/MidWay.h tables.h mwd.h
