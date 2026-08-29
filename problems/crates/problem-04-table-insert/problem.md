# Problem 4 — Table: insert across multiple pages

**Endpoint:** `cargo test -p problem-04-table-insert` — all tests green.

## The problem

This is roadmap Task 1.3, the first *real* table operation. Everything before
this was bookkeeping; now you wire the directory (provided as `pagedir.rs`,
the merged reference solution for Problems 1–3) into the engine's actual
`HeapFile` and `Page` so records flow onto as many pages as needed.

`Table::open` is written for you (study it — it shows the directory/page-0
convention). You implement **one method**: `Table::insert`.

## The insert algorithm (from the roadmap)

```
1. needed = record.len() + PageItemMeta::SERIALIZED_SIZE
2. find first directory entry with free_space >= needed
3. if none: page_id = heap.extend()?; add entry {page_id, fresh_page_free_space()}
4. load that page, add_item(record, item_count() + 1), write_page back
5. pagedir_update_free(dir, page_id, page.free_space())
6. return TupleId { page_id, offset_num }
```

## What's given

- `TupleId { page_id, offset_num }` — offset_num is the **1-based** offset
  number that `Page::add_item` expects
- `Table::open` / `Table::close` — directory load/store, page-0 convention
- `fresh_page_free_space()` — what a new page advertises
- `pagedir.rs` — Problems 1–3 solved

## Edge cases the tests cover

- First insert lands on page 1, offset 1
- Directory free space shrinks by exactly `record.len() + 8`
- 30 × 370-byte records force a spill to page 2+; the directory must track
  every page in use
- A record bigger than an empty page → `RecordTooLarge`, never attempted
- Directory state survives `close` → `open`

## Hints

1. Check `RecordTooLarge` *first*: if `needed > fresh_page_free_space()`,
   no page can ever take it — error out before touching the directory.
2. The first-fit search is Problem 2's `pagedir_find_free_index` — but it's
   not exposed for you here on purpose. Write the small loop (or re-derive
   it) inside `insert`; it's three lines.
3. After `add_item` succeeds, the page object you hold has the new
   `free_space()` — that's the value the directory must be updated to.
   Order matters: update the directory **after** a successful page write, so
   a failed insert never lies.
