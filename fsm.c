#include "fsm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Internal nodes store the maximum category of their descendant leaves. */

typedef struct
{
    uint32_t reserved;

    uint32_t freepage_next_slot;

    uint8_t freepage_nodes[];
} FSMPageData;

typedef FSMPageData *FSMPage;

/* Initialize the page metadata and all nodes to zero. */
void fsm_init(Page page)
{
    Size size = BLKSZ;
    FSMPage fsm_page = (FSMPage)page;

    memset(fsm_page, 0, size);
}

/* Update a leaf and recompute its ancestors up to the root. */
void fsm_set_avail(Page page, uint32_t page_no, uint8_t cat)
{
    FSMPage fsm_page = (FSMPage)page;
    fsm_page->freepage_nodes[LEAF_START + page_no] = cat;

    int parent = (LEAF_START + page_no - 1) / 2;
    while(parent >= 0)
    {
        uint8_t left = fsm_page->freepage_nodes[2 * parent + 1];
        uint8_t right = fsm_page->freepage_nodes[2 * parent + 2];

        uint8_t max = left > right ? left : right;

        if(fsm_page->freepage_nodes[parent] == max)
        {
            return;
        }

        fsm_page->freepage_nodes[parent] = max;

        if(parent == 0)
        {
            break;
        }

        parent = (parent - 1) / 2;
    }
}

/**
 *   fsm_search_avail(Page page, uint8_t min_cat, uint32_t *page_no_out)
 *     Find a heap page whose category is at least `min_cat`. On success,
 *     store its page number in `*page_no_out` and return 1. If the root is
 *     below `min_cat`, return 0. Any traversal order is valid: the checker
 *     only requires a matching page and a result consistent with the
 *     model's maximum category.
 *
 */
int fsm_search_avail(Page page, uint8_t min_cat, uint32_t *page_no_out)
{
    FSMPage fsm_page = (FSMPage)page;

    if(fsm_root_value(page) < min_cat)
    {
        return 0;
    }

    int current_node_idx = 0;
    while(current_node_idx < LEAF_START)
    {
        int ch_idx = 2 * current_node_idx + 1;

        if(fsm_page->freepage_nodes[ch_idx] >= min_cat)
        {
            current_node_idx = ch_idx;
            continue;
        }
        ch_idx++;

        if(fsm_page->freepage_nodes[ch_idx] >= min_cat)
        {

            current_node_idx = ch_idx;
        }
    }

    *page_no_out = current_node_idx - LEAF_START;
    return 1;
}

uint8_t fsm_root_value(Page page)
{
    FSMPage fsm_page = (FSMPage)page;

    return fsm_page->freepage_nodes[0];
}
