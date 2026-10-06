#ifndef FSM_H
#define FSM_H

#include "mosken.h"

/*
 * Single-page free space map, mirroring PostgreSQL's FSM design
 * (src/backend/storage/freespace/README) at one FSM page.
 *
 * Layout of one BLKSZ page:
 *   bytes 0..3   reserved (PG: PageHeaderData)
 *   bytes 4..7   next-slot pointer, u32 LE (PG: fp_next_slot)
 *   bytes 8..    node array, one byte per node (PG: fp_nodes[])
 *
 * The node array holds a binary tree in level order: slot 0 is the root,
 * children of slot i live at 2i+1 and 2i+2, parent at (i-1)/2. Leaves sit
 * at the array tail, one per heap page. 4088 node bytes hold a complete
 * tree over NSLOTS leaves; the tree is complete above the leaves, and a
 * rightmost run of leaf slots may be unused (kept 0, never returned by
 * search since any min_cat >= 1 rejects 0).
 */
#define FSM_NODES_START 8
#define NSLOTS ((BLKSZ - FSM_NODES_START + 1) / 2) /* 2044 leaves */
#define LEAF_START (NSLOTS - 1)                    /* 2043 */
#define FSM_NODES (2 * NSLOTS - 1)                 /* 4087 nodes, +1 spare */

void fsm_init(Page page);
void fsm_set_avail(Page page, uint32_t page_no, uint8_t cat);
int fsm_search_avail(Page page, uint8_t min_cat, uint32_t *page_no_out);
uint8_t fsm_root_value(Page page);

/*
 * Category arithmetic, mirroring PostgreSQL's fsm_space_avail_to_cat and
 * fsm_space_needed_to_cat. CAT_UNITS = 16 (BLKSZ/256).
 *
 *   fsm_space_avail_to_cat(free_bytes)
 *     Category stored for a page with `free_bytes` contiguous free bytes.
 *     Rounds DOWN, capped at 255. Storing must never over-promise.
 *
 *   fsm_space_needed_to_cat(need_bytes)
 *     Minimum category a page must advertise to serve an item of
 *     `need_bytes`. Rounds UP. A category computed with floor here accepts
 *     a page whose free run is one boundary byte short of the need.
 */
uint8_t fsm_space_avail_to_cat(uint16_t free_bytes);
uint8_t fsm_space_needed_to_cat(uint16_t need_bytes);

#endif /* FSM_H */
