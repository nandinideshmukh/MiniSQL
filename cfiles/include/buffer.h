#ifndef BUFFER_H
#define BUFFER_H

#include "page.h"
#include <stdio.h>
#include <stdbool.h>

#define POOL_SIZE 10 // The number of pages we keep in RAM at once

// A Frame wraps a Page with metadata for the caching system
typedef struct {
    Page page;            // The actual 4KB block
    uint32_t page_id;     // Which disk page is currently in this frame? (-1 if empty)
    bool is_dirty;        // Has this page been modified since reading from disk?
    int pin_count;        // How many threads are currently using this page?
    uint64_t last_used;   // Timestamp for the LRU (Least Recently Used) eviction policy
} Frame;

typedef struct {
    Frame frames[POOL_SIZE];  // The actual cache in RAM
    FILE *disk_file;          // The file we are reading/writing pages from
    uint64_t current_time;    // Simulated clock to track LRU
} BufferPool;

// Buffer Pool Functions
void init_buffer_pool(BufferPool *pool, const char *db_filename);
Page* fetch_page(BufferPool *pool, uint32_t page_id);
void unpin_page(BufferPool *pool, uint32_t page_id, bool is_dirty);
bool flush_page(BufferPool *pool, uint32_t page_id);
void flush_all_pages(BufferPool *pool);

#endif