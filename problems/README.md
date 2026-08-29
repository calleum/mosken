# Mosken Problems — Confidence Ladder

Nibble-sized problems that break the next features (page directory → table
layer) into steps with a single, unambiguous endpoint: **the crate's tests go
green**. Every starter crate compiles; every test fails with `not yet
implemented`. You implement until `cargo test -p <problem>` passes.

## The Ladder

Each problem depends on the previous one's *concepts*, and later problems ship
reference implementations of earlier ones in `src/provided.rs` so every crate
is self-contained.

| # | Crate | You implement | Unlocks |
|---|-------|---------------|---------|
| 1 | `problem-01-pagedir-init` | directory init, count, add entry, get entry | the directory data layout |
| 2 | `problem-02-pagedir-find-free` | first-fit free-page search | "which page do I insert into?" |
| 3 | `problem-03-pagedir-update-free` | update free space after insert/delete | "the directory stays truthful" |
| 4 | `problem-04-table-insert` | `Table::open` + `Table::insert` | records spanning multiple pages |
| 5 | `problem-05-table-read` | `Table::read` by `TupleId` | the full write→persist→read loop |

## Workflow

```bash
cd ~/src/personal/mosken/problems
cargo test -p problem-01-pagedir-init    # red → make it green
cargo test -p problem-02-pagedir-find-free
# ...and so on, in order
```

Read `problem.md` in each crate first. Hints are at the bottom — try the
problem before reading them.

## Progress

- [ ] Problem 1 — page directory init + add entry
- [ ] Problem 2 — find free page (first fit)
- [ ] Problem 3 — update free space
- [ ] Problem 4 — Table::insert across pages
- [ ] Problem 5 — Table::read by TupleId

## Planned Next Sets

- **Buffer manager** (roadmap 2.1–2.2): clock-sweep victim selection, pin/unpin
  pool, dirty write-back, wiring Table through it.
- **B+ tree** (roadmap 3.1–3.2): leaf insert + split, search, range scan, delete.
- **Records** (roadmap 4.1): schema-driven encode/decode.

## Conventions (same as the engine)

- All on-page integers are little-endian u32, written via `to_le_bytes()` /
  `from_le_bytes()` — never `repr(C)` struct overlays.
- Item offset numbers are **1-based** (first item is offset 1); slot indices
  are 0-based internally.
- Space needed for an item is `item.len() + PageItemMeta::SERIALIZED_SIZE` (8).
