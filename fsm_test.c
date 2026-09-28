/*
 * Unit checks for the FSM rung A2, first three functions only:
 * fsm_init, fsm_root_value, fsm_set_avail. fsm_search_avail is NOT
 * exercised here (fuzz harness covers it once written).
 *
 * All checks go through the public API plus the fixed fsm.h layout:
 * leaf for heap page p is the byte at FSM_NODES_START + LEAF_START + p.
 */

#include "fsm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t leaf_byte(Page page, uint32_t page_no)
{
    return ((uint8_t *)page)[FSM_NODES_START + LEAF_START + page_no];
}

int main(void)
{
    Page page = malloc(BLKSZ);

    /* 1. init: whole page zero, root reads 0 */
    memset(page, 0xAA, BLKSZ);
    fsm_init(page);
    for(uint32_t i = 0; i < BLKSZ; i++)
        assert(((uint8_t *)page)[i] == 0);
    assert(fsm_root_value(page) == 0);

    /* 2. single set reaches the root (full bubble chain, depth 11) */
    fsm_set_avail(page, 7, 42);
    assert(leaf_byte(page, 7) == 42);
    assert(fsm_root_value(page) == 42);

    /* 3. second set: root is the max of both leaves */
    fsm_set_avail(page, 100, 200);
    assert(fsm_root_value(page) == 200);

    /* 4. lowering a leaf lowers the root (early-stop must not stick) */
    fsm_set_avail(page, 100, 10);
    assert(fsm_root_value(page) == 42);
    fsm_set_avail(page, 7, 5);
    assert(fsm_root_value(page) == 10);

    /* 5. boundary leaves: page 0 and the last leaf slot stay in bounds */
    fsm_init(page);
    fsm_set_avail(page, 0, 1);
    assert(leaf_byte(page, 0) == 1);
    assert(fsm_root_value(page) == 1);
    fsm_set_avail(page, NSLOTS - 1, 2);
    assert(leaf_byte(page, NSLOTS - 1) == 2);
    assert(fsm_root_value(page) == 2);

    /* 6. category ceiling: 255 stores and reaches the root as 255 */
    fsm_set_avail(page, 0, 255);
    assert(fsm_root_value(page) == 255);

    /* 7. untouched leaves stay 0 */
    fsm_init(page);
    fsm_set_avail(page, 3, 99);
    assert(leaf_byte(page, 2) == 0);
    assert(leaf_byte(page, 4) == 0);

    free(page);
    puts("fsm-test: all checks passed");
    return 0;
}
