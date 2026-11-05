#ifndef WAL_H
#define WAL_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// Types of operations we need to log for recovery
typedef enum {
    LOG_BEGIN_TXN = 0,
    LOG_INSERT = 1,
    LOG_UPDATE = 2,
    LOG_DELETE = 3,
    LOG_COMMIT_TXN = 4,
    LOG_ABORT_TXN = 5
} LogRecordType;

// A standard Write-Ahead Log (WAL) record
typedef struct {
    uint32_t lsn;           // Log Sequence Number (monotonically increasing)
    uint32_t txn_id;        // Transaction ID doing the operation
    LogRecordType type;     // What kind of operation?
    
    // For data operations (INSERT, UPDATE, DELETE)
    uint32_t page_id;       // Which page was modified?
    uint16_t slot_id;       // Which slot inside the page?
    
    // In a real DB, we'd store variable-length before/after images here.
    // For simplicity in minidb, we'll reserve fixed space for the payload.
    char record_payload[128]; 
} LogRecord;

// WAL Manager state
typedef struct {
    FILE *log_file;
    uint32_t current_lsn;
    uint32_t flushed_lsn;
} WALManager;

// Core WAL Functions
bool wal_init(WALManager *wal, const char *log_filename);
uint32_t wal_write_record(WALManager *wal, uint32_t txn_id, LogRecordType type, uint32_t page_id, uint16_t slot_id, const char *payload);
void wal_flush(WALManager *wal);
void wal_close(WALManager *wal);

#endif