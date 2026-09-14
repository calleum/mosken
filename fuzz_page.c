/*
 * Fuzz harness for slotted-page delete + compaction.
 *
 * Model-checked fuzzing: a plain mirror model holds what the page should
 * contain, and every observable is verified against it after each op.
 *
 * CONTRACT — implement these three in mosken.c, then move the
 * declarations into mosken.h:
 *
 *   void page_delete_item(Page page, OffsetNum offset_number);
 *       Tombstone the slot. The item's bytes stay in place; lower and
 *       upper do not move. Slot-state encoding: design decision;
 *       document it in a comment above the implementation.
 *       Trap: a live zero-length item must stay distinguishable from a
 *       tombstone (PG trick: lp_off == 0 means "no storage"; alternatively
 *       reject empty items in add_page_item). This harness inserts empty
 *       items and requires them to stay live. If empty items are instead
 *       rejected at add time, update gen_item() here accordingly.
 *
 *   void page_compact(Page page);
 *       Reclaim dead slots and stranded bytes. Live items keep their
 *       relative order and are renumbered 1..n with no gaps. After it
 *       returns: lower == size_of_page_header + n*8 and
 *       upper == BLKSZ - (bytes of live items). Exactly.
 *
 *   int page_item_is_live(Page page, OffsetNum offset_number);
 *       1 if the slot is live, 0 if tombstoned. (Hides the encoding.)
 *
 * Existing add_page_item contract kept as-is: the caller must ensure free
 * space (it asserts otherwise, strictly `free > need`), so the harness
 * only ever adds when that predicate holds. Note the boundary: an
 * exactly-fitting add asserts — PG returns a failure there instead. If
 * the check loosens to `>=`, adjust WOULD_FIT below.
 *
 * Current add_page_item has no slot-reuse concept (it moves lower on
 * every write): reusing a dead slot is part of this rung. The model below
 * expects reuse to move ONLY upper.
 *
 * Set FUZZ_TRACE=1 to print one line per op plus page state (stderr, so
 * it survives the assert abort even when piped through tail).
 */

#include "mosken.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Implement in mosken.c, declare in mosken.h: */
void page_delete_item(Page page, OffsetNum offset_number);
void page_compact(Page page);
int page_item_is_live(Page page, OffsetNum offset_number);

#define MAX_SLOTS 640 /* (BLKSZ - 8) / 8 = 636 max; headroom */
#define MAX_ITEM 4096

static uint64_t g_seed;
static uint32_t g_op;
static int g_trace;

/* Trace goes to stderr: unbuffered, so it is already out when assert(0)
 * aborts. stdout would be buffer-flushed away by the abort. */
static void
trace_op(const char *fmt, ...)
{
    va_list ap;
    if(!g_trace)
        return;
    fprintf(stderr, "seed %llu op %u: ", (unsigned long long)g_seed, g_op);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
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

/*
 * Mirror model of what the page SHOULD hold. Invariants checked after
 * every operation: lower, upper, item count, and every slot's content and
 * liveness. upper is tracked explicitly (deletes strand bytes; only
 * compaction makes it equal the live total again).
 */
struct model
{
    uint32_t nslots;
    uint32_t upper;
    int live[MAX_SLOTS];
    uint32_t len[MAX_SLOTS];
    unsigned char data[MAX_SLOTS][MAX_ITEM];
};

static uint32_t model_lower(const struct model *m)
{
    return (uint32_t)(size_of_page_header
                      + (size_t)m->nslots * sizeof(PageItemMeta));
}

static void trace_state(Page page, const struct model *m)
{
    PgHeader p = (PgHeader)page;
    uint32_t nlive = 0;
    for(uint32_t k = 0; k < m->nslots; k++)
        nlive += (m->live[k] != 0);
    if(g_trace)
        fprintf(stderr, "    state: lower=%u upper=%u slots=%u live=%u\n",
                p->pgh_lower, p->pgh_upper, m->nslots, nlive);
}

static void gen_item(unsigned char *buf, uint32_t *len_out)
{
    uint32_t roll = rng_below(16);
    uint32_t len;
    if(roll == 0)
        len = 0; /* empty: legal API input, the tombstone trap */
    else if(roll <= 3)
        len = 1 + rng_below(8);
    else if(roll == 4)
        len = 400 + rng_below(3600);
    else
        len = 1 + rng_below(100);

    for(uint32_t i = 0; i < len; i++)
        buf[i] = (unsigned char)rng_next();
    *len_out = len;
}

static void verify(Page page, const struct model *m)
{
    PgHeader p = (PgHeader)page;

    CHECK(p->pgh_lower == model_lower(m), "lower %u, model %u", p->pgh_lower,
          model_lower(m));
    CHECK(p->pgh_upper == m->upper, "upper %u, model %u", p->pgh_upper,
          m->upper);
    CHECK(page_free_space(page) == m->upper - model_lower(m),
          "free_space %u, model %u", page_free_space(page),
          m->upper - model_lower(m));

    for(uint32_t k = 0; k < m->nslots; k++)
    {
        OffsetNum onum = k + 1;
        PgItemId id = page_get_item_id(page, onum);
        CHECK(page_item_is_live(page, onum) == (m->live[k] ? 1 : 0),
              "slot %u liveness %d, model %d", onum,
              page_item_is_live(page, onum), m->live[k]);
        if(!m->live[k])
            continue;
        CHECK(id->pgi_length == m->len[k], "slot %u len %u, model %u", onum,
              id->pgi_length, m->len[k]);
        CHECK(memcmp(page_item(page, id), m->data[k], (size_t)m->len[k]) == 0,
              "slot %u content mismatch", onum);
    }
}

static void fuzz_seed(uint64_t seed, uint32_t ops)
{
    static struct model m;
    static unsigned char buf[MAX_ITEM];
    Page page = malloc(BLKSZ);

    g_seed = seed;
    g_rng = seed | 1;
    g_trace = getenv("FUZZ_TRACE") != NULL;
    memset(&m, 0, sizeof m);
    page_init(page);
    m.upper = BLKSZ;

    for(g_op = 0; g_op < ops; g_op++)
    {
        uint32_t roll = rng_below(4);
        if(roll <= 1)
        {
            /* Add: append (roll 0) or reuse a dead slot (roll 1). */
            uint32_t len;
            gen_item(buf, &len);
            uint32_t need = len + (uint32_t)sizeof(PageItemMeta);
            uint32_t free_now = m.upper - model_lower(&m);

            /* WOULD_FIT mirrors add_page_item's strict `free > need`. */
            int would_fit = free_now > need;
            if(!would_fit)
            {
                trace_op("ADD skipped (page full, len=%u)", len);
                continue; /* the implementation asserts; never trip it */
            }

            OffsetNum onum;
            const char *kind;
            if(roll == 1)
            {
                uint32_t dead[MAX_SLOTS], ndead = 0;
                for(uint32_t k = 0; k < m.nslots; k++)
                    if(!m.live[k])
                        dead[ndead++] = k;
                if(ndead == 0)
                    roll = 0; /* nothing to reuse: fall through to append */
                else
                {
                    uint32_t k = dead[rng_below(ndead)];
                    onum = k + 1;
                    kind = "reuse";
                    m.live[k] = 1;
                    m.len[k] = len;
                    memcpy(m.data[k], buf, (size_t)len);
                    m.upper -= len; /* reuse moves ONLY upper */
                }
            }
            if(roll == 0)
            {
                CHECK(m.nslots < MAX_SLOTS, "model slot overflow");
                onum = m.nslots + 1;
                kind = "append";
                m.live[m.nslots] = 1;
                m.len[m.nslots] = len;
                memcpy(m.data[m.nslots], buf, (size_t)len);
                m.nslots++;
                m.upper -= len;
            }
            trace_op("ADD %s slot %u len=%u (free before %u)", kind, onum,
                     len, free_now);
            page_add_item(page, (Item)buf, (Size)len, onum);
        }
        else if(roll == 2)
        {
            /* Delete a random live slot. Nothing moves but the slot. */
            uint32_t live[MAX_SLOTS], nlive = 0;
            for(uint32_t k = 0; k < m.nslots; k++)
                if(m.live[k])
                    live[nlive++] = k;
            if(nlive == 0)
            {
                trace_op("DELETE skipped (no live items)");
                continue;
            }
            uint32_t k = live[rng_below(nlive)];
            trace_op("DELETE slot %u", k + 1);
            page_delete_item(page, k + 1);
            m.live[k] = 0;
        }
        else
        {
            /* Compact: live items keep order, gaps close, space reclaims. */
            uint32_t nslots_before = m.nslots;
            uint32_t sum = 0, w = 0;
            for(uint32_t k = 0; k < m.nslots; k++)
            {
                if(!m.live[k])
                    continue;
                m.live[w] = 1;
                m.len[w] = m.len[k];
                memcpy(m.data[w], m.data[k], (size_t)m.len[k]);
                sum += m.len[k];
                w++;
            }
            m.nslots = w;
            m.upper = (uint32_t)BLKSZ - sum;
            trace_op("COMPACT (%u live of %u slots, live bytes %u)", w,
                     nslots_before, sum);
            page_compact(page);
        }
        verify(page, &m);
        trace_state(page, &m);
    }
    free(page);
}

int main(void)
{
    static const uint64_t seeds[] = { 1, 2, 3, 7, 42, 12345, 0xBEEF, 0xDEAD };
    for(size_t i = 0; i < sizeof seeds / sizeof seeds[0]; i++)
    {
        fuzz_seed(seeds[i], 2000);
        printf("seed %llu: ok\n", (unsigned long long)seeds[i]);
    }
    printf("fuzz: 8 seeds x 2000 ops passed\n");
    return EXIT_SUCCESS;
}
