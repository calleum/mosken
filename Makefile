CFLAGS=-Wall -Wextra -Werror -std=c18 -pedantic


mosken: mosken.c
	$(CC) $(CFLAGS) -o mosken mosken.c

build: mosken

run: build 
	./mosken

.PHONY: clean
clean: 
	rm -f mosken
