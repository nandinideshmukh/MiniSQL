#include "btree.h"
#include "buffer.h"
#include <string.h>
#include <stdio.h>

void btree_init_node(Page *page, bool is_leaf, bool is_root, uint32_t parent_page_id) {
    BTreeHeader *header = (BTreeHeader*)page->data;
    header->is_leaf = is_leaf;
    header->is_root = is_root;
    header->parent_page_id = parent_page_id;
    header->num_keys = 0;
    header->next_leaf_id = 0; // Or whatever signifies 'none'
}

BTreeHeader* btree_get_header(Page *page) {
    return (BTreeHeader*)page->data;
}

// Very simplified find: sequential scan for now
bool btree_leaf_find(Page *page, BTreeKey key, RecordID *out_rid) {
    BTreeHeader *header = btree_get_header(page);
    if (!header->is_leaf) return false;

    LeafEntry *entries = (LeafEntry*)(page->data + sizeof(BTreeHeader));
    for (uint16_t i = 0; i < header->num_keys; i++) {
        if (strncmp(entries[i].key, key, 64) == 0) {
            *out_rid = entries[i].rid;
            return true;
        }
    }
    return false;
}

// Very simplified insert: assumes space exists and keys are sorted (or appends for now)
bool btree_leaf_insert(Page *page, BTreeKey key, RecordID rid) {
    BTreeHeader *header = btree_get_header(page);
    if (!header->is_leaf) return false;
    
    if (header->num_keys >= BTREE_LEAF_MAX_KEYS) {
        return false; // Full
    }

    LeafEntry *entries = (LeafEntry*)(page->data + sizeof(BTreeHeader));
    
    // Find position to insert (keeping sorted)
    int16_t insert_idx = header->num_keys;
    for (int16_t i = 0; i < header->num_keys; i++) {
        int cmp = strncmp(entries[i].key, key, 64);
        if (cmp > 0) {
            insert_idx = i;
            break;
        }
        if (cmp == 0) {
            return false; // Duplicate key
        }
    }

    // Shift elements
    for (int16_t i = header->num_keys; i > insert_idx; i--) {
        entries[i] = entries[i-1];
    }

    // Insert
    strncpy(entries[insert_idx].key, key, 64);
    entries[insert_idx].rid = rid;
    header->num_keys++;

    return true;
}

bool btree_leaf_delete(Page *page, BTreeKey key, RecordID *out_rid) {
    BTreeHeader *header = btree_get_header(page);
    if (!header->is_leaf) return false;

    LeafEntry *entries = (LeafEntry*)(page->data + sizeof(BTreeHeader));
    int found_idx = -1;
    for (uint16_t i = 0; i < header->num_keys; i++) {
        if (strncmp(entries[i].key, key, 64) == 0) {
            found_idx = i;
            break;
        }
    }
    if (found_idx < 0) return false;

    if (out_rid) *out_rid = entries[found_idx].rid;
    for (uint16_t i = (uint16_t)found_idx; i + 1 < header->num_keys; i++) {
        entries[i] = entries[i + 1];
    }
    header->num_keys--;
    return true;
}

// B-Tree High-level Operations using BufferPool

// Search the tree starting from root_page_id for a given key.
// Returns true and sets out_rid if found.
bool btree_search(BufferPool *pool, uint32_t root_page_id, BTreeKey key, RecordID *out_rid) {
    uint32_t current_page_id = root_page_id;

    while (true) {
        Page *current_page = fetch_page(pool, current_page_id);
        if (!current_page) return false;

        BTreeHeader *header = btree_get_header(current_page);

        if (header->is_leaf) {
            bool found = btree_leaf_find(current_page, key, out_rid);
            unpin_page(pool, current_page_id, false);
            return found;
        } else {
            // Find child pointer in internal node
            // Internal node format: [Header][child0][key0, child1][key1, child2]...
            uint32_t *first_child = (uint32_t*)(current_page->data + sizeof(BTreeHeader));
            InternalEntry *entries = (InternalEntry*)(current_page->data + sizeof(BTreeHeader) + sizeof(uint32_t));
            
            uint32_t next_page_id = *first_child;
            for (uint16_t i = 0; i < header->num_keys; i++) {
                if (strncmp(key, entries[i].key, 64) >= 0) {
                    next_page_id = entries[i].child_page_id;
                } else {
                    break;
                }
            }
            unpin_page(pool, current_page_id, false);
            current_page_id = next_page_id;
        }
    }
}
