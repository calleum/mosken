# Problem 1 — Page directory: init + add entry

**Endpoint:** `cargo test -p problem-01-pagedir-init` — all tests green.

## The problem

The roadmap's Task 1.2 starts: "Page 0 of the heap file is the directory page.
It tracks which pages exist and how much free space each has." Before you can
*search* that directory or *update* it, you need its byte layout and the
basic add/read operations. That's this problem — pure byte wrangling, no I/O.

Implement four functions over a raw `[u8; BLKSZ]` page buffer:

- `pagedir_init` — zero the page; count = 0
- `pagedir_num_entries` — read the count
- `pagedir_add_entry` — append `{page_id, free_space}` at the end, bump count
- `pagedir_get_entry` — read entry `i`, `None` when out of range

## Layout

```text
offset 0:  count: u32 (LE)
offset 4:  entry 0 { page_id: u32 LE, free_space: u32 LE }
offset 12: entry 1
...        entries are packed, no gaps
```

`MAX_DIR_ENTRIES` is `(BLKSZ - DIR_HEADER_SIZE) / DIR_ENTRY_SIZE` = 511.

## Constraints

- All integers little-endian via `to_le_bytes()` / `from_le_bytes()` — the
  same convention as `page.rs`. No `repr(C)` struct overlays.
- `add_entry` must return `Err(PageDirError::DirectoryFull)` instead of
  overflowing the page.

## Hints (try before reading)

1. The count lives at `page[0..4]`; entry `i` lives at
   `DIR_HEADER_SIZE + i * DIR_ENTRY_SIZE`.
2. `page[start..start+4].copy_from_slice(&value.to_le_bytes())` writes a u32;
   `u32::from_le_bytes(page[start..start+4].try_into().unwrap())` reads one.
3. `init` can be one line: `*page = [0u8; BLKSZ];` — zeroing the count field
   is implied by zeroing everything.
