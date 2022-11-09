CC=cc
PREFIX=/usr/local
CFLAGS=-std=c99 -Werror -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L -flto -D_GNU_SOURCE -fPIC
CWARN=-Wall -Wextra
EXTRA=
G=-ggdb
O=-O0
LIBS=-lgc
ALL_FLAGS=$(CFLAGS) $(EXTRA) $(CWARN) $(G) $(O) $(LIBS)

all: libintern.so

libintern.so: intern.c
	cc $(ALL_FLAGS) -shared -Wl,-soname,libintern.so -o $@ $^ -lgc

clean:
	rm -f libintern.so

test: test.c libintern.so
	cc $(ALL_FLAGS) -Wl,-rpath,$(PWD)/ -L$(PWD)/ -lintern test.c -o test
	./test

profile: profile.c libintern.so
	cc $(ALL_FLAGS) -L./ -lintern profile.c -o profile
	./profile 10 1000

install: libintern.so
	mkdir -p -m 755 "$(PREFIX)/lib" "$(PREFIX)/include"
	cp libintern.so "$(PREFIX)/lib"
	cp intern.h "$(PREFIX)/include"

.PHONY: all clean install
