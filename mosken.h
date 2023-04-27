#ifndef MOSKEN_H
#define MOSKEN_H

#include <stddef.h>
#define BLKSZ 4096 // size of a db block in bytes

#define size_of_page_header (offsetof (PgHeaderData, pgh_im))
#define item_id_set(item_id, off, len)                                        \
    ((item_id)->pgi_offset = (off), (item_id)->pgi_length = (len))
#define DIRECTORY_PAGE 1
#define DATA_PAGE 3

typedef struct
{
    int payment_id;
    char payment_name[24];
    unsigned long payment_time;
    unsigned long total_cents;
} PaymentData;

typedef PaymentData *Payment;

typedef unsigned short Offset;

typedef struct
{
    Offset pgi_offset; /* offset into the page to the start of the item */
    unsigned short pgi_length; /* length of the item in the page */
} PageItemMeta;

typedef PageItemMeta *PgItemId;

typedef struct
{
    unsigned short pgh_lower; /* offset to the start of free space in the page */
    unsigned short pgh_upper; /* offset to the end of free space in the page */
    PageItemMeta pgh_im[];    /* item metadata array */
} PgHeaderData;

typedef PgHeaderData *PgHeader;

typedef unsigned long PageId;

typedef struct
{
    PageId fpg_id;
    Offset fpg_offset;
} PageDirEntryData;

typedef PageDirEntryData *PageDirEntry;
typedef unsigned char PageFlags;
typedef struct
{
    PageFlags pt;
    PageDirEntryData pds[];
} PageDirectoryData;

typedef PageDirectoryData *PageDirectory;

typedef char *Page;
typedef size_t Size;

typedef char *Item;
typedef unsigned short OffsetNum;

#endif // !MOSKEN_H
