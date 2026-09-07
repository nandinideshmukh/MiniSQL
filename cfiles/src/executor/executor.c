#include <stdio.h>
#include <string.h>
#include "executor.h"
#include "storage.h"

ExecMessage execute_insert(table *cmd)
{
    ExecMessage result;
    result.success = 0; 
    
    char filename[max_len + 4];
    snprintf(filename, sizeof(filename), "%s.db", cmd->name);
    
    if (init_db(filename, cmd->cols, cmd->col_count) < 0) {
        snprintf(result.msg, max_len, "Error: Could not initialize database %s", filename);
        return result;
    }
    
    FILE* f = fopen(filename, "r+b");
    if (!f) {
        snprintf(result.msg, max_len, "Error: Could not open database %s", filename);
        return result;
    }
    
    Record new_record = {0};
    new_record.deleted = 0;
    
    for (int i = 0; i < cmd->value_count && i < max_val; i++) {
        strncpy(new_record.values[i], cmd->values[i], max_len - 1);
        new_record.values[i][max_len - 1] = '\0';
    }
    
    if (append_record(f, &new_record) < 0) {
        snprintf(result.msg, max_len, "Error: Failed to append record to %s", filename);
        fclose(f);
        return result;
    }
    
    fclose(f);
    
    result.success = 1;
    snprintf(result.msg, max_len, "Successfully inserted 1 record into '%s'", cmd->name);
    return result;
}

ExecMessage execute_select(table *cmd)
{
    ExecMessage result;
    result.success = 0;
    
    char filename[max_len + 4];
    snprintf(filename, sizeof(filename), "%s.db", cmd->name);
    
    FILE* f = fopen(filename, "rb");
    if (!f) {
        snprintf(result.msg, max_len, "Error: Table '%s' does not exist", cmd->name);
        return result;
    }
    
    Header header;
    // returns count of items read, as it is 1 here so success should return 1
    if (fread(&header, sizeof(Header), 1, f) != 1) {
        snprintf(result.msg, max_len, "Error: Could not read header from '%s'", filename);
        fclose(f);
        return result;
    }
    
    printf("--- SELECT * FROM %s ---\n", cmd->name);

    for (int i = 0; i < header.column_count; i++) {
        printf("%-15s", header.cols[i]);
    }
    printf("\n");
    for (int i = 0; i < header.column_count; i++) {
        printf("---------------");
    }
    printf("\n");
    
    int printed_rows = 0;
    Record current_record;
    
    for (int i = 0; i < header.row_count; i++) {
        if (read_record(f, i, &current_record) == 1) { 

            int match = 1;
            if (cmd->where.has_condition) {
                int where_col_idx = -1;
                // later would be optimized with btree and indexing
                for (int c = 0; c < header.column_count; c++) {
                    if (strcmp(header.cols[c], cmd->where.column) == 0) {
                        where_col_idx = c;
                        break;
                    }
                }
                if (where_col_idx == -1 || strcmp(current_record.values[where_col_idx], cmd->where.value) != 0) {
                    match = 0;
                }
            }

            if (match) {
                for (int j = 0; j < header.column_count; j++) {
                    printf("%-15s", current_record.values[j]);
                }
                printf("\n");
                printed_rows++;
            }
        }
    }
    printf("------------------------------\n");
    
    fclose(f);
    
    result.success = 1;
    snprintf(result.msg, max_len, "Select completed. Returned %d rows.", printed_rows);
    return result;
}

ExecMessage execute_update(table *cmd)
{
    ExecMessage result;
    result.success = 0;
    
    char filename[max_len + 4];
    snprintf(filename, sizeof(filename), "%s.db", cmd->name);
    
    FILE* f = fopen(filename, "r+b");
    if (!f) {
        snprintf(result.msg, max_len, "Error: Table '%s' does not exist", cmd->name);
        return result;
    }
    
    Header header;
    if (fread(&header, sizeof(Header), 1, f) != 1) {
        snprintf(result.msg, max_len, "Error: Could not read header from '%s'", filename);
        fclose(f);
        return result;
    }

    int update_col_idx = -1;
    for (int i = 0; i < header.column_count; i++) {
        // returns 0 if equal
        if (strcmp(header.cols[i], cmd->update_columns) == 0) {
            update_col_idx = i;
            break;
        }
    }

    if (update_col_idx == -1) {
        snprintf(result.msg, max_len, "Error: Column '%s' not found", cmd->update_columns);
        fclose(f);
        return result;
    }

    int where_col_idx = -1;
    if (cmd->where.has_condition) {
        for (int i = 0; i < header.column_count; i++) {
            if (strcmp(header.cols[i], cmd->where.column) == 0) {
                where_col_idx = i;
                break;
            }
        }
        if (where_col_idx == -1) {
            snprintf(result.msg, max_len, "Error: WHERE column '%s' not found", cmd->where.column);
            fclose(f);
            return result;
        }
    }

    int updated_count = 0;
    Record current_record;

    for (int i = 0; i < header.row_count; i++) {
        if (read_record(f, i, &current_record) == 1) {
            int match = 1;
            if (cmd->where.has_condition) {
                if (strcmp(current_record.values[where_col_idx], cmd->where.value) != 0) {
                    match = 0;
                }
            }

            if (match) {
                strncpy(current_record.values[update_col_idx], cmd->update_values, max_len - 1);
                current_record.values[update_col_idx][max_len - 1] = '\0';
                
                if (update_record(f, i, &current_record) == 1) {
                    updated_count++;
                }
            }
        }
    }

    fclose(f);
    result.success = 1;
    snprintf(result.msg, max_len, "Updated %d record(s).", updated_count);
    return result;
}

ExecMessage execute_delete(table *cmd)
{
    ExecMessage result;
    result.success = 0;
    
    char filename[max_len + 4];
    snprintf(filename, sizeof(filename), "%s.db", cmd->name);
    
    FILE* f = fopen(filename, "r+b");
    if (!f) {
        snprintf(result.msg, max_len, "Error: Table '%s' does not exist", cmd->name);
        return result;
    }
    
    Header header;
    if (fread(&header, sizeof(Header), 1, f) != 1) {
        snprintf(result.msg, max_len, "Error: Could not read header from '%s'", filename);
        fclose(f);
        return result;
    }

    int where_col_idx = -1;
    if (cmd->where.has_condition) {
        for (int i = 0; i < header.column_count; i++) {
            if (strcmp(header.cols[i], cmd->where.column) == 0) {
                where_col_idx = i;
                break;
            }
        }
        if (where_col_idx == -1) {
            snprintf(result.msg, max_len, "Error: WHERE column '%s' not found", cmd->where.column);
            fclose(f);
            return result;
        }
    }

    int deleted_count = 0;
    Record current_record;

    for (int i = 0; i < header.row_count; i++) {
        if (read_record(f, i, &current_record) == 1) {
            int match = 1;
            if (cmd->where.has_condition) {
                if (strcmp(current_record.values[where_col_idx], cmd->where.value) != 0) {
                    match = 0;
                }
            }

            if (match) {
                current_record.deleted = 1; 
                if (update_record(f, i, &current_record) == 1) {
                    deleted_count++;
                }
            }
        }
    }

    fclose(f);
    result.success = 1;
    snprintf(result.msg, max_len, "Deleted %d record(s).", deleted_count);
    return result;
}

ExecuteFn dispatch[] = {
    [cmd_select] = execute_select,
    [cmd_insert] = execute_insert,
    [cmd_update] = execute_update,
    [cmd_delete] = execute_delete,
};