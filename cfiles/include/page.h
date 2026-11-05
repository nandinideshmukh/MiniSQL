#include <stdint.h>
#include <stdbool.h>

#ifndef PAGE_H
#define PAGE_H

// 4096 bytes = 4096 * 8 bits = 4KB
#define PAGE_SIZE 4096

typedef struct
{
    uint16_t start;
    uint16_t offset;
} Slot;

typedef struct
{
    uint32_t page_id;
    uint16_t free_space;
    uint16_t slot_count;
    // later for transcations -> uint32_t lsn (Log Sequence Number)
} PageHeader;

typedef struct {
    char data[PAGE_SIZE];   
} Page;

void init_page(Page *page, uint32_t page_id);

int insert_tuple(Page *page, const char *record_data, uint16_t record_len);

bool get_tuple(Page *page, int slot_id, char *out_buffer);

bool update_tuple(Page *page, int slot_id, const char *record_data, uint16_t record_len);

bool delete_tuple(Page *page, int slot_id);

#endif

