#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "executor.h"
#include "buffer.h"
#include "btree.h"
#include "page.h"
#include "wal.h"

#define HEAP_FIRST_PAGE 1
#define V2_MAX_COLUMNS 4
#define HEAP_RECORD_MAGIC 0x56325231u

typedef struct {
    uint32_t magic;
    uint16_t column_count;
    char columns[V2_MAX_COLUMNS][max_len];
    char values[V2_MAX_COLUMNS][max_len];
} HeapRecord;

// Global instances for V2 architecture (in a real DB these would be passed around in a context)
BufferPool g_pool;
WALManager g_wal;
bool g_initialized = false;
uint32_t g_next_txn_id = 1;
static char g_active_db[max_len] = "";

static bool init_v2_engine(const char *db_name, bool create_if_missing) {
    if (g_initialized && strcmp(g_active_db, db_name) == 0) return true;

    char db_filename[max_len + 4];
    snprintf(db_filename, sizeof(db_filename), "%s.v2", db_name);
    if (!create_if_missing) {
        FILE *existing_db = fopen(db_filename, "rb");
        if (!existing_db) return false;
        fclose(existing_db);
    }

    if (g_initialized) shutdown_v2_engine();
    
    char log_filename[max_len + 8];
    // Changed extension to .v2 to guarantee we don't read old V1 flat files!
    snprintf(db_filename, sizeof(db_filename), "%s.v2", db_name);
    snprintf(log_filename, sizeof(log_filename), "%s.v2.log", db_name);
    
    init_buffer_pool(&g_pool, db_filename);
    wal_init(&g_wal, log_filename);
    
    // In a real system, you would check if the DB is new and initialize the root B+ tree page.
    // We will assume page 0 is the root for our B+ Tree index.
    Page *root = fetch_page(&g_pool, 0);
    if (root) {
        // If it's empty (just created), format it as a leaf root
        BTreeHeader *hdr = btree_get_header(root);
        // Force initialization if the page seems uninitialized or corrupt.
        // NOTE: btree_init_node() never sets header->page_id, and page 0
        // (the root) legitimately keeps page_id == 0 forever, so that field
        // can't be used to detect "already initialized". is_root is only
        // ever true after btree_init_node() has actually run, so it's a
        // reliable freshness check (a zeroed/new page has is_root == false).
        if (!hdr->is_root || hdr->num_keys > BTREE_LEAF_MAX_KEYS) {
            btree_init_node(root, true, true, 0); // is_leaf=true, is_root=true, parent=0
        }
        unpin_page(&g_pool, 0, true);
    }
    
    strncpy(g_active_db, db_name, max_len - 1);
    g_active_db[max_len - 1] = '\0';
    g_initialized = true;
    return true;
}

void shutdown_v2_engine(void) {
    if (!g_initialized) return;
    flush_all_pages(&g_pool);
    wal_close(&g_wal);
    if (g_pool.disk_file) {
        fclose(g_pool.disk_file);
        g_pool.disk_file = NULL;
    }
    g_active_db[0] = '\0';
    g_initialized = false;
}

static bool heap_insert_record(HeapRecord *record, RecordID *out_rid) {
    for (uint32_t page_id = HEAP_FIRST_PAGE; page_id < 1024; page_id++) {
        Page *page = fetch_page(&g_pool, page_id);
        if (!page) return false;

        PageHeader *header = (PageHeader *)page->data;
        if (header->page_id != page_id) init_page(page, page_id);

        int slot_id = insert_tuple(page, (const char *)record, sizeof(*record));
        unpin_page(&g_pool, page_id, slot_id >= 0);
        if (slot_id >= 0) {
            out_rid->page_id = page_id;
            out_rid->slot_id = (uint16_t)slot_id;
            return true;
        }
    }
    return false;
}

static bool heap_read_record(RecordID rid, HeapRecord *out_record) {
    Page *page = fetch_page(&g_pool, rid.page_id);
    if (!page) return false;

    memset(out_record, 0, sizeof(*out_record));
    bool found = get_tuple(page, rid.slot_id, (char *)out_record);
    unpin_page(&g_pool, rid.page_id, false);
    return found && out_record->magic == HEAP_RECORD_MAGIC &&
           out_record->column_count <= V2_MAX_COLUMNS;
}

static int heap_find_column(const HeapRecord *record, const char *column) {
    for (uint16_t i = 0; i < record->column_count; i++) {
        if (strcmp(record->columns[i], column) == 0) return (int)i;
    }
    return -1;
}

static bool validate_insert_columns(const table *cmd, char *error, size_t error_size) {
    if (cmd->col_count == 0) return true;

    Page *root = fetch_page(&g_pool, 0);
    if (!root) {
        snprintf(error, error_size, "Error: could not fetch table schema");
        return false;
    }

    BTreeHeader *header = btree_get_header(root);
    if (header->num_keys == 0) {
        unpin_page(&g_pool, 0, false);
        return true;
    }

    LeafEntry *entries = (LeafEntry *)(root->data + sizeof(BTreeHeader));
    HeapRecord schema;
    bool schema_found = heap_read_record(entries[0].rid, &schema);
    unpin_page(&g_pool, 0, false);
    if (!schema_found) {
        snprintf(error, error_size, "Error: could not read table schema");
        return false;
    }

    for (int i = 0; i < cmd->col_count; i++) {
        if (heap_find_column(&schema, cmd->cols[i]) < 0) {
            snprintf(error, error_size, "Error: column '%s' does not exist", cmd->cols[i]);
            return false;
        }
    }
    return true;
}

static bool heap_find_matching_record(const table *cmd, RecordID *out_rid) {
    Page *root = fetch_page(&g_pool, 0);
    if (!root) return false;

    BTreeHeader *header = btree_get_header(root);
    LeafEntry *entries = (LeafEntry *)(root->data + sizeof(BTreeHeader));
    bool found = false;
    for (uint16_t i = 0; i < header->num_keys; i++) {
        HeapRecord record;
        int column_idx;
        if (!heap_read_record(entries[i].rid, &record)) continue;
        column_idx = heap_find_column(&record, cmd->where.column);
        if (column_idx >= 0 && strcmp(record.values[column_idx], cmd->where.value) == 0) {
            *out_rid = entries[i].rid;
            found = true;
            break;
        }
    }
    unpin_page(&g_pool, 0, false);
    return found;
}

static void print_record_header(const HeapRecord *record) {
    for (uint16_t i = 0; i < record->column_count; i++) {
        printf("%-15s", record->columns[i]);
    }
    printf("\n");
}

static void print_record(const HeapRecord *record) {
    for (uint16_t i = 0; i < record->column_count; i++) {
        printf("%-15s", record->values[i]);
    }
    printf("\n");
}

ExecMessage execute_insert(table *cmd) {
    ExecMessage result;
    result.success = 0;
    
    if (!init_v2_engine(cmd->name, true)) {
        snprintf(result.msg, max_len, "Error: could not open table '%s'", cmd->name);
        return result;
    }

    if (!validate_insert_columns(cmd, result.msg, sizeof(result.msg))) {
        return result;
    }

    uint32_t txn_id = g_next_txn_id++;
    
    // 1. Write WAL Begin
    wal_write_record(&g_wal, txn_id, LOG_BEGIN_TXN, 0, 0, NULL);
    
    // 2. Use the id column as the index key, falling back to the first value.
    BTreeKey key;
    memset(key, 0, 64);
    int key_idx = 0;
    for (int i = 0; i < cmd->col_count; i++) {
        if (strcmp(cmd->cols[i], "id") == 0) {
            key_idx = i;
            break;
        }
    }
    if (cmd->value_count > key_idx) {
        strncpy(key, cmd->values[key_idx], 63);
    } else {
        snprintf(result.msg, max_len, "Error: insert requires at least one value for the key");
        return result;
    }

    if (cmd->value_count > V2_MAX_COLUMNS) {
        snprintf(result.msg, max_len, "Error: V2 supports at most %d columns", V2_MAX_COLUMNS);
        return result;
    }

    HeapRecord record = {0};
    record.magic = HEAP_RECORD_MAGIC;
    record.column_count = (uint16_t)cmd->value_count;
    for (int i = 0; i < cmd->value_count; i++) {
        if (i < cmd->col_count) {
            strncpy(record.columns[i], cmd->cols[i], max_len - 1);
        } else {
            snprintf(record.columns[i], max_len, "value%d", i + 1);
        }
        strncpy(record.values[i], cmd->values[i], max_len - 1);
    }

    RecordID new_rid;
    if (!heap_insert_record(&record, &new_rid)) {
        snprintf(result.msg, max_len, "Error: heap storage is full");
        return result;
    }
    
    // 3. Fetch Root Page
    Page *root = fetch_page(&g_pool, 0);
    if (!root) {
        Page *heap_page = fetch_page(&g_pool, new_rid.page_id);
        if (heap_page) {
            delete_tuple(heap_page, new_rid.slot_id);
            unpin_page(&g_pool, new_rid.page_id, true);
        }
        snprintf(result.msg, max_len, "Error: Could not fetch root page");
        return result;
    }
    
    // 4. Insert into B+ Tree
    bool inserted = btree_leaf_insert(root, key, new_rid);
    if (!inserted) {
        Page *heap_page = fetch_page(&g_pool, new_rid.page_id);
        if (heap_page) {
            delete_tuple(heap_page, new_rid.slot_id);
            unpin_page(&g_pool, new_rid.page_id, true);
        }
    }
    
    // 5. Write WAL Insert & Commit only after both storage layers succeed.
    if (inserted) {
        wal_write_record(&g_wal, txn_id, LOG_INSERT, new_rid.page_id, new_rid.slot_id, key);
        wal_write_record(&g_wal, txn_id, LOG_COMMIT_TXN, 0, 0, NULL);
    } else {
        wal_write_record(&g_wal, txn_id, LOG_ABORT_TXN, 0, 0, NULL);
    }
    wal_flush(&g_wal); // Force log to disk
    
    unpin_page(&g_pool, 0, true);
    
    if (inserted) {
        result.success = 1;
        snprintf(result.msg, max_len, "Successfully inserted key '%s' into B+ Tree", key);
    } else {
        snprintf(result.msg, max_len, "Error: Failed to insert (duplicate key or full page)");
    }
    
    return result;
}

ExecMessage execute_select(table *cmd) {
    ExecMessage result;
    result.success = 0;
    
    if (!init_v2_engine(cmd->name, false)) {
        snprintf(result.msg, max_len, "Error: Table '%s' not found", cmd->name);
        return result;
    }
    
    Page *root = fetch_page(&g_pool, 0);
    if (!root) {
        snprintf(result.msg, max_len, "Error: Table '%s' not found", cmd->name);
        return result;
    }
    
    if (!cmd->where.has_condition) {
        BTreeHeader *hdr = btree_get_header(root);
        LeafEntry *entries = (LeafEntry*)(root->data + sizeof(BTreeHeader));
        
        printf("--- SELECT * FROM %s (B+ Tree) ---\n", cmd->name);
        HeapRecord rows[BTREE_LEAF_MAX_KEYS];
        int row_count = 0;
        int count = 0;
        for (int i = 0; i < hdr->num_keys; i++) {
            if (row_count < BTREE_LEAF_MAX_KEYS &&
                heap_read_record(entries[i].rid, &rows[row_count])) {
                row_count++;
                count++;
            }
        }
        if (row_count > 0) {
            print_record_header(&rows[0]);
            for (int i = 0; i < row_count; i++) print_record(&rows[i]);
        } else {
            printf("No rows.\n");
        }

        unpin_page(&g_pool, 0, false);
        result.success = 1;
        snprintf(result.msg, max_len, "Returned %d rows.", count);
        return result;
    }
    
    // 3. Point Lookup via B+ Tree
    BTreeKey search_key;
    memset(search_key, 0, 64);
    strncpy(search_key, cmd->where.value, 63);
    RecordID found_rid;
    
    unpin_page(&g_pool, 0, false);
    bool found = btree_search(&g_pool, 0, search_key, &found_rid);
    
    if (found) {
        HeapRecord record;
        if (!heap_read_record(found_rid, &record)) {
            snprintf(result.msg, max_len, "Indexed row is missing from heap storage.");
            return result;
        }
        printf("--- SELECT * FROM %s WHERE key = '%s' ---\n", cmd->name, search_key);
        print_record_header(&record);
        print_record(&record);

        result.success = 1;
        snprintf(result.msg, max_len, "Found 1 matching row via B+ Tree index.");
    } else {
        snprintf(result.msg, max_len, "No rows found matching condition.");
    }
    
    return result;
}

ExecMessage execute_update(table *cmd) {
    ExecMessage result;
    result.success = 0;
    if (!init_v2_engine(cmd->name, false)) {
        snprintf(result.msg, max_len, "Error: Table '%s' not found", cmd->name);
        return result;
    }
    if (!cmd->where.has_condition) {
        snprintf(result.msg, max_len, "Error: update requires a WHERE clause");
        return result;
    }

    RecordID rid;
    if (!heap_find_matching_record(cmd, &rid)) {
        snprintf(result.msg, max_len, "No rows found matching condition.");
        return result;
    }

    HeapRecord record;
    if (!heap_read_record(rid, &record)) {
        snprintf(result.msg, max_len, "Indexed row is missing from heap storage.");
        return result;
    }
    int update_idx = heap_find_column(&record, cmd->update_columns);
    if (update_idx < 0) {
        snprintf(result.msg, max_len, "Error: column '%s' does not exist", cmd->update_columns);
        return result;
    }

    BTreeKey old_key;
    BTreeKey new_key;
    memset(old_key, 0, sizeof(old_key));
    memset(new_key, 0, sizeof(new_key));
    strncpy(old_key, record.values[0], sizeof(old_key) - 1);
    strncpy(new_key, record.values[0], sizeof(new_key) - 1);
    strncpy(record.values[update_idx], cmd->update_values, max_len - 1);
    if (update_idx == 0) {
        strncpy(new_key, record.values[0], sizeof(new_key) - 1);
    }

    Page *root = fetch_page(&g_pool, 0);
    bool index_updated = true;
        if (!root && update_idx == 0 && strcmp(old_key, new_key) != 0) {
            index_updated = false;
        } else if (root && update_idx == 0 && strcmp(old_key, new_key) != 0) {
            bool old_deleted = btree_leaf_delete(root, old_key, NULL);
            index_updated = old_deleted && btree_leaf_insert(root, new_key, rid);
            if (!index_updated && old_deleted) {
                btree_leaf_insert(root, old_key, rid);
            }
    }
    if (root) unpin_page(&g_pool, 0, index_updated && update_idx == 0);
    if (!index_updated) {
        snprintf(result.msg, max_len, "Error: could not update indexed key");
        return result;
    }

    Page *page = fetch_page(&g_pool, rid.page_id);
    bool updated = page && update_tuple(page, rid.slot_id, (const char *)&record, sizeof(record));
    if (page) unpin_page(&g_pool, rid.page_id, updated);
    if (updated) {
        result.success = 1;
        snprintf(result.msg, max_len, "Updated 1 row.");
    } else {
        snprintf(result.msg, max_len, "Error: could not update row.");
    }
    return result;
}

ExecMessage execute_delete(table *cmd) {
    ExecMessage result;
    result.success = 0;
    if (!init_v2_engine(cmd->name, false)) {
        snprintf(result.msg, max_len, "Error: Table '%s' not found", cmd->name);
        return result;
    }
    if (!cmd->where.has_condition) {
        snprintf(result.msg, max_len, "Error: delete requires a WHERE clause");
        return result;
    }

    RecordID rid;
    if (!heap_find_matching_record(cmd, &rid)) {
        snprintf(result.msg, max_len, "No rows found matching condition.");
        return result;
    }

    HeapRecord record;
    if (!heap_read_record(rid, &record)) {
        snprintf(result.msg, max_len, "Indexed row is missing from heap storage.");
        return result;
    }

    BTreeKey key;
    memset(key, 0, sizeof(key));
    strncpy(key, record.values[0], sizeof(key) - 1);
    Page *root = fetch_page(&g_pool, 0);
    if (!root) {
        snprintf(result.msg, max_len, "Error: could not fetch table index");
        return result;
    }

    Page *heap_page = fetch_page(&g_pool, rid.page_id);
    bool deleted_heap = heap_page && delete_tuple(heap_page, rid.slot_id);
    if (heap_page) unpin_page(&g_pool, rid.page_id, deleted_heap);
    bool deleted_index = deleted_heap && btree_leaf_delete(root, key, NULL);
    unpin_page(&g_pool, 0, deleted_index);

    if (deleted_index) {
        result.success = 1;
        snprintf(result.msg, max_len, "Deleted 1 row.");
    } else {
        snprintf(result.msg, max_len, "Error: could not delete row.");
    }
    return result;
}

ExecuteFn dispatch[] = {
    [cmd_select] = execute_select,
    [cmd_insert] = execute_insert,
    [cmd_update] = execute_update,
    [cmd_delete] = execute_delete,
};