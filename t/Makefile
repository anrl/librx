CPPFLAGS = -I..
CFLAGS = -g -O0 -Wall -Wno-parentheses
LDLIBS = -ltap

test: test.o ../rx.a
test.o: test.c ../rx.h

clean:
	rm -fv *.o test

