#include "mosken.h"
#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void page_init(Page page)
{
    Size size = BLKSZ;
    PgHeader p = (PgHeader)page;
    assert(size == BLKSZ);

    memset(p, 0, size);

    p->pgh_lower = size_of_page_header;
    p->pgh_upper = size;
}

Item page_item(Page page, PgItemId pgi_id)
{
    return (Item)(((char *)page) + pgi_id->pgi_offset);
}

PgItemId page_get_item_id(Page page, OffsetNum offset_number)
{
    return &((PgHeader)page)->pgh_im[offset_number - 1];
}

uint32_t page_free_space(Page page)
{
    PgHeader p = (PgHeader)page;
    return p->pgh_upper - p->pgh_lower;
}

/*
 * Slot metadata grows upward while item bytes grow downward. A new slot is
 * appended only when offset_number is the next slot number; reusing a
 * tombstone therefore changes pgh_upper but leaves pgh_lower unchanged.
 * The strict free-space assertion reserves room for a slot even on reuse,
 * matching the low-level API contract documented in mosken.h.
 */
void add_page_item(Page page, Item item, Size size, OffsetNum offset_number)
{
    PgHeader p = (PgHeader)page;
    PgItemId item_id = page_get_item_id(page, offset_number);
    uint32_t max_offset_num = page_get_max_offset_number(page);
    uint32_t upper, lower;

    assert(page_free_space(page) > (size + sizeof(PageItemMeta)));

    upper = p->pgh_upper - size;
    if(offset_number == max_offset_num + 1)
    {
        lower = p->pgh_lower + sizeof(PageItemMeta);
    }
    else
    {
        lower = p->pgh_lower;
    }

    item_id_set(item_id, upper, size);

    memcpy((char *)page + upper, item, size);

    p->pgh_upper = upper;
    p->pgh_lower = lower;
}

/**
 * Initialize the fixed-size directory-page layout.
 *
 * The buffer is cleared and its first directory entry is seeded with page
 * identifier zero and offset `size`; `size` must equal BLKSZ. It currently
 * has no declaration in the public slotted-page header, but it uses the same
 * BLKSZ-sized page-buffer convention.
 */
void dir_page_init(Page page, Size size)
{
    PageDirectory pd = (PageDirectory)page;
    assert(size == BLKSZ);

    memset(pd, 0, size);

    PageDirEntryData pde;
    pde.fpg_offset = size;
    pde.fpg_id = 0;

    pd->pds[0] = pde;
}

/*
 * Tombstones use a zero physical offset. Every valid live item is stored
 * after the page header, so zero cannot be a live item's offset. The length
 * is left untouched because the offset alone encodes liveness; checking the
 * offset (rather than the length) keeps a live zero-length item possible.
 * The item bytes and both page boundaries are intentionally left untouched;
 * page_compact() is the operation that reclaims them.
 */
void page_delete_item(Page page, OffsetNum offset_number)
{
    PgItemId item_id = page_get_item_id(page, offset_number);

    item_id->pgi_offset = 0;
}

/*
 * Rebuild into a separate page so an item's source bytes cannot be
 * overwritten while earlier live items are copied. Iterating the original
 * slot directory preserves order; the fresh page naturally drops tombstones
 * and assigns contiguous slot numbers.
 */
void page_compact(Page page)
{
    uint32_t max_offset_num = page_get_max_offset_number(page);

    Page new_page = malloc(BLKSZ);
    page_init(new_page);

    for(uint32_t i = 0; i < max_offset_num; i++)
    {
        if(page_item_is_live(page, i + 1))
        {
            PgItemId item_id = page_get_item_id(page, i + 1);
            Size size = item_id->pgi_length;
            Item item = page_item(page, item_id);
            add_page_item(new_page, item, size,
                          page_get_max_offset_number(new_page) + 1);
        }
    }

    memcpy((char *)page, new_page, BLKSZ);
    free(new_page);
}

/*
 * Liveness is encoded independently of length: offset zero is the
 * tombstone marker, so an item with length zero is still live when its
 * physical offset is non-zero.
 */
int page_item_is_live(Page page, OffsetNum offset_number)
{
    PgItemId item_id = page_get_item_id(page, offset_number);

    return (item_id->pgi_offset != 0);
}
