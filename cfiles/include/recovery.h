#ifndef RECOVERY_H
#define RECOVERY_H

#include "wal.h"
#include "buffer.h"

// Scans the WAL log file and re-applies committed transactions
// that didn't make it to the main database file before a crash.
void recover_database(BufferPool *pool, const char *log_filename);

#endif