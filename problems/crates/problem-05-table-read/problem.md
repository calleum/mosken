# Problem 5 — Table: read by TupleId

**Endpoint:** `cargo test -p problem-05-table-read` — all tests green.

## The problem

Roadmap Task 1.3's second half. Inserts are only useful if you can get the
data back. You implement **one method**: `TableReader::read`.

Problem 4's insert is provided as a reference implementation
(`provided_table.rs`) — worth reading regardless, it's what a clean `insert`
looks like. The tests write fixtures through it, then your `read` must fetch
everything back by address.

## The read algorithm

```
1. page = heap.read_page(tuple_id.page_id)?
2. if tuple_id.offset_num > page.item_count() → Err(InvalidTupleId)
3. Ok(page.get_item(tuple_id.offset_num).to_vec())
```

Three steps. The subtlety is all in the error design: which failures are
`InvalidTupleId` versus a `HeapFile` error bubbling up.

## Decisions the tests force you to make

- `offset_num` past the page's item count → `InvalidTupleId` (the tuple
  doesn't exist; it's not an I/O failure)
- `page_id` out of bounds → also surfaced as `InvalidTupleId`, because from
  the caller's perspective a TupleId pointing nowhere is one kind of mistake,
  no matter which layer noticed
- Remember `offset_num` is 1-based and `get_item` returns `None` for deleted
  items — treat a `None` as `InvalidTupleId` too

## Hints

1. `heap.read_page` already returns `HeapFileError::InvalidPageId` for a bad
   page — you *could* let that bubble up via `#[from]`, but the tests want a
   uniform `InvalidTupleId`. Convert: catch the out-of-bounds page before
   calling into the heap file.
2. `page.get_item(n)` returns `Option<&[u8]>` — `.map(|s| s.to_vec())` gets
   you to the return type.
3. The whole method body is roughly eight lines. If it's growing, you're
   redoing insert logic — don't; `provided_table` already did that work.
