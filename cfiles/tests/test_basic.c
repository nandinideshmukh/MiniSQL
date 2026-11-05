#include <stdio.h>
#include <string.h>
#include "executor.h"
#include "parser.h"

int main(void) {
    table cmd;

    // Insert into a completely fresh/empty table (the reported failing case)
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = cmd_insert;
    strcpy(cmd.name, "basictbl");
    strcpy(cmd.values[0], "bob");
    cmd.value_count = 1;
    ExecMessage r1 = execute_insert(&cmd);
    printf("Insert 'bob'  -> success=%d msg=%s\n", r1.success, r1.msg);

    // Insert a second, different record
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = cmd_insert;
    strcpy(cmd.name, "basictbl");
    strcpy(cmd.values[0], "carol");
    cmd.value_count = 1;
    ExecMessage r2 = execute_insert(&cmd);
    printf("Insert 'carol'-> success=%d msg=%s\n", r2.success, r2.msg);

    // Select all
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = cmd_select;
    strcpy(cmd.name, "basictbl");
    ExecMessage r3 = execute_select(&cmd);
    printf("Select        -> success=%d msg=%s\n", r3.success, r3.msg);

    return (r1.success && r2.success && r3.success) ? 0 : 1;
}
