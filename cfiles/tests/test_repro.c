#include <stdio.h>
#include <string.h>
#include "executor.h"
#include "parser.h"

int main(void) {
    table cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type = cmd_insert;
    strcpy(cmd.name, "reprotbl");
    strcpy(cmd.values[0], "alice");
    cmd.value_count = 1;

    ExecMessage r = execute_insert(&cmd);
    printf("Insert 1 -> success=%d msg=%s\n", r.success, r.msg);

    return 0;
}
