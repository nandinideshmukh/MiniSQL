#include "page.h"
#include <string.h>

// Initializes a 4KB block as an empty Slotted Page
void init_page(Page *page, uint32_t page_id) {
    memset(page->data, 0, PAGE_SIZE); // Zero out the 4KB
    
    // The header lives at the very front (byte 0)
    PageHeader *header = (PageHeader *)page->data;
    header->page_id = page_id;
    header->free_space = PAGE_SIZE; // Free space ends at the very back of the 4KB block
    header->slot_count = 0;
}

// Inserts a tuple and returns its Slot ID
int insert_tuple(Page *page, const char *record_data, uint16_t record_len) {
    PageHeader *header = (PageHeader *)page->data;
    
    // We need space for the new data AND the new slot metadata
    uint16_t space_needed = record_len + sizeof(Slot);
    
    // Where does the slot array currently end?
    uint16_t slot_array_end = sizeof(PageHeader) + (header->slot_count * sizeof(Slot));
    
    // Check if the gap between the slots (growing forward) and tuples (growing backward) is big enough
    if (header->free_space - slot_array_end < space_needed) {
        return -1; // Not enough room! Page is full.
    }
    
    // Tuples grow BACKWARDS, so subtract offset from free_space
    uint16_t tuple_start = header->free_space - record_len;
    
    // Copy the actual data to the back
    memcpy(page->data + tuple_start, record_data, record_len);
    
    // Create the slot (immediately after the existing slots)
    Slot *slots = (Slot *)(page->data + sizeof(PageHeader));
    slots[header->slot_count].start = tuple_start;
    slots[header->slot_count].offset = record_len;
    
    // Update the header
    header->free_space = tuple_start; // Move the free space boundary
    int slot_id = header->slot_count;
    header->slot_count++;
    
    return slot_id;
}

// Reads a tuple out into a buffer
bool get_tuple(Page *page, int slot_id, char *out_buffer) {
    PageHeader *header = (PageHeader *)page->data;
    
    // Invalid slot ID
    if (slot_id < 0 || slot_id >= header->slot_count) {
        return false;
    }
    
    Slot *slots = (Slot *)(page->data + sizeof(PageHeader));
    Slot target_slot = slots[slot_id];
    
    // If offset is 0, it means it's a deleted tombstone
    if (target_slot.offset == 0) {
        return false;
    }
    
    // Copy from the page into the output buffer
    memcpy(out_buffer, page->data + target_slot.start, target_slot.offset);
    return true;
}

bool update_tuple(Page *page, int slot_id, const char *record_data, uint16_t record_len) {
    PageHeader *header = (PageHeader *)page->data;

    if (slot_id < 0 || slot_id >= header->slot_count || !record_data) {
        return false;
    }

    Slot *slots = (Slot *)(page->data + sizeof(PageHeader));
    Slot *target_slot = &slots[slot_id];
    if (target_slot->offset == 0 || record_len > target_slot->offset) {
        return false;
    }

    memcpy(page->data + target_slot->start, record_data, record_len);
    return true;
}

// Deletes a tuple (tombstone method)
bool delete_tuple(Page *page, int slot_id) {
    PageHeader *header = (PageHeader *)page->data;
    
    if (slot_id < 0 || slot_id >= header->slot_count) {
        return false;
    }
    
    Slot *slots = (Slot *)(page->data + sizeof(PageHeader));
    
    // We don't shift data to defragment immediately. 
    // We just set offset to 0 to mark it as deleted.
    slots[slot_id].offset = 0;
    slots[slot_id].start = 0;
    
    return true;
}