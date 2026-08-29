//! Problem 2 — Page directory: first-fit free-page search.
//!
//! Builds directly on Problem 1's layout (count at offset 0, then packed
//! `{page_id, free_space}` u32 pairs). That layout is *provided* here so this
//! crate is self-contained; you only implement the search.

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
pub enum FindFreeError {
    #[error("no tracked page has at least {required} bytes free")]
    NoFit { required: usize },

    #[error("directory is empty")]
    Empty,
}

/// Re-exported Problem 1 operations (already solved — do not modify).
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
}

pub use provided::{pagedir_add_entry, pagedir_init, pagedir_num_entries};

/// Find the entry with enough free space for `required` bytes.
///
/// Scan entries from index 0 upward and return the **first** entry whose
/// `free_space >= required` — first fit, not best fit. (Real engines do
/// something smarter; first fit is the correct first step.)
///
/// # Errors
///
/// - `FindFreeError::Empty` if the directory has zero entries.
/// - `FindFreeError::NoFit` if no entry has enough free space.
pub fn pagedir_find_free(page: &[u8; BLKSZ], required: u32) -> Result<DirEntry, FindFreeError> {
    let _ = (page, required);
    todo!()
}

/// Convenience: index of the first fitting entry. `None` if no fit.
///
/// Useful when the caller needs to update that entry's free space later
/// (that's Problem 3).
pub fn pagedir_find_free_index(page: &[u8; BLKSZ], required: u32) -> Option<u32> {
    let _ = (page, required);
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
    fn empty_directory_errors() {
        let page = setup(&[]);
        let result = pagedir_find_free(&page, 1);
        assert!(matches!(result, Err(FindFreeError::Empty)));
    }

    #[test]
    fn first_fit_wins() {
        let page = setup(&[(0, 100), (1, 500), (2, 50)]);
        let entry = pagedir_find_free(&page, 60).unwrap();
        assert_eq!(entry.page_id, 0); // first with >= 60, not the biggest
    }

    #[test]
    fn skips_pages_too_small() {
        let page = setup(&[(0, 100), (1, 500), (2, 50)]);
        let entry = pagedir_find_free(&page, 200).unwrap();
        assert_eq!(entry.page_id, 1);
    }

    #[test]
    fn exact_fit_counts() {
        let page = setup(&[(0, 100), (1, 500)]);
        let entry = pagedir_find_free(&page, 500).unwrap();
        assert_eq!(entry.page_id, 1);
    }

    #[test]
    fn no_fit_errors_with_required() {
        let page = setup(&[(0, 100), (1, 200)]);
        let result = pagedir_find_free(&page, 1000);
        assert!(matches!(
            result,
            Err(FindFreeError::NoFit { required: 1000 })
        ));
    }

    #[test]
    fn find_free_index_matches_find_free() {
        let page = setup(&[(7, 10), (3, 20), (5, 30)]);
        assert_eq!(pagedir_find_free_index(&page, 25), Some(2));
        assert_eq!(pagedir_find_free_index(&page, 31), None);
        assert_eq!(pagedir_find_free(&page, 25).unwrap().page_id, 5);
    }
}
