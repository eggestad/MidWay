
CC = gcc
 
INCLUDES = -I../include 

INCPATH = ../include

CFLAGS =  $(INCLUDES) -g

LIBS =  -L../lib -lreadline -lMWipc

LDFLAGS = $(LIBS) 

OBJ = mwadm.o commands.o

EXE = mwadm

all: $(EXE)

mwadm: $(OBJ)
	gcc -o $@ $(OBJ) $(LIBS)

depend:
	makedepend -Y *.c

clean:
	rm mwadm $(OBJ) core 

tags:
	etags -a -o ../TAGS *.c *.h

# DO NOT DELETE

mwadm.o: $(INCPATH)/MiddleWay.h mwadm.c $(INCPATH)/ipctables.h

commands.o: commands.c $(INCPATH)/MiddleWay.h  $(INCPATH)/ipctables.h $(INCPATH)/ipcmessages.h
