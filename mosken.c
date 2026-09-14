#include "mosken.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
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

void page_add_item(Page page, Item item, Size size, OffsetNum offset_number)
{
    PgHeader p = (PgHeader)page;
    PgItemId item_id = page_get_item_id(page, offset_number);
    uint32_t upper, lower;

    uint32_t slot_count = (p->pgh_lower - (uint32_t)size_of_page_header)
                          / (uint32_t)sizeof(PageItemMeta);

    assert(page_free_space(page) > (size + sizeof(PageItemMeta)));

    upper = p->pgh_upper - size;
    if(offset_number > slot_count)
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
 * Tombstone the slot at offset_number. The item's bytes stay in place;
 * pgh_lower and pgh_upper do not move.
 */
void page_delete_item(Page page, OffsetNum offset_number)
{
    PgItemId item_id = page_get_item_id(page, offset_number);
    item_id_set(item_id, 0, 0);
}

/*
 * Reclaim dead slots and stranded bytes. Live items keep their relative
 * order and are renumbered 1..n with no gaps.
 */
void page_compact(Page page)
{
    PgHeader p = (PgHeader)page;
    Page fresh_page = malloc(BLKSZ);
    page_init(fresh_page);
    OffsetNum offset_number = 1;

    uint32_t count = (p->pgh_lower - (uint32_t)size_of_page_header)
                     / (uint32_t)sizeof(PageItemMeta);

    for(uint32_t i = 0; i < count; i++)
    {
        PgItemId item_id = page_get_item_id(page, i + 1);
        if(!page_item_is_live(page, i + 1))
            continue;
        page_add_item(fresh_page, page_item(page, item_id),
                      item_id->pgi_length, offset_number++);
    }
    memcpy(page, fresh_page, BLKSZ);
    free(fresh_page);
}

/*
 * Check whether the slot at offset_number is live.
 * Returns 1 if the slot is live, or 0 if tombstoned.
 */
int page_item_is_live(Page page, OffsetNum offset_number)
{
    PgItemId item_id = page_get_item_id(page, offset_number);
    return item_id->pgi_offset != 0;
}
