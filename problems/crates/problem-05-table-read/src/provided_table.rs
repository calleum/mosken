//! Reference implementation of Problem 4's table insert.
//!
//! Provided so Problem 5 stands alone. Do not modify — Problem 5's tests
//! build fixtures with this.

use std::path::Path;

use mosken::heapfile::{HeapFile, HeapFileError};
use mosken::page::{Page, PageError, BLKSZ};

use super::pagedir;

/// Where a tuple lives: which page, and which 1-based offset number.
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

    #[error(transparent)]
    Page(#[from] PageError),

    #[error("record of {record_len} bytes exceeds an empty page's capacity")]
    RecordTooLarge { record_len: usize },
}

fn fresh_page_free_space() -> u32 {
    BLKSZ as u32 - mosken::page::PAGE_HEADER_SIZE as u32
}

/// A table: a heap file whose page 0 is the directory.
pub struct Table {
    heap: HeapFile,
    dir: [u8; BLKSZ],
}

impl Table {
    /// Open (or create) a table backed by the heap file at `path`.
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
    pub fn insert(&mut self, record: &[u8]) -> Result<TupleId, TableError> {
        let needed = (record.len() + mosken::page::PageItemMeta::SERIALIZED_SIZE) as u32;
        if needed > fresh_page_free_space() {
            return Err(TableError::RecordTooLarge {
                record_len: record.len(),
            });
        }

        // First-fit search over the directory.
        let chosen = (0..pagedir::pagedir_num_entries(&self.dir))
            .map(|i| pagedir::pagedir_get_entry(&self.dir, i).unwrap())
            .find(|e| e.free_space >= needed);

        let page_id = match chosen {
            Some(entry) => entry.page_id,
            None => {
                let new_page_id = self.heap.extend()?;
                pagedir::pagedir_add_entry(
                    &mut self.dir,
                    pagedir::DirEntry {
                        page_id: new_page_id,
                        free_space: fresh_page_free_space(),
                    },
                )
                .expect("directory cannot fill during a single test run");
                new_page_id
            }
        };

        let mut page = self.heap.read_page(page_id as u64)?;
        let offset_num = page.item_count() + 1;
        page.add_item(record, offset_num)?;
        self.heap.write_page(page_id as u64, &page)?;

        pagedir::pagedir_update_free(&mut self.dir, page_id, page.free_space())
            .expect("inserted page must be tracked");

        Ok(TupleId {
            page_id,
            offset_num,
        })
    }
}
