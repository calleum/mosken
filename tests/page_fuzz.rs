//! Fuzz harness for slotted-page delete + compaction (rung A1).
//!
//! Agent-written scaffolding per the crucible rule — the fixes this surfaces
//! belong in `src/page.rs`, written by the human, not here.
//!
//! Deterministic LCG (no rand dependency). Maintains a mirror model of the
//! page and verifies every invariant after every operation:
//!   - `item_count()` matches the model
//!   - `free_space()` matches the model byte-for-byte
//!   - `get_item(n)` matches the model for every slot, live and deleted

use mosken::page::{Page, PageItemMeta, BLKSZ, PAGE_HEADER_SIZE};

struct Model {
    /// `slots[k]` = Some(bytes) if offset number k+1 is live.
    slots: Vec<Option<Vec<u8>>>,
    /// Bytes carved off the top of the page since the last compaction.
    upper: usize,
}

impl Model {
    fn lower(&self) -> usize {
        PAGE_HEADER_SIZE + self.slots.len() * PageItemMeta::SERIALIZED_SIZE
    }

    fn free(&self) -> usize {
        self.upper - self.lower()
    }
}

/// Tiny LCG so the harness needs no rand dependency.
struct Lcg(u64);

impl Lcg {
    fn next(&mut self) -> u64 {
        self.0 = self
            .0
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        self.0 >> 33
    }

    fn below(&mut self, n: u64) -> u64 {
        self.next() % n
    }
}

fn gen_item(rng: &mut Lcg) -> Vec<u8> {
    let roll = rng.below(16);
    let len = match roll {
        0 => 0,                              // empty: legal API input
        1..=3 => 1 + rng.below(8) as usize,  // tiny
        4 => 400 + rng.below(3600) as usize, // large
        _ => 1 + rng.below(100) as usize,    // typical
    };
    (0..len).map(|_| rng.next() as u8).collect()
}

fn verify(page: &Page, model: &Model, seed: u64, op_i: usize) {
    assert_eq!(
        page.item_count() as usize,
        model.slots.len(),
        "seed {seed} op {op_i}: item_count"
    );
    assert_eq!(
        page.free_space() as usize,
        model.free(),
        "seed {seed} op {op_i}: free_space"
    );
    for k in 0..model.slots.len() {
        let expected = model.slots[k].as_deref();
        let actual = page.get_item((k + 1) as u32);
        assert_eq!(
            actual,
            expected,
            "seed {seed} op {op_i}: slot {} content",
            k + 1
        );
    }
}

fn fuzz_seed(seed: u64, ops: usize) {
    let mut rng = Lcg(seed | 1);
    let mut page = Page::new();
    let mut model = Model {
        slots: Vec::new(),
        upper: BLKSZ,
    };

    for op_i in 0..ops {
        match rng.below(4) {
            // Add: append, or reuse a deleted slot.
            op @ (0 | 1) => {
                let item = gen_item(&mut rng);
                let reuse = if op == 1 {
                    model.slots.iter().position(|s| s.is_none())
                } else {
                    None
                };
                let offset_number = match reuse {
                    Some(k) => (k + 1) as u32,
                    None => (model.slots.len() + 1) as u32,
                };
                let would_fit = model.free() >= item.len() + PageItemMeta::SERIALIZED_SIZE;
                let result = page.add_item(&item, offset_number);
                match result {
                    Ok(()) => {
                        assert!(
                            would_fit,
                            "seed {seed} op {op_i}: add succeeded but model says full"
                        );
                        match reuse {
                            Some(k) => model.slots[k] = Some(item.clone()),
                            None => model.slots.push(Some(item.clone())),
                        }
                        model.upper -= item.len();
                    }
                    Err(_) => {
                        assert!(
                            !would_fit,
                            "seed {seed} op {op_i}: add refused but model says it fits"
                        );
                    }
                }
            }
            // Delete a random live slot. No byte space moves: the tombstone
            // only changes the slot entry. That stranded space is exactly
            // what compact() must reclaim.
            2 => {
                let live: Vec<usize> = (0..model.slots.len())
                    .filter(|&k| model.slots[k].is_some())
                    .collect();
                if !live.is_empty() {
                    let k = live[rng.below(live.len() as u64) as usize];
                    page.delete_item((k + 1) as u32)
                        .unwrap_or_else(|e| panic!("seed {seed} op {op_i}: delete: {e}"));
                    model.slots[k] = None;
                }
            }
            // Compact: live items keep relative order, offset numbers close gaps.
            _ => {
                page.compact();
                let live: Vec<Vec<u8>> = model.slots.iter().filter_map(|s| s.clone()).collect();
                model.upper = BLKSZ - live.iter().map(|v| v.len()).sum::<usize>();
                model.slots = live.into_iter().map(Some).collect();
            }
        }
        verify(&page, &model, seed, op_i);
    }
}

#[test]
fn slotted_page_survives_fuzz() {
    for seed in [1, 2, 3, 7, 42, 12345, 0xBEEF, 0xDEAD] {
        fuzz_seed(seed, 2000);
    }
}
