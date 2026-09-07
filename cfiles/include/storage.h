#ifndef STORAGE_H
#define STORAGE_H
#include "parser.h"

// offset(i) = i * record_size + header_size

typedef struct {
    char cols[max_val][max_len];
    int column_count;
    int row_count;
} Header;

typedef struct {
    char values[max_val][max_len];
    int deleted;
} Record;

int init_db(const char* filename, char cols[][max_len], int col_count);
int read_record(FILE* db_file, int index, Record* out_record);
int append_record(FILE* db_file, Record* new_record);
int update_record(FILE* db_file, int index, Record* updated_record);


#endif