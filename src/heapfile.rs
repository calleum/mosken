//! Heap file manager for the Mosken storage engine.
//!
//! A heap file is a simple collection of fixed-size pages (4 KiB) stored
//! sequentially in a single file on disk. Pages are addressed by their
//! 0-based ID.

use std::fs::{File, OpenOptions};
use std::io::{self, Read, Seek, SeekFrom, Write};
use std::path::Path;

use crate::page::{Page, BLKSZ};

/// Errors returned by heap file operations.
#[derive(Debug, thiserror::Error)]
pub enum HeapFileError {
    #[error("I/O error: {0}")]
    Io(#[from] io::Error),

    #[error("page ID {page_id} is out of bounds (file has {num_pages} pages)")]
    InvalidPageId { page_id: u64, num_pages: u32 },
}

/// A heap file backed by a single OS file.
///
/// Each page is `BLKSZ` (4096) bytes. Pages are numbered starting from 0.
pub struct HeapFile {
    file: File,
}

impl HeapFile {
    /// Open an existing heap file, or create it if it doesn't exist.
    pub fn open(path: &Path) -> Result<Self, HeapFileError> {
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .open(path)?;
        Ok(Self { file })
    }

    /// Close the heap file (flushes any pending writes).
    pub fn close(mut self) -> Result<(), HeapFileError> {
        self.file.flush()?;
        // File is dropped here, which closes the file descriptor.
        Ok(())
    }

    /// Return the number of pages in the file.
    pub fn num_pages(&mut self) -> Result<u32, HeapFileError> {
        let file_size = self.file.seek(SeekFrom::End(0))?;
        Ok((file_size / BLKSZ as u64) as u32)
    }

    /// Append a new empty page to the end of the file.
    ///
    /// Returns the page ID of the newly created page.
    pub fn extend(&mut self) -> Result<u32, HeapFileError> {
        let page_id = self.num_pages()?;
        let page = Page::new();
        self.file.seek(SeekFrom::End(0))?;
        self.file.write_all(page.as_bytes())?;
        self.file.flush()?;
        Ok(page_id)
    }

    /// Read a page from the file by its ID.
    pub fn read_page(&mut self, page_id: u64) -> Result<Page, HeapFileError> {
        let num_pages = self.num_pages()?;
        if page_id >= num_pages as u64 {
            return Err(HeapFileError::InvalidPageId {
                page_id,
                num_pages,
            });
        }

        let offset = page_id * BLKSZ as u64;
        self.file.seek(SeekFrom::Start(offset))?;

        let mut buf = [0u8; BLKSZ];
        self.file.read_exact(&mut buf)?;

        Ok(Page::from_bytes(buf))
    }

    /// Write a page to the file at the given page ID.
    pub fn write_page(&mut self, page_id: u64, page: &Page) -> Result<(), HeapFileError> {
        let num_pages = self.num_pages()?;
        if page_id >= num_pages as u64 {
            return Err(HeapFileError::InvalidPageId {
                page_id,
                num_pages,
            });
        }

        let offset = page_id * BLKSZ as u64;
        self.file.seek(SeekFrom::Start(offset))?;
        self.file.write_all(page.as_bytes())?;
        self.file.flush()?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;

    fn test_heapfile() -> (HeapFile, NamedTempFile) {
        let tmp = NamedTempFile::new().unwrap();
        let hf = HeapFile::open(tmp.path()).unwrap();
        (hf, tmp)
    }

    #[test]
    fn open_creates_file() {
        let tmp = NamedTempFile::new().unwrap();
        let path = tmp.path().to_owned();
        // Remove the file so we test creation.
        std::fs::remove_file(&path).unwrap();
        assert!(!path.exists());

        let hf = HeapFile::open(&path).unwrap();
        assert!(path.exists());
        hf.close().unwrap();
    }

    #[test]
    fn num_pages_empty_file() {
        let (mut hf, _tmp) = test_heapfile();
        assert_eq!(hf.num_pages().unwrap(), 0);
    }

    #[test]
    fn extend_adds_pages() {
        let (mut hf, _tmp) = test_heapfile();
        let p0 = hf.extend().unwrap();
        let p1 = hf.extend().unwrap();
        let p2 = hf.extend().unwrap();

        assert_eq!(p0, 0);
        assert_eq!(p1, 1);
        assert_eq!(p2, 2);
        assert_eq!(hf.num_pages().unwrap(), 3);
    }

    #[test]
    fn write_and_read_page_round_trip() {
        let (mut hf, _tmp) = test_heapfile();
        hf.extend().unwrap();

        let mut page = Page::new();
        page.add_item(b"hello from rust", 1).unwrap();
        hf.write_page(0, &page).unwrap();

        let loaded = hf.read_page(0).unwrap();
        assert_eq!(loaded.get_item(1).unwrap(), b"hello from rust");
    }

    #[test]
    fn read_invalid_page_returns_error() {
        let (mut hf, _tmp) = test_heapfile();
        hf.extend().unwrap();

        let result = hf.read_page(5);
        assert!(result.is_err());
    }

    #[test]
    fn write_and_read_five_pages() {
        let (mut hf, _tmp) = test_heapfile();

        for i in 0..5u32 {
            hf.extend().unwrap();
            let mut page = Page::new();
            let data = format!("page {i} data");
            page.add_item(data.as_bytes(), 1).unwrap();
            hf.write_page(i as u64, &page).unwrap();
        }

        for i in 0..5u32 {
            let page = hf.read_page(i as u64).unwrap();
            let expected = format!("page {i} data");
            assert_eq!(page.get_item(1).unwrap(), expected.as_bytes());
        }
    }
}
