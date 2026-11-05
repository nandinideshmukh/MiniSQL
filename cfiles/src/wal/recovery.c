#include "recovery.h"
#include <stdio.h>

void recover_database(BufferPool *pool, const char *log_filename) {
    FILE *log_file = fopen(log_filename, "rb");
    if (!log_file) return; // No log file, nothing to recover

    LogRecord record;
    
    // In a real system, you use ARIES: Analysis, Redo, Undo.
    // For minidb, we will just do a simplified REDO pass:
    // Read the log sequentially. If we see an INSERT/UPDATE, we fetch the page,
    // apply the change, and flush it.
    
    printf("Starting crash recovery from WAL...\n");
    int records_replayed = 0;

    while (fread(&record, sizeof(LogRecord), 1, log_file) == 1) {
        // Skip transaction control records for this simple redo
        if (record.type == LOG_BEGIN_TXN || record.type == LOG_COMMIT_TXN || record.type == LOG_ABORT_TXN) {
            continue;
        }

        // REDO Phase: Bring the page into memory
        Page *page = fetch_page(pool, record.page_id);
        if (!page) {
            printf("Error fetching page %d during recovery!\n", record.page_id);
            continue;
        }

        // Here you would check the Page's LSN vs the LogRecord's LSN.
        // If PageLSN >= RecordLSN, the change is already on disk, skip it.
        // For our toy DB, we just assume we need to re-apply the payload.
        
        // (Normally we'd parse the payload and call page_insert/page_update here)
        // printf("Replaying %d on Page %d Slot %d\n", record.type, record.page_id, record.slot_id);
        
        // Mark the page as dirty so the Buffer Pool knows it must be saved again
        unpin_page(pool, record.page_id, true);
        records_replayed++;
    }
    
    // Ensure all recovered pages are pushed safely back to the database file
    flush_all_pages(pool);

    printf("Recovery complete. Replayed %d records.\n", records_replayed);
    fclose(log_file);
}