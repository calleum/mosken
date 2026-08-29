# Problem 2 — Page directory: first-fit free-page search

**Endpoint:** `cargo test -p problem-02-pagedir-find-free` — all tests green.

## The problem

A `Table::insert` needs to answer one question before anything else:
*"which page do I put this record in?"* The directory already knows every
page's free space — you just have to search it.

Implement:

- `pagedir_find_free(page, required)` — return the **first** entry with
  `free_space >= required`
- `pagedir_find_free_index(page, required)` — same, but return the index

Problem 1's operations are provided in a `mod provided` block so this crate
stands alone. Read them once — they're the reference solution for Problem 1 —
then leave them alone.

## Why first fit?

PostgreSQL's FSM and most textbook engines start with first fit; best fit and
more clever policies come later, if ever needed. The correctness bar is: never
return a page that can't hold the record, and never skip an earlier page that
could.

## Edge cases the tests cover

- Empty directory → `Err(Empty)`, not `NoFit`
- Exact fit (`required == free_space`) counts as a fit
- No entry fits → `Err(NoFit)` carrying the required size
- Index and entry-returning versions must agree

## Hints

1. Loop `for i in 0..pagedir_num_entries(page)` and decode each entry's
   `free_space` directly from the bytes — no need to materialise all entries.
2. First hit wins: return immediately, don't keep scanning for a better fit.
3. `find_free` can be three lines on top of `find_free_index`.
