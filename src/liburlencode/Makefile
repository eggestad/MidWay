
LIBS = liburlencode.a # liburlencode.so 

all:  $(LIBS)  test-liburlencode

xall:  $(LIBS) liburl++.a test-liburlencode

clean: 
	rm -f $(LIBS) *.o

tags:
	etags -a -o ../TAGS *.c *.h

CFLAGS=-g -Wall -I../include

COBJS=urlencoded.o urlfieldlist.o urlmap.o

liburlencode.a: $(COBJS)
	ar qcs $@ $(COBJS)

liburlencode.so:
	gcc -shared -o $@ $(COBJS)

test-liburlencode: liburlencode.a test-liburlencode.o
	gcc -o $@ test-liburlencode.o -L. -lurlencode

urlencoded.o: urlencoded.c ../include/urlencode.h

urlmap.o: urlmap.c ../include/urlencode.h

urlfieldlist.o: urlfieldlist.c ../include/urlencode.h

liburl++.a: urlencoded++.o
	ar qcs $@ urlencoded++.o

urlencoded++.o: urlencoded.c
	gcc -xc++ -c -g -o $@ urlencoded.c

