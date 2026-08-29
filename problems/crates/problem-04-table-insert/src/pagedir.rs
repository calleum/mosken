//! Page directory — the combined reference solution for Problems 1–3.
//!
//! Provided so the Table problems stand alone. Do not modify: the tests in
//! this crate (and Problem 5) assume this exact behaviour.

#![allow(dead_code)]

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
pub enum PageDirError {
    #[error("directory is full")]
    DirectoryFull,

    #[error("page {page_id} is not tracked by this directory")]
    PageNotTracked { page_id: u32 },
}

/// Initialise a fresh directory page: zero everything, count = 0.
pub fn pagedir_init(page: &mut [u8; BLKSZ]) {
    *page = [0u8; BLKSZ];
}

/// Number of entries currently in the directory.
pub fn pagedir_num_entries(page: &[u8; BLKSZ]) -> u32 {
    u32::from_le_bytes(page[0..4].try_into().unwrap())
}

/// Append an entry to the end of the directory.
pub fn pagedir_add_entry(page: &mut [u8; BLKSZ], entry: DirEntry) -> Result<(), PageDirError> {
    let count = pagedir_num_entries(page) as usize;
    if count >= (BLKSZ - DIR_HEADER_SIZE) / DIR_ENTRY_SIZE {
        return Err(PageDirError::DirectoryFull);
    }
    let start = DIR_HEADER_SIZE + count * DIR_ENTRY_SIZE;
    page[start..start + 4].copy_from_slice(&entry.page_id.to_le_bytes());
    page[start + 4..start + 8].copy_from_slice(&entry.free_space.to_le_bytes());
    page[0..4].copy_from_slice(&((count + 1) as u32).to_le_bytes());
    Ok(())
}

/// Entry at `index` (0-based), or `None` if out of range.
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

/// Index of the first entry with `free_space >= required` (first fit).
pub fn pagedir_find_free_index(page: &[u8; BLKSZ], required: u32) -> Option<u32> {
    (0..pagedir_num_entries(page)).find(|&i| {
        pagedir_get_entry(page, i)
            .map(|e| e.free_space >= required)
            .unwrap_or(false)
    })
}

/// Set the free space recorded for `page_id`.
pub fn pagedir_update_free(
    page: &mut [u8; BLKSZ],
    page_id: u32,
    new_free: u32,
) -> Result<(), PageDirError> {
    let count = pagedir_num_entries(page);
    for i in 0..count {
        if let Some(entry) = pagedir_get_entry(page, i) {
            if entry.page_id == page_id {
                let start = DIR_HEADER_SIZE + i as usize * DIR_ENTRY_SIZE + 4;
                page[start..start + 4].copy_from_slice(&new_free.to_le_bytes());
                return Ok(());
            }
        }
    }
    Err(PageDirError::PageNotTracked { page_id })
}
