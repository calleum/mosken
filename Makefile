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

fuzz: fuzz_page.c mosken.c mosken.h
	$(CC) $(CFLAGS) -o fuzz fuzz_page.c mosken.c
	./fuzz

fuzz-fsm: fuzz_fsm.c fsm.h mosken.c mosken.h heapfile.c heapfile.h fsm.c
	$(CC) $(CFLAGS) -o fuzz-fsm fuzz_fsm.c mosken.c heapfile.c fsm.c
	./fuzz-fsm

# Standalone FSM stub check (re-gate red baseline): compiles fuzz_fsm.c
# against throwaway stubs outside the repo, runs it, must abort with a
# real contract violation (not a link error).
fsm-stub-check:
	@mkdir -p /tmp/fsm-stub && printf '\043include "fsm.h"\n\043include <stdint.h>\nvoid fsm_init(Page p){(void)p;} void fsm_set_avail(Page p,uint32_t n,uint8_t c){(void)p;(void)n;(void)c;} int fsm_search_avail(Page p,uint8_t m,uint32_t*o){(void)p;(void)m;(void)o;return 1;} uint8_t fsm_root_value(Page p){(void)p;return 0;}\nuint8_t fsm_space_avail_to_cat(uint16_t f){return (uint8_t)(f/16u);} uint8_t fsm_space_needed_to_cat(uint16_t n){return (uint8_t)(n/16u);}\n' > /tmp/fsm-stub/fsm_stub.c
	$(CC) $(CFLAGS) -I. -o /tmp/fsm-stub/fuzz-stub fuzz_fsm.c mosken.c heapfile.c /tmp/fsm-stub/fsm_stub.c
	@/tmp/fsm-stub/fuzz-stub; rc=$$?; if [ $$rc -ne 0 ]; then echo "stub check: aborted rc=$$rc (expected: contract violation)"; else echo "stub check: PASSED with stubs (BAD: stubs satisfy the checker)"; exit 1; fi

.PHONY: clean
clean:
	rm -f mosken test fuzz fuzz-fsm
