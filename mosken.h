#ifndef MOSKEN_H
#define MOSKEN_H

#include <stddef.h>
#include <stdint.h>

/**
 * In-memory representation and primitive operations for a fixed-size
 * slotted page.
 *
 * A page grows its slot directory from the beginning and its item storage
 * from the end. The interval [pgh_lower, pgh_upper) is the currently unused
 * gap between them. Deleted items leave their bytes stranded until
 * page_compact() is called.
 *
 * Values of type Offset are byte offsets from the beginning of a page.
 * Callers must provide a writable buffer of at least BLKSZ bytes for every
 * Page.
 */
#define BLKSZ 4096

/**
 * Size in bytes of the fixed header before the flexible slot array.
 *
 * The expression is valid once PgHeaderData is a complete type, as it is
 * whenever this header's page operations use it.
 */
#define size_of_page_header (offsetof(PgHeaderData, pgh_im))

/**
 * Update a slot's physical location and length.
 *
 * An offset of zero is reserved for a tombstoned slot. A live zero-length
 * item remains distinguishable because its offset is still non-zero. The
 * macro expects item_id to be a simple pointer expression; it is evaluated
 * more than once.
 */
#define item_id_set(item_id, off, len)                                        \
    ((item_id)->pgi_offset = (off), (item_id)->pgi_length = (len))

/** Numeric page-type value reserved for the heap-file directory. */
#define DIRECTORY_PAGE 1

/** Numeric page-type value reserved for an ordinary data page. */
#define DATA_PAGE 3

/** Byte offset measured from the beginning of a page. */
typedef uint32_t Offset;

/**
 * On-page metadata for one item-directory slot.
 *
 * Slots are numbered from one by the public API. An offset of zero marks a
 * tombstone; its length is ignored while the slot is dead. A live item may
 * still have length zero.
 */
typedef struct
{
    Offset pgi_offset;   /* byte offset to the item's first byte */
    uint32_t pgi_length; /* item length in bytes */
} PageItemMeta;

/**
 * Pointer to one slot's metadata in a page's item directory.
 *
 * The pointer refers into the page buffer and becomes invalid if that page is
 * compacted or reinitialized.
 */
typedef PageItemMeta *PgItemId;

/**
 * Fixed header and flexible slot directory at the front of a page.
 *
 * pgh_lower is the first byte after the allocated slot directory. pgh_upper
 * is the boundary between the free gap and the item-storage area. New item
 * bytes move pgh_upper downward, while a newly appended slot moves pgh_lower
 * upward.
 */
typedef struct
{
    uint32_t pgh_lower;    /* first byte after the slot directory */
    uint32_t pgh_upper;    /* boundary before the item-storage area */
    PageItemMeta pgh_im[]; /* flexible slot metadata array */
} PgHeaderData;

/** Pointer to a page's header and slot directory. */
typedef PgHeaderData *PgHeader;

/** Zero-based page number within a heap file. */
typedef unsigned long PageId;

/**
 * One entry in a page directory.
 *
 * The entry associates a page identifier with an offset-sized directory
 * value. The exact meaning and sentinel values of fpg_offset are not fixed
 * by this low-level header; higher layers decide how the value is used.
 */
typedef struct
{
    PageId fpg_id;
    Offset fpg_offset;
} PageDirEntryData;

/** Pointer to one page-directory entry. */
typedef PageDirEntryData *PageDirEntry;

/** Flags describing the kind or state of a page-directory page. */
typedef unsigned char PageFlags;

/**
 * Header and flexible entry array for a page directory.
 *
 * pt is a caller-managed type/flags byte; the directory initializer does not
 * assign one of the page-type constants automatically.
 */
typedef struct
{
    PageFlags pt;
    PageDirEntryData pds[];
} PageDirectoryData;

/** Pointer to a page-directory header and its entries. */
typedef PageDirectoryData *PageDirectory;

/** Caller-owned writable pointer to the first byte of a BLKSZ-byte page. */
typedef char *Page;

/** Number of bytes in an item or page operation argument. */
typedef uint32_t Size;

/** Untyped pointer to an item's opaque bytes. */
typedef char *Item;

/** One-based index of a slot in a page's item directory. */
typedef uint32_t OffsetNum;

/**
 * Add an item to a page or replace a tombstoned slot.
 *
 * @param page A page initialized by page_init().
 * @param item Bytes to copy into the page. The source must contain at least
 *             size bytes and must not overlap the destination page storage.
 * @param size Number of item bytes to copy; zero is valid.
 * @param offset_number Existing tombstoned slot to reuse, or exactly one
 *                      greater than page_get_max_offset_number(page) to
 *                      append a new slot.
 *
 * The item is copied into storage growing downward from pgh_upper. Appending
 * also grows the slot directory upward; reusing a slot only moves pgh_upper.
 * The function asserts that page_free_space(page) is strictly greater than
 * size + sizeof(PageItemMeta), so an exact fit is not accepted.
 */
void add_page_item(Page page, Item item, Size size, OffsetNum offset_number);

/**
 * Clear a page buffer and initialize an empty slotted-page layout.
 *
 * The caller owns the BLKSZ-byte buffer. Existing contents are destroyed.
 * After initialization, the slot directory is empty, pgh_lower equals
 * size_of_page_header, and pgh_upper equals BLKSZ.
 */
void page_init(Page page);

/**
 * Return the unused gap between the slot directory and item storage.
 *
 * This is physical free space, not the total space that could be reclaimed:
 * bytes belonging to tombstoned items remain outside this gap until
 * page_compact() is called.
 */
uint32_t page_free_space(Page page);

/**
 * Return the address of an item's bytes for a live slot.
 *
 * No liveness or bounds check is performed. The caller must obtain pgi_id
 * from a valid live slot and must not call this for a tombstoned slot. The
 * returned pointer becomes stale if the page is compacted or reinitialized.
 */
Item page_item(Page page, PgItemId pgi_id);

/**
 * Return metadata for a one-based slot number.
 *
 * No bounds check is performed; zero underflows the one-based lookup.
 * Use page_get_max_offset_number() to find the highest slot number currently
 * represented by the page; that count includes tombstoned slots.
 */
PgItemId page_get_item_id(Page page, OffsetNum offset_number);

/**
 * Tombstone a slot without moving its metadata or item bytes.
 *
 * The slot number must be in the existing range 1 through
 * page_get_max_offset_number(page); no bounds check is performed. The slot's
 * offset is set to zero; its length is ignored while the slot is dead. Its
 * former item bytes become reclaimable stranded space, and both pgh_lower and
 * pgh_upper are unchanged.
 */
void page_delete_item(Page page, OffsetNum offset_number);

/**
 * Rebuild a page using only its live items.
 *
 * Live items retain their relative order and are renumbered from one without
 * gaps. Tombstoned slots and stranded item bytes are discarded. On a
 * successful return, the slot directory and item area describe the compacted
 * page. Any existing PgItemId or item pointer into the page must be
 * reacquired. The current implementation has no allocation-failure return;
 * its temporary BLKSZ-byte page is assumed to be available.
 */
void page_compact(Page page);

/**
 * Report whether a slot is live.
 *
 * The slot number must be in the existing range 1 through
 * page_get_max_offset_number(page); no bounds check is performed. Returns
 * non-zero when the slot's offset is non-zero, and zero for a tombstone. This
 * encoding deliberately keeps a live zero-length item live.
 */
int page_item_is_live(Page page, OffsetNum offset_number);

/**
 * Return the highest slot number represented by the page.
 *
 * This is derived from pgh_lower and includes tombstoned slots; it is not a
 * count of live items. The page must have a valid initialized header. An
 * empty page returns zero.
 */
static inline OffsetNum page_get_max_offset_number(Page page)
{
    const PgHeaderData *p = (const PgHeaderData *)page;

    if(p->pgh_lower <= size_of_page_header)
        return 0;

    return (OffsetNum)((p->pgh_lower - size_of_page_header)
                       / sizeof(PageItemMeta));
}

#endif // !MOSKEN_H
