#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stdbool.h>
#include "page.h"

// A Record ID (RID) points to a specific record in a heap page
typedef struct {
    uint32_t page_id;
    uint16_t slot_id;
} RecordID;

// B+ tree node header (metadata), placed at the start of a 4096-byte Page
typedef struct {
    bool is_leaf;
    bool is_root;
    uint32_t page_id;
    uint32_t parent_page_id;  // 0 if root
    uint16_t num_keys;        // current number of keys in the node
    uint32_t next_leaf_id;    // for leaf nodes only: pointer to next leaf for range scans
} BTreeHeader;

// For now, assuming string keys up to 64 bytes
typedef char BTreeKey[64];

// Leaf Node Layout:
// [ BTreeHeader ] [ BTreeKey array ] [ RecordID array ]
// Or interleaved array of struct { BTreeKey, RecordID }. Let's define the pair:
typedef struct {
    char key[64]; // String key instead of integer to support names
    RecordID rid;
} LeafEntry;

// Internal Node Layout:
typedef struct {
    char key[64];
    uint32_t child_page_id;
} InternalEntry;

// Max keys calculation based on PAGE_SIZE
#define BTREE_LEAF_MAX_KEYS   ((PAGE_SIZE - sizeof(BTreeHeader)) / sizeof(LeafEntry))
#define BTREE_INTERNAL_MAX_KEYS ((PAGE_SIZE - sizeof(BTreeHeader) - sizeof(uint32_t)) / sizeof(InternalEntry))

// Function prototypes to manage nodes mapped onto a Page
void btree_init_node(Page *page, bool is_leaf, bool is_root, uint32_t parent_page_id);
BTreeHeader* btree_get_header(Page *page);

// Operations on Leaf Nodes
bool btree_leaf_insert(Page *page, BTreeKey key, RecordID rid);
bool btree_leaf_find(Page *page, BTreeKey key, RecordID *out_rid);
bool btree_leaf_delete(Page *page, BTreeKey key, RecordID *out_rid);

// Operations on Internal Nodes
void btree_internal_insert(Page *page, BTreeKey key, uint32_t child_page_id);

#endif // BTREE_H
