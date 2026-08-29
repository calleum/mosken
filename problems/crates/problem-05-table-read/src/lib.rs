//! Problem 5 — Table: read a tuple back by TupleId.
//!
//! Completes the loop: insert (Problem 4, provided as a reference
//! implementation this time) → persist → read back. After this problem the
//! table layer is genuinely useful: data written, closed, reopened, and read
//! by address.

use std::path::Path;

use mosken::heapfile::{HeapFile, HeapFileError};

mod pagedir;
mod provided_table;

pub use provided_table::{Table as InsertTable, TupleId};

/// Errors returned by reading a tuple.
#[derive(Debug, thiserror::Error)]
pub enum TableReadError {
    #[error(transparent)]
    HeapFile(#[from] HeapFileError),

    #[error("tuple {tuple_id:?} does not exist (page has {item_count} items)")]
    InvalidTupleId {
        tuple_id: (u32, u32),
        item_count: u32,
    },
}

/// A table opened specifically for reading tuples back.
///
/// Deliberately a separate, minimal struct from Problem 4's `Table`: you
/// focus on the read path without touching insert logic.
pub struct TableReader {
    heap: HeapFile,
}

impl TableReader {
    /// Open an existing table file.
    pub fn open(path: &Path) -> Result<Self, TableReadError> {
        let heap = HeapFile::open(path)?;
        Ok(Self { heap })
    }

    /// Close the underlying heap file.
    pub fn close(self) -> Result<(), TableReadError> {
        self.heap.close()?;
        Ok(())
    }

    /// Read the record at `tuple_id`.
    ///
    /// Load page `tuple_id.page_id`, then return the item at
    /// `tuple_id.offset_num`.
    ///
    /// # Errors
    ///
    /// Returns `TableReadError::InvalidTupleId` if the offset number exceeds
    /// the page's item count (or the item was deleted).
    pub fn read(&mut self, tuple_id: TupleId) -> Result<Vec<u8>, TableReadError> {
        let _ = &tuple_id;
        todo!()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use provided_table::TupleId as Tid;

    fn write_fixture(path: &Path, records: &[&[u8]]) -> Vec<TupleId> {
        let mut table = InsertTable::open(path).unwrap();
        let ids = records
            .iter()
            .map(|r| table.insert(r).unwrap())
            .collect::<Vec<_>>();
        table.close().unwrap();
        ids
    }

    #[test]
    fn read_single_record_back() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("t.db");
        let ids = write_fixture(&path, &[b"expensive tuna"]);

        let mut reader = TableReader::open(&path).unwrap();
        let data = reader
            .read(Tid {
                page_id: ids[0].page_id,
                offset_num: ids[0].offset_num,
            })
            .unwrap();
        assert_eq!(data, b"expensive tuna");
        reader.close().unwrap();
    }

    #[test]
    fn read_all_records_in_order() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("t.db");
        let records: Vec<Vec<u8>> = (0..25u32)
            .map(|i| format!("record-{i}").into_bytes())
            .collect();
        let refs: Vec<&[u8]> = records.iter().map(|r| r.as_slice()).collect();
        let ids = write_fixture(&path, &refs);

        let mut reader = TableReader::open(&path).unwrap();
        for (i, id) in ids.iter().enumerate() {
            let data = reader
                .read(Tid {
                    page_id: id.page_id,
                    offset_num: id.offset_num,
                })
                .unwrap();
            assert_eq!(data, records[i], "record {i} must round-trip exactly");
        }
    }

    #[test]
    fn records_that_spilled_pages_read_back() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("t.db");
        let big = vec![0xCDu8; 370];
        let records: Vec<Vec<u8>> = (0..30u32)
            .map(|i| {
                let mut r = big.clone();
                r[0] = i as u8;
                r
            })
            .collect();
        let refs: Vec<&[u8]> = records.iter().map(|r| r.as_slice()).collect();
        let ids = write_fixture(&path, &refs);
        assert!(ids.last().unwrap().page_id >= 2, "fixture must span pages");

        let mut reader = TableReader::open(&path).unwrap();
        for (i, id) in ids.iter().enumerate() {
            let data = reader
                .read(Tid {
                    page_id: id.page_id,
                    offset_num: id.offset_num,
                })
                .unwrap();
            assert_eq!(data, records[i]);
        }
    }

    #[test]
    fn invalid_offset_num_errors() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("t.db");
        let ids = write_fixture(&path, &[b"only one"]);
        let _ = &ids;

        let mut reader = TableReader::open(&path).unwrap();
        let result = reader.read(Tid {
            page_id: ids[0].page_id,
            offset_num: 5,
        });
        assert!(matches!(result, Err(TableReadError::InvalidTupleId { .. })));
    }

    #[test]
    fn invalid_page_id_errors() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("t.db");
        let ids = write_fixture(&path, &[b"data"]);
        let _ = &ids;

        let mut reader = TableReader::open(&path).unwrap();
        let result = reader.read(Tid {
            page_id: 99,
            offset_num: 1,
        });
        assert!(matches!(result, Err(TableReadError::InvalidTupleId { .. })));
    }

    #[test]
    fn full_cycle_insert_close_reopen_read() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("t.db");
        let ids = write_fixture(&path, &[b"before close"]);

        let mut reader = TableReader::open(&path).unwrap();
        let data = reader
            .read(Tid {
                page_id: ids[0].page_id,
                offset_num: ids[0].offset_num,
            })
            .unwrap();
        assert_eq!(data, b"before close");
    }
}
