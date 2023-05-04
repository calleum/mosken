#include "mosken.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void
page_init(Page page, Size size)
{
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

Payment
payment(Page page, PgItemId pgi_id)
{
    return (Payment)page_item(page, pgi_id);
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
    int upper, lower;

    upper = (int)p->pgh_upper - (int)size;
    lower = (int)p->pgh_lower + sizeof(PageItemMeta);

    item_id_set(item_id, upper, size);

    memcpy((char *)page + upper, item, size);

    p->pgh_upper = (unsigned short)upper;
    p->pgh_lower = (unsigned short)lower;
}

void
print_payment(Payment p)
{
    printf("Payment { payment_id [%d] payment_name [%s] payment_time [%u] "
           "total_cents [%u] }\n",
           p->payment_id, p->payment_name, p->payment_time, p->total_cents);
}

void
dir_page_init(Page page, Size size)
{
    PageDirectory pd = (PageDirectory)page;
    assert(size == BLKSZ);

    memset(pd, 0, size);

    PageDirEntry pde = NULL;

    pde->fpg_offset = size;
    pde->fpg_id = 0;
    memcpy((char *)page + sizeof(pd->pt), (char *)pde,
           sizeof(PageDirEntryData));
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

int
main(void)
{
    PaymentData payment_obj = { .payment_id = 1,
                                .total_cents = 2000,
                                .payment_name = "Expensive Tuna",
                                .payment_time = 1682331745 };
    Page page = malloc(BLKSZ);
    page_init(page, BLKSZ);
    OffsetNum offset_number = 1;
    add_page_item(page, (Item)&payment_obj, sizeof(PaymentData),
                  offset_number);

    char *filename = "mosken.db";

    struct stat buffer;
    if(stat(filename, &buffer) != 0)
    {
        FILE *fp = fopen(filename, "w");
        fclose(fp);
    }

    FILE *fp = fopen(filename, "r+");
    if(fp == NULL)
    {
        fprintf(stderr, "Error opening file\n");
        exit(1);
    }

    write_page(fp, 0L, (char *)page);

    Page page_2 = malloc(BLKSZ);
    read_page(fp, 0L, page_2);

    print_payment(payment(page, page_get_item_id(page, 1)));

    print_payment(payment(page_2, page_get_item_id(page_2, 1)));
    free(page);
    free(page_2);
    return EXIT_SUCCESS;
}
