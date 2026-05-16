//! Mosken storage engine — demo binary.
//!
//! Mirrors the original C `main.c`: creates a PaymentData record,
//! writes it to a heap file, reads it back, and prints it.

use std::path::Path;

use mosken::heapfile::HeapFile;
use mosken::page::Page;

/// Mirrors the C `PaymentData` struct for the demo.
#[repr(C)]
#[derive(Debug)]
struct PaymentData {
    payment_id: i32,
    payment_name: [u8; 24],
    payment_time: u32,
    total_cents: u32,
}

impl PaymentData {
    fn new(id: i32, name: &str, time: u32, cents: u32) -> Self {
        let mut name_buf = [0u8; 24];
        let bytes = name.as_bytes();
        let copy_len = bytes.len().min(24);
        name_buf[..copy_len].copy_from_slice(&bytes[..copy_len]);
        Self {
            payment_id: id,
            payment_name: name_buf,
            payment_time: time,
            total_cents: cents,
        }
    }

    fn as_bytes(&self) -> &[u8] {
        unsafe {
            std::slice::from_raw_parts(
                self as *const Self as *const u8,
                std::mem::size_of::<Self>(),
            )
        }
    }

    fn from_bytes(bytes: &[u8]) -> &Self {
        unsafe { &*(bytes.as_ptr() as *const Self) }
    }

    fn name_str(&self) -> &str {
        let nul = self
            .payment_name
            .iter()
            .position(|&b| b == 0)
            .unwrap_or(24);
        std::str::from_utf8(&self.payment_name[..nul]).unwrap_or("<invalid>")
    }
}

fn print_payment(p: &PaymentData) {
    println!(
        "Payment {{ payment_id [{}] payment_name [{}] payment_time [{}] total_cents [{}] }}",
        p.payment_id,
        p.name_str(),
        p.payment_time,
        p.total_cents,
    );
}

fn main() {
    let payment = PaymentData::new(1, "Expensive Tuna", 1682331745, 2000);

    // Create an in-memory page and add the payment as an item.
    let mut page = Page::new();
    page.add_item(payment.as_bytes(), 1)
        .expect("failed to add item to page");

    // Write the page to a heap file.
    let db_path = Path::new("mosken.db");
    let mut hf = HeapFile::open(db_path).expect("failed to open heap file");
    hf.extend().unwrap();
    hf.write_page(0, &page).expect("failed to write page");

    // Read the page back from the heap file.
    let page_2 = hf.read_page(0).expect("failed to read page");

    hf.close().unwrap();

    // Print payments from both the in-memory and on-disk pages.
    let item_data = page.get_item(1).expect("item not found in memory page");
    print_payment(PaymentData::from_bytes(item_data));

    let item_data_2 = page_2.get_item(1).expect("item not found in disk page");
    print_payment(PaymentData::from_bytes(item_data_2));
}
