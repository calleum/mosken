//! Problem 1 — Page directory: init + add entry.
//!
//! Page 0 of a Mosken heap file will become the *directory page*: it tracks
//! every data page in the file and how much free space each has. You are
//! building that directory on top of a raw 4 KiB page buffer.
//!
//! Layout (all integers little-endian, matching the rest of mosken):
//!
//! ```text
//! ┌───────────────┐  offset 0
//! │ count: u32    │  number of entries
//! ├───────────────┤  offset 4
//! │ entry 0       │  page_id: u32 | free_space: u32
//! │ entry 1       │  page_id: u32 | free_space: u32
//! │ ...           │
//! └───────────────┘  offset 4 + count * 8
//! ```

use mosken::page::BLKSZ;

/// Bytes used by the entry-count header at the start of the directory page.
pub const DIR_HEADER_SIZE: usize = 4;

/// Bytes per directory entry (`page_id: u32` + `free_space: u32`).
pub const DIR_ENTRY_SIZE: usize = 8;

/// Maximum number of entries a directory page can hold.
pub const MAX_DIR_ENTRIES: usize = (BLKSZ - DIR_HEADER_SIZE) / DIR_ENTRY_SIZE;

/// One directory entry: a data page and its remaining free space.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DirEntry {
    pub page_id: u32,
    pub free_space: u32,
}

/// Errors returned by page directory operations.
#[derive(Debug, thiserror::Error)]
pub enum PageDirError {
    #[error("directory is full ({MAX_DIR_ENTRIES} entries)")]
    DirectoryFull,

    #[error("page {page_id} is not tracked by this directory")]
    PageNotTracked { page_id: u32 },
}

/// Initialise a fresh directory page: zero everything and set count to 0.
pub fn pagedir_init(page: &mut [u8; BLKSZ]) {
    let _ = page;
    todo!()
}

/// Return the number of entries currently in the directory.
pub fn pagedir_num_entries(page: &[u8; BLKSZ]) -> u32 {
    let _ = page;
    todo!()
}

/// Append an entry to the end of the directory.
///
/// # Errors
///
/// Returns `PageDirError::DirectoryFull` if the page already holds
/// `MAX_DIR_ENTRIES` entries.
pub fn pagedir_add_entry(page: &mut [u8; BLKSZ], entry: DirEntry) -> Result<(), PageDirError> {
    let _ = (page, entry);
    todo!()
}

/// Return the entry at `index` (0-based), or `None` if out of range.
pub fn pagedir_get_entry(page: &[u8; BLKSZ], index: u32) -> Option<DirEntry> {
    let _ = (page, index);
    todo!()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn init_sets_count_to_zero() {
        let mut page = [0u8; BLKSZ];
        pagedir_init(&mut page);
        assert_eq!(pagedir_num_entries(&page), 0);
    }

    #[test]
    fn init_clears_a_dirty_page() {
        let mut page = [0xFFu8; BLKSZ];
        pagedir_init(&mut page);
        assert!(page.iter().all(|&b| b == 0));
        assert_eq!(pagedir_num_entries(&page), 0);
    }

    #[test]
    fn add_and_get_round_trip() {
        let mut page = [0u8; BLKSZ];
        pagedir_init(&mut page);
        pagedir_add_entry(
            &mut page,
            DirEntry {
                page_id: 5,
                free_space: 123,
            },
        )
        .unwrap();
        assert_eq!(pagedir_num_entries(&page), 1);
        assert_eq!(
            pagedir_get_entry(&page, 0),
            Some(DirEntry {
                page_id: 5,
                free_space: 123
            })
        );
    }

    #[test]
    fn entries_preserve_order() {
        let mut page = [0u8; BLKSZ];
        pagedir_init(&mut page);
        for (i, id) in [9u32, 4, 7, 1].into_iter().enumerate() {
            pagedir_add_entry(
                &mut page,
                DirEntry {
                    page_id: id,
                    free_space: 100,
                },
            )
            .unwrap();
            assert_eq!(pagedir_get_entry(&page, i as u32).unwrap().page_id, id);
        }
        assert_eq!(pagedir_num_entries(&page), 4);
    }

    #[test]
    fn get_out_of_range_is_none() {
        let mut page = [0u8; BLKSZ];
        pagedir_init(&mut page);
        pagedir_add_entry(
            &mut page,
            DirEntry {
                page_id: 1,
                free_space: 10,
            },
        )
        .unwrap();
        assert_eq!(pagedir_get_entry(&page, 1), None);
        assert_eq!(pagedir_get_entry(&page, 100), None);
    }

    #[test]
    fn add_past_max_errors() {
        let mut page = [0u8; BLKSZ];
        pagedir_init(&mut page);
        for i in 0..MAX_DIR_ENTRIES as u32 {
            pagedir_add_entry(
                &mut page,
                DirEntry {
                    page_id: i,
                    free_space: 1,
                },
            )
            .unwrap();
        }
        let result = pagedir_add_entry(
            &mut page,
            DirEntry {
                page_id: 999,
                free_space: 1,
            },
        );
        assert!(matches!(result, Err(PageDirError::DirectoryFull)));
    }
}
