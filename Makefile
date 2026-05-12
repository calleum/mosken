CFLAGS=-Wall -Wextra -Werror -std=c18 -pedantic -g3 -fsanitize=address,undefined -Wconversion -Wdouble-promotion -Wno-sign-conversion


mosken: mosken.c mosken.h heapfile.c heapfile.h
	$(CC) $(CFLAGS) -o mosken mosken.c heapfile.c main.c

build: mosken

run: build 
	./mosken

.PHONY: clean
clean: 
	rm -f mosken test

test: test.c test.h mosken.c mosken.h heapfile.c heapfile.h
	$(CC) $(CFLAGS) -o test test.c mosken.c heapfile.c
	./test
