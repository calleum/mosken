//! Problem 3 — Page directory: update free space after insert/delete.
//!
//! Same layout again (count + packed entries). The search from Problem 2 is
//! provided. You implement the write side: after a Table inserts into or
//! deletes from a data page, the directory's free-space figure must be
//! corrected — otherwise the next `find_free` lies.

use mosken::page::BLKSZ;

pub const DIR_HEADER_SIZE: usize = 4;
pub const DIR_ENTRY_SIZE: usize = 8;

/// One directory entry: a data page and its remaining free space.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DirEntry {
    pub page_id: u32,
    pub free_space: u32,
}

/// Errors returned by page directory operations.
#[derive(Debug, thiserror::Error)]
pub enum UpdateFreeError {
    #[error("page {page_id} is not tracked by this directory")]
    PageNotTracked { page_id: u32 },
}

/// Provided Problem 1 + 2 operations (already solved — do not modify).
mod provided {
    use super::*;

    pub fn pagedir_init(page: &mut [u8; BLKSZ]) {
        *page = [0u8; BLKSZ];
    }

    pub fn pagedir_num_entries(page: &[u8; BLKSZ]) -> u32 {
        u32::from_le_bytes(page[0..4].try_into().unwrap())
    }

    pub fn pagedir_add_entry(page: &mut [u8; BLKSZ], entry: DirEntry) -> Result<(), &'static str> {
        let count = pagedir_num_entries(page) as usize;
        if count >= (BLKSZ - DIR_HEADER_SIZE) / DIR_ENTRY_SIZE {
            return Err("directory full");
        }
        let start = DIR_HEADER_SIZE + count * DIR_ENTRY_SIZE;
        page[start..start + 4].copy_from_slice(&entry.page_id.to_le_bytes());
        page[start + 4..start + 8].copy_from_slice(&entry.free_space.to_le_bytes());
        page[0..4].copy_from_slice(&((count + 1) as u32).to_le_bytes());
        Ok(())
    }

    pub fn pagedir_get_entry(page: &[u8; BLKSZ], index: u32) -> Option<DirEntry> {
        let count = pagedir_num_entries(page);
        if index >= count {
            return None;
        }
        let start = DIR_HEADER_SIZE + index as usize * DIR_ENTRY_SIZE;
        Some(DirEntry {
            page_id: u32::from_le_bytes(page[start..start + 4].try_into().unwrap()),
            free_space: u32::from_le_bytes(page[start + 4..start + 8].try_into().unwrap()),
        })
    }
}

pub use provided::{pagedir_add_entry, pagedir_get_entry, pagedir_init, pagedir_num_entries};

/// Set the free space recorded for `page_id` to `new_free`.
///
/// Entries are keyed by `page_id`, **not** by directory index — the caller
/// thinks in page IDs, the directory handles the lookup.
///
/// # Errors
///
/// Returns `UpdateFreeError::PageNotTracked` if no entry has that `page_id`.
pub fn pagedir_update_free(
    page: &mut [u8; BLKSZ],
    page_id: u32,
    new_free: u32,
) -> Result<(), UpdateFreeError> {
    let _ = (page, page_id, new_free);
    todo!()
}

/// Look up an entry by `page_id`. `None` if not tracked.
pub fn pagedir_find_by_page_id(page: &[u8; BLKSZ], page_id: u32) -> Option<DirEntry> {
    let _ = (page, page_id);
    todo!()
}

#[cfg(test)]
mod tests {
    use super::provided as p;
    use super::*;

    fn setup(entries: &[(u32, u32)]) -> [u8; BLKSZ] {
        let mut page = [0u8; BLKSZ];
        p::pagedir_init(&mut page);
        for (id, free) in entries {
            p::pagedir_add_entry(
                &mut page,
                DirEntry {
                    page_id: *id,
                    free_space: *free,
                },
            )
            .unwrap();
        }
        page
    }

    #[test]
    fn find_by_page_id_hits_and_misses() {
        let page = setup(&[(7, 10), (3, 20), (5, 30)]);
        assert_eq!(
            pagedir_find_by_page_id(&page, 3),
            Some(DirEntry {
                page_id: 3,
                free_space: 20
            })
        );
        assert_eq!(pagedir_find_by_page_id(&page, 4), None);
    }

    #[test]
    fn update_changes_only_target_entry() {
        let mut page = setup(&[(7, 10), (3, 20), (5, 30)]);
        pagedir_update_free(&mut page, 3, 5).unwrap();

        assert_eq!(
            pagedir_find_by_page_id(&page, 7).unwrap().free_space,
            10,
            "untouched entry must not change"
        );
        assert_eq!(
            pagedir_find_by_page_id(&page, 3).unwrap().free_space,
            5,
            "target entry must be updated"
        );
        assert_eq!(
            pagedir_find_by_page_id(&page, 5).unwrap().free_space,
            30,
            "untouched entry must not change"
        );
    }

    #[test]
    fn update_unknown_page_errors() {
        let mut page = setup(&[(7, 10)]);
        let result = pagedir_update_free(&mut page, 99, 1);
        assert!(matches!(
            result,
            Err(UpdateFreeError::PageNotTracked { page_id: 99 })
        ));
    }

    #[test]
    fn update_then_find_free_sees_new_value() {
        let mut page = setup(&[(0, 100), (1, 200)]);
        pagedir_update_free(&mut page, 0, 50).unwrap();

        // 50 < 60 so page 0 no longer fits; the search must land on page 1.
        let entry = p::pagedir_get_entry(&page, 0).unwrap();
        assert_eq!(entry.free_space, 50);
        assert_eq!(pagedir_find_by_page_id(&page, 0).unwrap().free_space, 50);
    }

    #[test]
    fn update_all_entries_one_by_one() {
        let mut page = setup(&[(0, 10), (1, 20), (2, 30)]);
        pagedir_update_free(&mut page, 2, 0).unwrap();
        pagedir_update_free(&mut page, 0, 40).unwrap();
        pagedir_update_free(&mut page, 1, 25).unwrap();

        assert_eq!(pagedir_find_by_page_id(&page, 0).unwrap().free_space, 40);
        assert_eq!(pagedir_find_by_page_id(&page, 1).unwrap().free_space, 25);
        assert_eq!(pagedir_find_by_page_id(&page, 2).unwrap().free_space, 0);
    }
}
