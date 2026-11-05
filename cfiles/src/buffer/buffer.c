#include "buffer.h"
#include <string.h>

void init_buffer_pool(BufferPool *pool, const char *db_filename) {
    // Open the database file in read/write binary mode
    pool->disk_file = fopen(db_filename, "r+b");
    if (!pool->disk_file) {
        pool->disk_file = fopen(db_filename, "w+b"); // Create if it doesn't exist
    }
    
    pool->current_time = 0;
    
    // Initialize all frames as empty
    for (int i = 0; i < POOL_SIZE; i++) {
        pool->frames[i].page_id = -1; // -1 means no page loaded
        pool->frames[i].is_dirty = false;
        pool->frames[i].pin_count = 0;
        pool->frames[i].last_used = 0;
    }
}

// Find a victim frame to evict using LRU (Least Recently Used)
static int find_victim_frame(BufferPool *pool) {
    int victim_idx = -1;
    uint64_t oldest_time = (uint64_t)-1;
    
    for (int i = 0; i < POOL_SIZE; i++) {
        // We can only evict frames that are NOT currently pinned (in use)
        if (pool->frames[i].pin_count == 0) {
            // If the frame is completely empty, pick it immediately
            if (pool->frames[i].page_id == -1) {
                return i;
            }
            
            // Otherwise, track the oldest unpinned frame
            if (pool->frames[i].last_used < oldest_time) {
                oldest_time = pool->frames[i].last_used;
                victim_idx = i;
            }
        }
    }
    
    return victim_idx;
}

Page* fetch_page(BufferPool *pool, uint32_t page_id) {
    pool->current_time++;
    
    // 1. Check if the page is already in the buffer pool (Cache Hit)
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool->frames[i].page_id == page_id) {
            pool->frames[i].pin_count++;
            pool->frames[i].last_used = pool->current_time;
            return &pool->frames[i].page;
        }
    }
    
    // 2. Cache Miss: Find a frame to evict
    int frame_idx = find_victim_frame(pool);
    if (frame_idx == -1) {
        // All frames are pinned! We are out of memory for this transaction.
        return NULL; 
    }
    
    // 3. If the victim was modified, flush it to disk first
    if (pool->frames[frame_idx].page_id != -1 && pool->frames[frame_idx].is_dirty) {
        flush_page(pool, pool->frames[frame_idx].page_id);
    }
    
    // 4. Load the requested page from disk
    long offset = (long)page_id * PAGE_SIZE;
    fseek(pool->disk_file, offset, SEEK_SET);
    
    // Read the 4KB block. If it doesn't exist (EOF), it's a brand new page.
    size_t bytes_read = fread(&pool->frames[frame_idx].page, 1, PAGE_SIZE, pool->disk_file);
    if (bytes_read < PAGE_SIZE) {
        // It's a new page. IMPORTANT: only zero it here - do NOT stamp a
        // PageHeader (slotted heap-page) layout on it via init_page().
        // BufferPool is generic and has no idea whether this page will be
        // used as a heap page (PageHeader) or a B+Tree node (BTreeHeader).
        // Those two structs are NOT layout-compatible, so writing one and
        // reading the other (as btree_get_header() does) produces garbage
        // field values. Let the caller initialize the header in whatever
        // format is appropriate for how the page is actually used.
        memset(&pool->frames[frame_idx].page, 0, PAGE_SIZE);
    }
    
    // 5. Update frame metadata
    pool->frames[frame_idx].page_id = page_id;
    pool->frames[frame_idx].is_dirty = false;
    pool->frames[frame_idx].pin_count = 1; // Pinned by the caller
    pool->frames[frame_idx].last_used = pool->current_time;
    
    return &pool->frames[frame_idx].page;
}

void unpin_page(BufferPool *pool, uint32_t page_id, bool is_dirty) {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool->frames[i].page_id == page_id) {
            if (is_dirty) {
                pool->frames[i].is_dirty = true;
            }
            if (pool->frames[i].pin_count > 0) {
                pool->frames[i].pin_count--;
            }
            return;
        }
    }
}

bool flush_page(BufferPool *pool, uint32_t page_id) {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool->frames[i].page_id == page_id) {
            long offset = (long)page_id * PAGE_SIZE;
            fseek(pool->disk_file, offset, SEEK_SET);
            fwrite(&pool->frames[i].page, 1, PAGE_SIZE, pool->disk_file);
            fflush(pool->disk_file);
            
            pool->frames[i].is_dirty = false;
            return true;
        }
    }
    return false;
}

void flush_all_pages(BufferPool *pool) {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool->frames[i].page_id != -1 && pool->frames[i].is_dirty) {
            flush_page(pool, pool->frames[i].page_id);
        }
    }
}