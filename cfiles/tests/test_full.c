#include <stdio.h>
#include <string.h>
#include "executor.h"
#include "parser.h"
#include "btree.h"

int main(void) {
    printf("BTREE_LEAF_MAX_KEYS = %d\n", (int)BTREE_LEAF_MAX_KEYS);

    table cmd;
    int inserted = 0, failed = 0;
    for (int i = 0; i < (int)BTREE_LEAF_MAX_KEYS + 5; i++) {
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = cmd_insert;
        strcpy(cmd.name, "fulltbl");
        snprintf(cmd.values[0], 64, "key%04d", i);
        cmd.value_count = 1;
        ExecMessage r = execute_insert(&cmd);
        if (r.success) inserted++; else failed++;
    }
    printf("inserted=%d failed=%d (expected inserted=%d, failed=5)\n", inserted, failed, (int)BTREE_LEAF_MAX_KEYS);

    // Now verify select shows all rows
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = cmd_select;
    strcpy(cmd.name, "fulltbl");
    ExecMessage r = execute_select(&cmd);
    printf("select msg: %s\n", r.msg);

    return 0;
}
