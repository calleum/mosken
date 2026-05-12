#include "mosken.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
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

void add_page_item(Page page, Item item, Size size, OffsetNum offset_number)
{
    PgHeader p = (PgHeader)page;
    PgItemId item_id = page_get_item_id(page, offset_number);
    uint32_t upper, lower;

    assert(page_free_space(page) > (size + sizeof(PageItemMeta)));

    upper = p->pgh_upper - size;
    lower = p->pgh_lower + sizeof(PageItemMeta);

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
