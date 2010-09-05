CFLAGS = -g -O0 -Wall -Wno-parentheses

%.a:
	ar rcs $@ $^

all: rx.a rxtry t

t: rx.a FORCE
	make -C t/

rx.a: rx.o handy.o list.o state.o cursor.o parser.o matcher.o
rx.o: rx.c rx.h rxpriv.h
handy.o: handy.c rx.h rxpriv.h
list.o: list.c rx.h rxpriv.h
state.o: state.c rx.h rxpriv.h
cursor.o: cursor.c rx.h rxpriv.h
parser.o: parser.c rx.h rxpriv.h
matcher.o: matcher.c rx.h rxpriv.h

rxtry: rxtry.o rx.a
rxtry.o: rxtry.c rx.h

test: t
	./t/test

memcheck:
	valgrind --leak-check=yes ./t/test

clean:
	rm -fv *.o rx.a rxtry
	make -C t/ clean

FORCE:

