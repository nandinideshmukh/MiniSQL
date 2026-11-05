# Bug: `Error: Failed to insert (duplicate key or full page)` on a brand-new/empty table

## Symptom
Even the very first `INSERT` into a table that has no records yet fails with:

```
Error: Failed to insert (duplicate key or full page)
```

## Root cause
Two different header structs are laid over the same raw 4KB `Page`:

- `PageHeader` (page.h) — used for slotted **heap** pages: `{page_id, free_space, slot_count}`
- `BTreeHeader` (btree.h) — used for **B+Tree** nodes: `{is_leaf, is_root, page_id, parent_page_id, num_keys, next_leaf_id}`

These are **not layout-compatible**. `BufferPool::fetch_page()` (`src/buffer/buffer.c`) called
`init_page()` on every brand-new page, unconditionally stamping the **heap** `PageHeader` layout
onto it — even though page 0 is always used as the **B+Tree** root.

When `init_v2_engine()` then read that same page through `btree_get_header()` (the `BTreeHeader`
view), the bytes landed on the wrong fields:
- `is_leaf` byte overlapped `PageHeader.page_id`'s low byte → read as `0` (false)
- `BTreeHeader.page_id` (offset 4) overlapped `PageHeader.free_space`/`slot_count` → read as `4096`, not `0`

Because `hdr->page_id == 0` was false, the code believed the root was "already initialized" and
**skipped `btree_init_node()`** — so the page was left with `is_leaf = false`.

`btree_leaf_insert()` starts with:
```c
if (!header->is_leaf) return false;
```
...which is exactly the `false` returned by `execute_insert()`, reported as "duplicate key or full page" — on a completely empty table.

A second, related bug: `btree_init_node()` never actually sets `header->page_id`, and the root's
real `page_id` is legitimately `0` forever — so the old `hdr->page_id == 0` check could never
reliably distinguish "uninitialized page" from "a real, already-initialized root," even once the
struct-mismatch above was fixed.

## Fix
1. **`src/buffer/buffer.c`** — `fetch_page()` no longer calls the heap-specific `init_page()` on a
   new page. It just zeroes the 4KB block and lets the caller initialize whichever header format
   (`PageHeader` for heap pages, `BTreeHeader` for B+Tree nodes) is appropriate for that page's use.
2. **`src/executor/executor.c`** — `init_v2_engine()`'s "needs initialization" check now uses
   `!hdr->is_root` instead of `hdr->page_id == 0`. A freshly zeroed page has `is_root == false`;
   a page that has already gone through `btree_init_node()` has `is_root == true`. This reliably
   distinguishes "never initialized" from "already a valid root," which `page_id` could not do.

## Tests
See `tests/test_repro.c`, `tests/test_basic.c`, `tests/test_full.c`:
- `test_repro.c` — reproduces the original bug (insert into a fresh table). Fails before the fix, passes after.
- `test_basic.c` — insert two records into a fresh table, then select — matches the reported scenario end-to-end.
- `test_full.c` — inserts `BTREE_LEAF_MAX_KEYS` records (should all succeed) plus 5 more (should all
  correctly fail with the *genuine* "full page" error), confirming the real full-page error path
  still works and wasn't papered over.

Build and run any of them, e.g.:
```
gcc -Iinclude -o tests/test_basic tests/test_basic.c \
    src/executor/executor.c src/btree/btree.c src/buffer/buffer.c src/page/page.c src/wal/wal.c
./tests/test_basic
```

## Known follow-up (not part of this fix, separate issue)
Nothing currently calls `flush_all_pages()` / closes the buffer pool on normal program exit
(only `recover_database()` does, on crash recovery). This means data does not persist to the
`.v2` file across separate runs of the program yet — every fresh process start effectively sees
an empty database file. Worth flushing on a clean CLI exit if cross-run persistence is expected.
