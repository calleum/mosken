/*
 * Fuzz harness for the single-page free space map (rung A2).
 *
 * HOW THE CHECKER WORKS
 *   It keeps a mirror model with the exact free bytes on each heap page and
 *   checks the FSM against that model after every operation. The model
 *   pre-checks every predicate the implementation may assume. An assert
 *   failure is therefore a contract bug, not a coverage event.
 *
 * FSM CONTRACT
 *   Implement these four functions in fsm.c. fsm.h provides the layout
 *   constants and prototypes; do not change them.
 *
 *   fsm_init(Page page)
 *     Set every node byte and the next-slot pointer to 0.
 *
 *   fsm_set_avail(Page page, uint32_t page_no, uint8_t cat)
 *     Store `cat` in the leaf for heap page `page_no`. Then walk toward the
 *     root, setting each ancestor to the maximum of its two children. Stop
 *     when an ancestor's value does not change.
 *
 *   fsm_search_avail(Page page, uint8_t min_cat, uint32_t *page_no_out)
 *     Find a heap page whose category is at least `min_cat`. On success,
 *     store its page number in `*page_no_out` and return 1. If the root is
 *     below `min_cat`, return 0. Any traversal order is valid: the checker
 *     only requires a matching page and a result consistent with the
 *     model's maximum category.
 *
 *     The caller guarantees `min_cat >= 1`. Unwritten leaf slots contain 0
 *     and must not be returned; 0 means "full" for every valid request.
 *
 *   fsm_root_value(Page page)
 *     Return the root node's category (the maximum across all heap pages).
 *
 * PAGE LAYOUT
 *   Fixed by fsm.h; mirrors PostgreSQL's FSM design.
 *
 *   - Bytes 4..7 hold the little-endian u32 next-slot pointer,
 *     `fp_next_slot`. fsm_init sets it to 0; the implementation may rotate
 *     it freely.
 *   - Node bytes start at FSM_NODES_START = 8, with one byte per node.
 *   - Nodes use level order: slot 0 is the root; slot i's children are
 *     2i+1 and 2i+2, and its parent is (i-1)/2.
 *   - Leaves occupy LEAF_START..FSM_NODES-1, one per heap page. The tree is
 *     not perfect, so unused rightmost leaves may remain 0.
 *   - The 4088 available bytes fit a complete tree with NSLOTS = 2044
 *     leaves: FSM_NODES = 2*NSLOTS - 1 = 4087 nodes, in slots 0..4086.
 *
 * CATEGORY ARITHMETIC
 *   This matches PostgreSQL's fsm_space_avail_to_cat exactly:
 *
 *     cat = min(255, free / 16)  // 16 = BLKSZ/256 for BLKSZ 4096
 *     satisfies request `need` iff cat >= ceil(need / 16)
 *
 *   Store rounds down; search rounds up. These operations are not symmetric.
 *
 * Set FUZZ_TRACE=1 for one line per operation plus FSM state
 * (stderr, unbuffered).
 */

#include "mosken.h"
#include "heapfile.h"
#include "fsm.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CAT_UNITS 16 /* BLKSZ / 256 */
#define PAGE_DATA (BLKSZ - 8) /* A1 layout: free space = upper - lower */
#define MODEL_MAX_PAGES 512

static uint64_t g_seed;
static uint32_t g_op;
static int g_trace;

static void trace_op(const char *fmt, ...)
{
    va_list ap;
    if(!g_trace)
        return;
    fprintf(stderr, "seed %llu op %u: ", (unsigned long long)g_seed, g_op);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

#define CHECK(cond, ...)                                                      \
    do                                                                        \
    {                                                                         \
        if(!(cond))                                                           \
        {                                                                     \
            fprintf(stderr, "seed %llu op %u: ", (unsigned long long)g_seed,  \
                    g_op);                                                    \
            fprintf(stderr, __VA_ARGS__);                                     \
            fprintf(stderr, "\n");                                            \
            assert(0);                                                        \
        }                                                                     \
    }                                                                         \
    while(0)

/* Deterministic LCG: same stream on every platform, no rand(). */
static uint64_t g_rng;

static uint64_t rng_next(void)
{
    g_rng = g_rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return g_rng >> 33;
}

static uint32_t rng_below(uint32_t n) { return (uint32_t)(rng_next() % n); }

/* Mirror model: exact free bytes per heap page (not categories), so the
 * checker is independent of the rounding the FSM performs. */
struct model
{
    uint32_t npages;
    uint16_t free[MODEL_MAX_PAGES]; /* exact free bytes, 0..4088 */
};

static uint8_t cat_of(uint16_t free_bytes)
{
    uint32_t c = free_bytes / CAT_UNITS;
    return (uint8_t)(c > 255 ? 255 : c);
}

static uint8_t model_max_cat(const struct model *m)
{
    uint8_t max = 0;
    for(uint32_t i = 0; i < m->npages; i++)
    {
        uint8_t c = cat_of(m->free[i]);
        if(c > max)
            max = c;
    }
    return max;
}

/* Structural check: leaf values match the model's categories, every
 * internal node is the max of its two children, root == max leaf. */
static void verify_structure(Page page, const struct model *m)
{
    const unsigned char *nodes = (const unsigned char *)page + FSM_NODES_START;

    for(uint32_t p = 0; p < m->npages; p++)
        CHECK(nodes[LEAF_START + p] == cat_of(m->free[p]),
              "leaf %u: stored %u, model cat %u (free %u)", p,
              nodes[LEAF_START + p], cat_of(m->free[p]), m->free[p]);

    for(uint32_t i = 0; i < LEAF_START; i++)
    {
        unsigned l = nodes[2 * i + 1], r = nodes[2 * i + 2];
        unsigned mx = l > r ? l : r;
        CHECK(nodes[i] == mx, "internal %u: stored %u, children max %u", i,
              nodes[i], mx);
    }

    CHECK(fsm_root_value(page) == model_max_cat(m), "root %u, model max %u",
          fsm_root_value(page), model_max_cat(m));
}

/* Leaf slots beyond npages must hold 0 (search must never return them). */
static void verify_unused_leaves(Page page, uint32_t npages)
{
    const unsigned char *nodes = (const unsigned char *)page + FSM_NODES_START;
    for(uint32_t p = npages; p < NSLOTS; p++)
        CHECK(nodes[LEAF_START + p] == 0, "unused leaf %u: %u", p,
              nodes[LEAF_START + p]);
    CHECK(nodes[FSM_NODES] == 0, "spare byte after last node: %u",
          nodes[FSM_NODES]);
}

static void fuzz_seed(uint64_t seed, uint32_t ops)
{
    static struct model m;
    Page page = malloc(BLKSZ);
    const char *hf_path = "/tmp/mosken-fsm-fuzz.dat";
    unlink(hf_path); /* fresh file even after an aborted run */
    HeapFile *hf = hf_open(hf_path);
    CHECK(hf != NULL, "hf_open failed");

    g_seed = seed;
    g_rng = seed | 1;
    g_trace = getenv("FUZZ_TRACE") != NULL;
    memset(&m, 0, sizeof m);

    /* Fresh FSM + heap: one page exists, entirely free. */
    fsm_init(page);
    m.npages = 1;
    m.free[0] = PAGE_DATA;
    fsm_set_avail(page, 0, cat_of(m.free[0]));
    hf_extend(hf);

    for(g_op = 0; g_op < ops; g_op++)
    {
        uint32_t roll = rng_below(10);
        if(roll <= 3)
        {
            /* INSERT of `need` bytes: search the FSM, extend when none. */
            uint32_t need = 1 + rng_below(200);
            uint8_t min_cat = (uint8_t)((need + CAT_UNITS - 1) / CAT_UNITS);
            uint32_t pg = 0;
            int found = fsm_search_avail(page, min_cat, &pg);

            int model_found = 0;
            for(uint32_t i = 0; i < m.npages; i++)
                if(cat_of(m.free[i]) >= min_cat)
                {
                    model_found = 1;
                    break;
                }

            CHECK(found == model_found,
                  "INSERT need=%u min_cat=%u: fsm found %d, model %d", need,
                  min_cat, found, model_found);
            if(found)
            {
                CHECK(pg < m.npages,
                      "returned page %u out of range (npages %u)", pg,
                      m.npages);
                CHECK(cat_of(m.free[pg]) >= min_cat,
                      "returned page %u cat %u < min_cat %u", pg,
                      cat_of(m.free[pg]), min_cat);
                m.free[pg] -= (uint16_t)need;
                fsm_set_avail(page, pg, cat_of(m.free[pg]));
                trace_op("INSERT %u -> page %u (free now %u)", need, pg,
                         m.free[pg]);
            }
            else
            {
                CHECK(m.npages < MODEL_MAX_PAGES, "model page overflow");
                CHECK(m.npages < NSLOTS, "leaf slot overflow");
                m.free[m.npages] = (uint16_t)(PAGE_DATA - need);
                m.npages++;
                hf_extend(hf);
                fsm_set_avail(page, m.npages - 1, cat_of(m.free[m.npages - 1]));
                trace_op("INSERT %u -> EXTEND to %u pages", need, m.npages);
            }
            CHECK(m.npages <= (uint32_t)hf_num_pages(hf),
                  "model npages %u > file pages %u", m.npages,
                  (uint32_t)hf_num_pages(hf));
        }
        else if(roll <= 6)
        {
            /* Delete-like release: pick a page, free r bytes of its used
             * space (bounded so free never exceeds PAGE_DATA). */
            uint32_t p = rng_below(m.npages);
            uint32_t r = 1 + rng_below(200);
            uint32_t used = PAGE_DATA - m.free[p];
            if(r > used)
                r = used;
            if(r == 0)
            {
                trace_op("FREE skipped (page %u already empty)", p);
                continue;
            }
            m.free[p] += (uint16_t)r;
            fsm_set_avail(page, p, cat_of(m.free[p]));
            trace_op("FREE %u on page %u (free now %u)", r, p, m.free[p]);
        }
        else
        {
            verify_structure(page, &m);
            verify_unused_leaves(page, m.npages);
            trace_op("VERIFY (%u pages, root %u)", m.npages,
                     fsm_root_value(page));
        }
    }
    verify_structure(page, &m);
    verify_unused_leaves(page, m.npages);
    printf("seed %llu: ok (%u pages)\n", (unsigned long long)seed, m.npages);
    hf_close(hf);
    free(page);
}

int main(void)
{
    static const uint64_t seeds[] = { 1, 2, 3, 7, 42, 12345, 0xBEEF, 0xDEAD };
    for(size_t i = 0; i < sizeof seeds / sizeof seeds[0]; i++)
        fuzz_seed(seeds[i], 2000);
    printf("fuzz-fsm: 8 seeds x 2000 ops passed\n");
    return EXIT_SUCCESS;
}
