
CC = gcc
 
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
	rm -f mwadm $(OBJ) core 

tags:
	etags -a -o ../TAGS *.c *.h

# DO NOT DELETE

mwcall.o: $(INCPATH)/MidWay.h mwcall.c 

