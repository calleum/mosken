CFLAGS=-Wall -Wextra -Werror -std=c18 -pedantic -g3 -fsanitize=address,undefined -Wconversion -Wdouble-promotion -Wno-sign-conversion


mosken: mosken.c mosken.h
	$(CC) $(CFLAGS) -o mosken mosken.c

build: mosken

run: build 
	./mosken

.PHONY: clean
clean: 
	rm -f mosken
