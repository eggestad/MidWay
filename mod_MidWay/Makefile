APXS = apxs

.SUFFIXES: .c .o .so

INC=-I../include

LIB=-L../src/lib -lMidWay 

MODULES = mod_midway.so 

all: $(MODULES)

.c.so: $*.c
	$(APXS) -c $(DEF) $(INC) $(LIB) $*.c

install: all
	@for i in $(MODULES); do \
		cp mod_midway.so /usr/lib/apache/mod_midway.so; \
		chmod 755 /usr/lib/apache/mod_midway.so; \
	done

firstinstall: all
	@for i in $(MODULES); do \
		$(APXS) -i -a $$i; \
	done


clean:
	rm -f *.o *.so

distclean:
	rm -f Makefile *~
