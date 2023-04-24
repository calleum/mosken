CFLAGS=-Wall -Wextra -Werror -std=c18 -pedantic
INPUTS=sample-01.txt sample-02.txt input.txt

.PHONY: test
test: main $(INPUTS)
	./main $(INPUTS)

mosken: mosken.c
	$(CC) $(CFLAGS) -o mosken mosken.c


build: mosken

run: build 
	./mosken


