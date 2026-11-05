#include "wal.h"
#include <string.h>

bool wal_init(WALManager *wal, const char *log_filename) {
    // Open in append mode for writing logs sequentially
    wal->log_file = fopen(log_filename, "a+b");
    if (!wal->log_file) {
        return false;
    }
    
    // In a real system, you would scan the file to find the max LSN.
    // For now, we'll just start at 1.
    wal->current_lsn = 1;
    wal->flushed_lsn = 0;
    
    return true;
}

uint32_t wal_write_record(WALManager *wal, uint32_t txn_id, LogRecordType type, uint32_t page_id, uint16_t slot_id, const char *payload) {
    if (!wal->log_file) return 0;

    LogRecord record;
    memset(&record, 0, sizeof(LogRecord));
    
    record.lsn = wal->current_lsn++;
    record.txn_id = txn_id;
    record.type = type;
    record.page_id = page_id;
    record.slot_id = slot_id;
    
    if (payload != NULL) {
        strncpy(record.record_payload, payload, sizeof(record.record_payload) - 1);
    }

    // Write to the OS buffer (but not necessarily to disk yet)
    fwrite(&record, sizeof(LogRecord), 1, wal->log_file);
    
    return record.lsn;
}

void wal_flush(WALManager *wal) {
    if (wal->log_file) {
        // Force the OS to write the buffer to the physical disk
        fflush(wal->log_file);
        wal->flushed_lsn = wal->current_lsn - 1;
    }
}

void wal_close(WALManager *wal) {
    if (wal->log_file) {
        wal_flush(wal);
        fclose(wal->log_file);
        wal->log_file = NULL;
    }
}