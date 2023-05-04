#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#define MIN_DEGREE 2

typedef uint64_t BptKey;
typedef uint32_t PgId;
typedef uint32_t Offset;
typedef uint8_t BptNumKeys;

typedef struct ItemPtr {
    PgId it_pg_id;
    Offset it_offset;
} ItemPtr;

typedef struct BptLeaf {
    struct BptLeaf *next_leaf;
    struct BptLeaf *prev_leaf;
    ItemPtr *tup_ids;
    BptKey *keys;
} BptLeaf;

typedef struct BptNode {
    BptNumKeys num_keys;
    BptKey *keys;
    struct BptNode *children;
} BptNode;

typedef struct Bptree {
    BptNode root;
} Bptree;

int main(void)
{
    #if 0
    Bptree tree;
    int32_t i = 5;

    ItemPtr tup_one = { .it_pg_id = (PgId)(1 * i) + 1, .it_offset = (Offset)(1 * i) + 1 };
    ItemPtr tup_two = { .it_pg_id = (PgId)(10 * i) + 1, .it_offset = (Offset)(10 * i) + 1 };
    ItemPtr tup_three = { .it_pg_id = (PgId)(15 * i)+ 1, .it_offset = (Offset)(12 * i) + 1 };

    BptKey key_one = 7;
    BptKey key_two = 51;
    BptKey key_three = 76;

    BptNodeData node_one = { .keys = &key_one };
    BptNodeData node_two = { .keys = &key_two };
    BptNodeData node_three = { .keys = &key_three };

    BptLeaf leaf_one = { .keys = &key_one, .tup_ids = &tup_one};
    BptLeaf leaf_two = { .keys = &key_two, .tup_ids = &tup_two, .prev_leaf = &leaf_one};
    BptLeaf leaf_three = { .keys = &key_three, .tup_ids = &tup_three, .prev_leaf = &leaf_two};
    leaf_one.next_leaf = &leaf_two;
    leaf_two.next_leaf = &leaf_three;
    #endif /* if 0 */


}
