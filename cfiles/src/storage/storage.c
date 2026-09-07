#include "storage.h"
#include <stdio.h>
#include <string.h>

#define loop(n) for(int i=0;i<n;i++)
int init_db(const char* filename, char cols[][max_len], int col_count){
    if(!filename){
        return -1;
    }
    
    FILE* f = fopen(filename, "r+b");
    if(!f){
        f = fopen(filename, "w+b");
        if(!f){
            return -1; 
        }
        
        Header empty_header = {0}; 
        empty_header.column_count = col_count;
        loop(empty_header.column_count){
            strcpy(empty_header.cols[i], cols[i]);
        }
        fwrite(&empty_header, sizeof(Header), 1, f);
    }
    
    fclose(f);
    return 1;
}

int read_record(FILE* db_file,int index,Record *out_record){
    if (!db_file || !out_record || index < 0) return 0;
    
    // maths 
    long offset = sizeof(Header) + index * sizeof(Record);
    if (fseek(db_file, offset, SEEK_SET) != 0) {
        return 0;
    }
    
    if (fread(out_record, sizeof(Record), 1, db_file) != 1) {
        return 0;
    }
    
    if (out_record->deleted == 1) {
        return 0;
    }
    
    return 1;
}


int append_record(FILE* db_file, Record* new_record) {
    if(!db_file || !new_record) return -1;
    
    Header header;
    fseek(db_file, 0, SEEK_SET);
    if(fread(&header, sizeof(Header), 1, db_file) != 1) return -1;
    
    long offset = sizeof(Header) + (header.row_count * sizeof(Record));
    fseek(db_file, offset, SEEK_SET);
    if(fwrite(new_record, sizeof(Record), 1, db_file) != 1) return -1;
    
    header.row_count++;
    fseek(db_file, 0, SEEK_SET);
    if(fwrite(&header, sizeof(Header), 1, db_file) != 1) return -1;
    
    fflush(db_file);
    return 1;
}
int update_record(FILE* db_file, int index, Record* updated_record) {
    if (!db_file || !updated_record || index < 0) return -1;
    
    long offset = sizeof(Header) + index * sizeof(Record);
    if (fseek(db_file, offset, SEEK_SET) != 0) {
        return -1;
    }
    
    if (fwrite(updated_record, sizeof(Record), 1, db_file) != 1) {
        return -1;
    }
    
    fflush(db_file);
    return 1;
}
