
CC = gcc
INSTALL = /usr/bin/install
 
INCLUDES = -I../include 

INCPATH = ../include

CFLAGS =  $(INCLUDES) -g

LIBS =  -L../lib -lMidWay

LDFLAGS = $(LIBS) 

OBJ = mwcall.o

EXE = mwcall

all: $(EXE)

$(EXE): $(OBJ)
	gcc -o $@ $(OBJ) $(LIBS)

depend:
	makedepend -Y *.c

clean:
	rm -f mwcall $(OBJ) core 

install:
	$(INSTALL) -D mwcall $(INSTALLDIR)/bin/mwcall

uninstall:
	rm -f $(INSTALLDIR)/bin/mwcall

tags:
	etags -a -o ../TAGS *.c *.h

# DO NOT DELETE

mwcall.o: $(INCPATH)/MidWay.h mwcall.c 

