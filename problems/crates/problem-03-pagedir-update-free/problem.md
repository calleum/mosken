# Problem 3 — Page directory: update free space

**Endpoint:** `cargo test -p problem-03-pagedir-update-free` — all tests green.

## The problem

After `Table::insert` squeezes a record into page 5, page 5's real free space
shrank — but the directory still says otherwise. Every subsequent
`find_free` would happily send inserts into an overflowing page. This
problem closes that loop.

Implement:

- `pagedir_update_free(page, page_id, new_free)` — overwrite one entry's
  `free_space`, located **by page ID** (not index)
- `pagedir_find_by_page_id(page, page_id)` — lookup helper by page ID

Problem 1 + 2 operations are provided again in `mod provided`.

## The key subtlety

The directory's index space and the page-ID space are *not* the same thing.
Entry 0 might hold page 7. Callers speak in page IDs; your job is the
translation. This is exactly the bug class the original C `dir_page_init`
fell into — getting the indirection right here is the whole exercise.

## Hints

1. `find_by_page_id` is a linear scan calling `pagedir_get_entry(i)` until
   `entry.page_id == page_id`.
2. `update_free` needs the *index* to know which bytes to rewrite —
   find the index first, then write `new_free.to_le_bytes()` at
   `DIR_HEADER_SIZE + index * DIR_ENTRY_SIZE + 4`.
3. Return `Err(PageNotTracked)` before writing anything if the page ID isn't
   present — don't half-update.
