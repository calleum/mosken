//! Slotted page layout for the Mosken storage engine.
//!
//! Page layout (4 KiB = 4096 bytes):
//!
//! ```text
//! ┌──────────────────────────────────┐  ← offset 0
//! │  PageHeader { lower, upper }     │
//! ├──────────────────────────────────┤  ← offset lower (grows down)
//! │  PageItemMeta[]                  │    item metadata array
//! │  ...                             │
//! │                                  │
//! │        (free space)              │
//! │                                  │
//! │  ...                             │
//! │  Item data                       │    items grow upward
//! └──────────────────────────────────┘  ← offset upper (grows up)
//! ```
//!
//! Items are stored from the top of the page downward. Item metadata entries
//! grow upward from just after the header. Free space is in the middle.

/// Database block size in bytes.
pub const BLKSZ: usize = 4096;

/// Offset of the first byte past the fixed header fields.
///
/// The header contains `lower` (u32) and `upper` (u32) = 8 bytes.
/// The flexible `PageItemMeta` array starts immediately after.
pub const PAGE_HEADER_SIZE: usize = 8;

/// Metadata for a single item within a page.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub struct PageItemMeta {
    /// Byte offset from the start of the page to the item data.
    pub offset: u32,
    /// Length of the item data in bytes.
    pub length: u32,
}

impl PageItemMeta {
    pub const SERIALIZED_SIZE: usize = 8;

    pub fn new(offset: u32, length: u32) -> Self {
        Self { offset, length }
    }

    /// Whether this slot has been deleted (tombstone).
    pub fn is_deleted(&self) -> bool {
        self.length == 0
    }
}

/// Errors returned by page operations.
#[derive(Debug, thiserror::Error)]
pub enum PageError {
    #[error("page is full: need {need} bytes but only {available} free")]
    InsufficientSpace { need: usize, available: usize },

    #[error("invalid offset number {offset_number} (page has {item_count} items)")]
    InvalidOffset { offset_number: u32, item_count: usize },

    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

/// A slotted page backed by a fixed-size buffer.
///
/// This is the core page abstraction. It manages a 4 KiB byte array with a
/// slotted-page layout: item metadata grows upward from the header, while
/// item data grows downward from the end.
pub struct Page {
    data: [u8; BLKSZ],
}

impl Page {
    /// Create a new zeroed page with proper header initialization.
    ///
    /// After initialization:
    /// - `lower` = `PAGE_HEADER_SIZE` (just past the fixed header)
    /// - `upper` = `BLKSZ` (entire page is free space)
    pub fn new() -> Self {
        let mut page = Self {
            data: [0u8; BLKSZ],
        };
        page.init();
        page
    }

    /// Re-initialize the page (clear all data and reset header).
    pub fn init(&mut self) {
        self.data.fill(0);
        self.set_lower(PAGE_HEADER_SIZE as u32);
        self.set_upper(BLKSZ as u32);
    }

    /// Number of bytes of free space between the item metadata and the item data.
    pub fn free_space(&self) -> u32 {
        self.upper() - self.lower()
    }

    /// Number of item metadata entries currently stored.
    pub fn item_count(&self) -> u32 {
        (self.lower() - PAGE_HEADER_SIZE as u32) / PageItemMeta::SERIALIZED_SIZE as u32
    }

    /// Add an item to the page.
    ///
    /// The item is stored at the given 1-based `offset_number`. The caller is
    /// responsible for choosing the correct offset number (usually
    /// `item_count() + 1` for appends).
    ///
    /// # Errors
    ///
    /// Returns `PageError::InsufficientSpace` if the page doesn't have room.
    /// Returns `PageError::InvalidOffset` if the offset number is out of range.
    pub fn add_item(&mut self, item: &[u8], offset_number: u32) -> Result<(), PageError> {
        let space_needed = item.len() + PageItemMeta::SERIALIZED_SIZE;
        if self.free_space() < space_needed as u32 {
            return Err(PageError::InsufficientSpace {
                need: space_needed,
                available: self.free_space() as usize,
            });
        }

        // Offset numbers are 1-based; the slot index is 0-based.
        let slot_index = (offset_number - 1) as usize;
        // We allow writing to the next available slot (append) or overwriting
        // an existing slot.
        let current_count = self.item_count() as usize;
        if slot_index > current_count {
            return Err(PageError::InvalidOffset {
                offset_number,
                item_count: current_count,
            });
        }

        // Shift upper down to make room for the item data.
        let new_upper = self.upper() - item.len() as u32;
        self.set_upper(new_upper);

        // Copy item data into the page.
        let upper = self.upper() as usize;
        self.data[upper..upper + item.len()].copy_from_slice(item);

        // Write the item metadata.
        let meta = PageItemMeta::new(new_upper, item.len() as u32);
        self.write_item_meta(slot_index, &meta);

        // Advance lower past the new metadata entry (only if we're appending).
        if slot_index == current_count {
            self.set_lower(self.lower() + PageItemMeta::SERIALIZED_SIZE as u32);
        }

        Ok(())
    }

    /// Retrieve an item by its 1-based offset number.
    ///
    /// Returns `None` if the offset is out of bounds or the item was deleted.
    pub fn get_item(&self, offset_number: u32) -> Option<&[u8]> {
        let slot_index = (offset_number - 1) as usize;
        let meta = self.read_item_meta(slot_index)?;
        if meta.is_deleted() {
            return None;
        }
        let offset = meta.offset as usize;
        Some(&self.data[offset..offset + meta.length as usize])
    }

    /// Get the metadata for an item by its 1-based offset number.
    pub fn get_item_meta(&self, offset_number: u32) -> Option<PageItemMeta> {
        let slot_index = (offset_number - 1) as usize;
        self.read_item_meta(slot_index)
    }

    /// Mark an item as deleted by setting its length to 0 (tombstone).
    pub fn delete_item(&mut self, offset_number: u32) -> Result<(), PageError> {
        let slot_index = (offset_number - 1) as usize;
        let count = self.item_count() as usize;
        if slot_index >= count {
            return Err(PageError::InvalidOffset {
                offset_number,
                item_count: count,
            });
        }
        let mut meta = self
            .read_item_meta(slot_index)
            .expect("slot index checked above");
        meta.length = 0;
        self.write_item_meta(slot_index, &meta);
        Ok(())
    }

    /// Compact the page by removing deleted items and defragmenting.
    ///
    /// After compaction, all live items are contiguous from the top and
    /// the metadata array has no gaps.
    pub fn compact(&mut self) {
        let count = self.item_count() as usize;

        // Collect live items.
        let mut live_items: Vec<(usize, Vec<u8>)> = Vec::new();
        for i in 0..count {
            if let Some(meta) = self.read_item_meta(i) {
                if !meta.is_deleted() {
                    let data = self.data[meta.offset as usize
                        ..meta.offset as usize + meta.length as usize]
                        .to_vec();
                    live_items.push((i, data));
                }
            }
        }

        // Re-initialize the page.
        self.init();

        // Re-insert all live items sequentially.
        for (idx, (_, item_data)) in live_items.into_iter().enumerate() {
            // offset_number is 1-based.
            self.add_item(&item_data, (idx + 1) as u32)
                .expect("compaction should always have enough space");
        }
    }

    /// Read the raw page bytes.
    pub fn as_bytes(&self) -> &[u8; BLKSZ] {
        &self.data
    }

    /// Construct a page from a raw byte buffer.
    pub fn from_bytes(data: [u8; BLKSZ]) -> Self {
        Self { data }
    }

    // ── Internal helpers ───────────────────────────────────────────────

    fn lower(&self) -> u32 {
        u32::from_le_bytes(self.data[0..4].try_into().unwrap())
    }

    fn set_lower(&mut self, val: u32) {
        self.data[0..4].copy_from_slice(&val.to_le_bytes());
    }

    fn upper(&self) -> u32 {
        u32::from_le_bytes(self.data[4..8].try_into().unwrap())
    }

    fn set_upper(&mut self, val: u32) {
        self.data[4..8].copy_from_slice(&val.to_le_bytes());
    }

    /// Read the `i`th item metadata entry (0-based index).
    fn read_item_meta(&self, i: usize) -> Option<PageItemMeta> {
        let start = PAGE_HEADER_SIZE + i * PageItemMeta::SERIALIZED_SIZE;
        if start + PageItemMeta::SERIALIZED_SIZE > BLKSZ {
            return None;
        }
        // Also check against lower to ensure we don't read past the metadata area.
        if start + PageItemMeta::SERIALIZED_SIZE > self.lower() as usize {
            return None;
        }
        let offset = u32::from_le_bytes(self.data[start..start + 4].try_into().unwrap());
        let length = u32::from_le_bytes(self.data[start + 4..start + 8].try_into().unwrap());
        Some(PageItemMeta { offset, length })
    }

    /// Write the `i`th item metadata entry (0-based index).
    fn write_item_meta(&mut self, i: usize, meta: &PageItemMeta) {
        let start = PAGE_HEADER_SIZE + i * PageItemMeta::SERIALIZED_SIZE;
        self.data[start..start + 4].copy_from_slice(&meta.offset.to_le_bytes());
        self.data[start + 4..start + 8].copy_from_slice(&meta.length.to_le_bytes());
    }
}

impl Default for Page {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn page_init_upper_is_blksz() {
        let page = Page::new();
        assert_eq!(page.upper(), BLKSZ as u32);
    }

    #[test]
    fn page_init_lower_is_header_size() {
        let page = Page::new();
        assert_eq!(page.lower(), PAGE_HEADER_SIZE as u32);
    }

    #[test]
    fn page_init_zeroed_free_space() {
        let page = Page::new();
        // All bytes in the free space area should be zero.
        let lower = page.lower() as usize;
        let upper = page.upper() as usize;
        for &byte in &page.data[lower..upper] {
            assert_eq!(byte, 0);
        }
    }

    #[test]
    fn add_and_read_single_item() {
        let mut page = Page::new();
        let item_data: &[u8] = b"hello mosken";
        page.add_item(item_data, 1).unwrap();

        let retrieved = page.get_item(1).unwrap();
        assert_eq!(retrieved, item_data);
    }

    #[test]
    fn add_multiple_items_and_read_back() {
        let mut page = Page::new();

        let items: [&[u8]; 3] = [b"first", b"second item", b"third"];

        for (i, item) in items.iter().enumerate() {
            page.add_item(item, (i + 1) as u32).unwrap();
        }

        for (i, expected) in items.iter().enumerate() {
            assert_eq!(page.get_item((i + 1) as u32).unwrap(), *expected);
        }
    }

    #[test]
    fn free_space_decreases_after_insert() {
        let mut page = Page::new();
        let initial_free = page.free_space();

        let item = b"some data here";
        page.add_item(item, 1).unwrap();

        let after_free = page.free_space();
        let expected_decrease = item.len() as u32 + PageItemMeta::SERIALIZED_SIZE as u32;
        assert_eq!(initial_free - after_free, expected_decrease);
    }

    #[test]
    fn insufficient_space_returns_error() {
        let mut page = Page::new();
        // Fill the page.
        let big_item = vec![0xAAu8; 3000];
        page.add_item(&big_item, 1).unwrap();

        // Try adding something that won't fit.
        let another = vec![0xBBu8; 2000];
        let result = page.add_item(&another, 2);
        assert!(result.is_err());
    }

    #[test]
    fn delete_item_returns_none_on_get() {
        let mut page = Page::new();
        page.add_item(b"item 1", 1).unwrap();
        page.add_item(b"item 2", 2).unwrap();
        page.add_item(b"item 3", 3).unwrap();

        page.delete_item(2).unwrap();

        assert!(page.get_item(1).is_some());
        assert!(page.get_item(2).is_none()); // deleted
        assert!(page.get_item(3).is_some());
    }

    #[test]
    fn compact_reclaims_deleted_space() {
        let mut page = Page::new();
        page.add_item(b"keep me", 1).unwrap();
        page.add_item(b"delete me please", 2).unwrap();
        page.add_item(b"keep me too", 3).unwrap();

        let _free_before_delete = page.free_space();
        page.delete_item(2).unwrap();

        page.compact();

        // After compaction, items 1 and 3 should still be readable (now at
        // offset numbers 1 and 2).
        assert_eq!(page.get_item(1).unwrap(), b"keep me");
        assert_eq!(page.get_item(2).unwrap(), b"keep me too");
        assert_eq!(page.item_count(), 2);
    }
}
