
CC = gcc
INSTALL = /usr/bin/install
 
INCLUDES = -I../include 

INCPATH = ../include

CFLAGS =  $(INCLUDES) -g

LIBS =  -L../lib -lreadline -ncurses -lMidWay

LDFLAGS = $(LIBS) 

OBJ = mwadm.o commands.o

EXE = mwadm

all: $(EXE)

mwadm: $(OBJ)
	gcc -o $@ $(OBJ) $(LIBS)

depend:
	makedepend -Y *.c

clean:
	rm -f mwadm $(OBJ) core 

install:
	$(INSTALL) -D mwadm $(INSTALLDIR)/bin/mwadm

uninstall:
	rm -f $(INSTALLDIR)/bin/mwadm

tags:
	etags -a -o ../TAGS *.c *.h

# DO NOT DELETE

mwadm.o: $(INCPATH)/MidWay.h mwadm.c $(INCPATH)/ipctables.h

commands.o: commands.c $(INCPATH)/MidWay.h  $(INCPATH)/ipctables.h $(INCPATH)/ipcmessages.h
