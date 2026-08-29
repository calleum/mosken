//! Problem 4 — Table: insert records across multiple pages.
//!
//! This is the payoff problem: combine the directory (Problems 1–3, provided
//! here as a working module) with the engine's real `HeapFile` and `Page` to
//! build the first genuine table layer. After this, a record inserted into a
//! Table survives a close/reopen cycle and the heap file holds as many pages
//! as the data demanded — not one.

use std::path::Path;

use mosken::heapfile::{HeapFile, HeapFileError};
use mosken::page::{Page, BLKSZ};

mod pagedir;

/// Where a tuple lives: which page, and which 1-based offset number within
/// that page. Mirrors PostgreSQL's `ItemPointer`/ctid concept.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TupleId {
    pub page_id: u32,
    pub offset_num: u32,
}

/// Errors returned by table operations.
#[derive(Debug, thiserror::Error)]
pub enum TableError {
    #[error(transparent)]
    HeapFile(#[from] HeapFileError),

    #[error("record of {record_len} bytes exceeds an empty page's capacity")]
    RecordTooLarge { record_len: usize },
}

/// A table: a heap file whose page 0 is the directory.
///
/// Invariant maintained by `insert`: the directory entry for every data page
/// always reflects that page's *current* `free_space()`.
pub struct Table {
    heap: HeapFile,
    /// Cached copy of the directory page (page 0 of the file).
    dir: [u8; BLKSZ],
}

/// The free space a freshly initialised data page offers.
///
/// A new page starts with `lower = PAGE_HEADER_SIZE (8)` and
/// `upper = BLKSZ`, so its usable free space is exactly `BLKSZ - 8`. The
/// directory should record this value when a new page is born.
fn fresh_page_free_space() -> u32 {
    BLKSZ as u32 - mosken::page::PAGE_HEADER_SIZE as u32
}

impl Table {
    /// Open (or create) a table backed by the heap file at `path`.
    ///
    /// A brand-new file gets: directory page written to page 0, then one
    /// empty data page (page 1) appended and registered in the directory.
    /// An existing file just loads its directory page.
    pub fn open(path: &Path) -> Result<Self, TableError> {
        let mut heap = HeapFile::open(path)?;
        if heap.num_pages()? == 0 {
            let dir_page_id = heap.extend()?;
            debug_assert_eq!(dir_page_id, 0);
            let first_data_page = heap.extend()?;
            debug_assert_eq!(first_data_page, 1);

            let mut dir = [0u8; BLKSZ];
            pagedir::pagedir_init(&mut dir);
            pagedir::pagedir_add_entry(
                &mut dir,
                pagedir::DirEntry {
                    page_id: first_data_page,
                    free_space: fresh_page_free_space(),
                },
            )
            .expect("fresh directory always has room");
            heap.write_page(0, &Page::from_bytes(dir))?;

            Ok(Self { heap, dir })
        } else {
            let dir_page = heap.read_page(0)?;
            Ok(Self {
                heap,
                dir: *dir_page.as_bytes(),
            })
        }
    }

    /// Persist the directory page and close the underlying heap file.
    pub fn close(mut self) -> Result<(), TableError> {
        self.heap.write_page(0, &Page::from_bytes(self.dir))?;
        self.heap.close()?;
        Ok(())
    }

    /// Insert a record, returning where it landed.
    ///
    /// Algorithm:
    /// 1. First-fit search the directory for a page with room for the record
    ///    (`record.len() + PageItemMeta::SERIALIZED_SIZE`).
    /// 2. If none fits, extend the heap file with a new page and register it.
    /// 3. Load the chosen page, `add_item` at the next offset number
    ///    (`item_count() + 1`), write it back.
    /// 4. Update the directory entry's free space to the page's new value.
    ///
    /// # Errors
    ///
    /// Returns `TableError::RecordTooLarge` if the record can never fit on
    /// even an empty page.
    pub fn insert(&mut self, record: &[u8]) -> Result<TupleId, TableError> {
        let _ = record;
        todo!()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn temp_table() -> (Table, tempfile::TempDir) {
        let dir = tempfile::tempdir().unwrap();
        let table = Table::open(&dir.path().join("test.db")).unwrap();
        (table, dir)
    }

    #[test]
    fn open_new_table_creates_dir_and_one_data_page() {
        let (table, _dir) = temp_table();
        assert_eq!(
            pagedir::pagedir_num_entries(&table.dir),
            1,
            "exactly one data page registered"
        );
        let entry = pagedir::pagedir_get_entry(&table.dir, 0).unwrap();
        assert_eq!(
            entry.page_id, 1,
            "first data page is page 1 (page 0 is the directory)"
        );
        assert_eq!(entry.free_space, fresh_page_free_space());
    }

    #[test]
    fn open_existing_table_loads_dir() {
        let (table, tmp) = temp_table();
        table.close().unwrap();
        let table = Table::open(&tmp.path().join("test.db")).unwrap();
        assert_eq!(pagedir::pagedir_num_entries(&table.dir), 1);
    }

    #[test]
    fn single_insert_returns_tuple_id() {
        let (mut table, _dir) = temp_table();
        let tid = table.insert(b"hello").unwrap();
        assert_eq!(tid.page_id, 1);
        assert_eq!(tid.offset_num, 1, "first item on a page is offset 1");
    }

    #[test]
    fn consecutive_inserts_bump_offset_num() {
        let (mut table, _dir) = temp_table();
        let t1 = table.insert(b"one").unwrap();
        let t2 = table.insert(b"two").unwrap();
        assert_eq!(
            t1,
            TupleId {
                page_id: 1,
                offset_num: 1
            }
        );
        assert_eq!(
            t2,
            TupleId {
                page_id: 1,
                offset_num: 2
            }
        );
    }

    #[test]
    fn directory_free_space_shrinks_after_insert() {
        let (mut table, _dir) = temp_table();
        let before = pagedir::pagedir_get_entry(&table.dir, 0)
            .unwrap()
            .free_space;
        table.insert(b"some record").unwrap();
        let after = pagedir::pagedir_get_entry(&table.dir, 0)
            .unwrap()
            .free_space;
        let expected_shrink =
            ("some record".len() + mosken::page::PageItemMeta::SERIALIZED_SIZE) as u32;
        assert_eq!(before - after, expected_shrink);
    }

    #[test]
    fn spills_to_second_page_when_full() {
        let (mut table, _dir) = temp_table();
        // ~370-byte records: page 1 fits about 11 of them, then it spills.
        let record = vec![0xABu8; 370];
        let mut last = None;
        for _ in 0..30 {
            last = Some(table.insert(&record).unwrap());
        }
        let last = last.unwrap();
        assert!(
            last.page_id >= 2,
            "30 x 370B records cannot fit on one page; last landed on page {}",
            last.page_id
        );
        assert_eq!(
            pagedir::pagedir_num_entries(&table.dir),
            last.page_id as u32,
            "directory must track every data page in use"
        );
    }

    #[test]
    fn oversized_record_errors() {
        let (mut table, _dir) = temp_table();
        let result = table.insert(&vec![0u8; BLKSZ]);
        assert!(matches!(result, Err(TableError::RecordTooLarge { .. })));
    }

    #[test]
    fn directory_survives_close_reopen() {
        let (table, tmp) = temp_table();
        let mut table = table;
        for i in 0..20u32 {
            table.insert(format!("record-{i}").as_bytes()).unwrap();
        }
        table.close().unwrap();

        let table = Table::open(&tmp.path().join("test.db")).unwrap();
        let entries: Vec<u32> = (0..pagedir::pagedir_num_entries(&table.dir))
            .map(|i| pagedir::pagedir_get_entry(&table.dir, i).unwrap().page_id)
            .collect();
        assert!(entries.len() >= 1);
        assert_eq!(entries[0], 1);
    }
}
