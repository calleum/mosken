#include "mosken.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void
page_init(Page page)
{
    Size size = BLKSZ;
    PgHeader p = (PgHeader)page;
    assert(size == BLKSZ);

    memset(p, 0, size);

    p->pgh_lower = size_of_page_header;
    p->pgh_upper = size;
}

Item
page_item(Page page, PgItemId pgi_id)
{
    return (Item)(((char *)page) + pgi_id->pgi_offset);
}

PgItemId
page_get_item_id(Page page, OffsetNum offset_number)
{
    return &((PgHeader)page)->pgh_im[offset_number - 1];
}

void
add_page_item(Page page, Item item, Size size, OffsetNum offset_number)
{
    PgHeader p = (PgHeader)page;
    PgItemId item_id = page_get_item_id(page, offset_number);
    uint32_t upper, lower;

    upper = p->pgh_upper - size;
    lower = p->pgh_lower + sizeof(PageItemMeta);

    item_id_set(item_id, upper, size);

    memcpy((char *)page + upper, item, size);

    p->pgh_upper = upper;
    p->pgh_lower = lower;
}
void
dir_page_init(Page page, Size size)
{
    PageDirectory pd = (PageDirectory)page;
    assert(size == BLKSZ);

    memset(pd, 0, size);

    PageDirEntryData pde;

    pde.fpg_offset = size;
    pde.fpg_id = 0;

    pd->pds[0] = pde;
}

int
check_file_offset_bound(FILE *stream, long offset)
{
    long file_sz;

    fseek(stream, 0L, SEEK_END);
    if(ferror(stream))
    {
        perror("Error seeking to end of the page file");
        return -1;
    }

    file_sz = ftell(stream);
    if(ferror(stream))
    {
        perror("Error getting the position in the page file");
        return -1;
    }

    if(offset > file_sz)
    {
        fprintf(stderr, "Offset of the page is invalid.\n");
        return -1;
    }

    return 1;
}

void
read_page(FILE *stream, PageId pg_id, char *pg_data)
{
    long offset = pg_id * BLKSZ;

    if(check_file_offset_bound(stream, offset) == -1)
        return;

    fseek(stream, offset, SEEK_SET);
    if(ferror(stream))
        perror("Error seeking to offset in page file");

    fread(pg_data, BLKSZ, 1, stream);

    if(ferror(stream))
        perror("Error reading page from file");
}

void
write_page(FILE *stream, PageId pg_id, char *pg_data)
{
    long offset = pg_id * BLKSZ;

    if(check_file_offset_bound(stream, offset) == -1)
        return;

    fseek(stream, offset, SEEK_SET);
    if(ferror(stream))
        perror("Error seeking to the offset in the page file");

    fwrite(pg_data, BLKSZ, 1, stream);

    if(ferror(stream))
        perror("Error writing page to file");
}
