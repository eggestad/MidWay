
CC = gcc
INSTALL = /usr/bin/install

LIBS = -lMidWay -lurlencode -lpthread

CFLAGS = -I../include  -g  -Wstrict-prototypes

LDFLAGS = -L../lib -L../liburlencode

EXE = mwgwd

OBJS = SRBprotocolServer.o connections.o tcpserver.o gateway.o store.o

all: $(EXE)

clean:
	rm -f $(EXE) $(OBJS) core store

install:
	$(INSTALL) -D mwgwd $(INSTALLDIR)/bin

uninstall:
	rm -f $(INSTALLDIR)/bin/mwgwd

tags:
	etags -a -o ../TAGS *.c *.h

mwgwd: $(OBJS)
	gcc $(LDFLAGS) -o $@ $(OBJS) -lefence $(LIBS)

# used for testing store module.
store: store.c store.h 
	 gcc -g -DMODULETEST -o store store.c -I../include -L../lib -L../liburlencode/ -lurlencode -lMidWay

SRBprotocolServer.o: SRBprotocolServer.c tcpserver.h gateway.h ../include/SRBprotocol.h

connections.o: connections.c

tcpserver.o: tcpserver.c tcpserver.h gateway.h ../include/SRBprotocol.h

gateway.o: gateway.c gateway.h store.h 

store.o: store.c store.h 
