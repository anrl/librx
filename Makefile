CFLAGS = -g -O0 -Wall -Wno-parentheses -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

%.a:
	$(AR) rcs $@ $(filter %.o, $^)

all: rx.a rxtry rxdot t/test

rx.a: rx.o handy.o list.o state.o assertions.o parser.o matcher.o charclass.o
rx.o: rx.c rx.h rxpriv.h
handy.o: handy.c rx.h rxpriv.h
list.o: list.c rx.h rxpriv.h
state.o: state.c rx.h rxpriv.h
parser.o: parser.c rx.h rxpriv.h
matcher.o: matcher.c rx.h rxpriv.h
assertions.o: assertions.c rx.h rxpriv.h
charclass.o: charclass.c rx.h rxpriv.h

rxtry: rxtry.o rx.a
rxtry.o: rxtry.c rx.h

rxdot: rxdot.o rx.a
rxdot.o: rxdot.c rx.h

t/test: t/test.o t/tap.o rx.a
t/test.o: t/test.c rx.h t/tap.h
t/tap.o: t/tap.c t/tap.h

test: t/test
	./t/test

memcheck:
	valgrind --leak-check=yes ./t/test

clean:
	rm -fv *.o rx.a rxtry rxdot t/*.o t/test

