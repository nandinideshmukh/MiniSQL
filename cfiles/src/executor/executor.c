#include <stdio.h>
#include <string.h>
#include "executor.h"

ExecMessage execute_insert(table *cmd)
{
    ExecMessage result;
    result.success = 1;

    snprintf(result.msg, max_len, "would insert %d value(s) into '%s'",
             cmd->value_count, cmd->name);

    for (int i = 0; i < cmd->value_count; i++)
    {
        printf("  value[%d] = %s\n", i, cmd->values[i]);
    }

    return result;
}

ExecMessage execute_select(table *cmd)
{
    ExecMessage result;
    result.success = 1;

    snprintf(result.msg, max_len, "would select from %s %s",
             cmd->name,
             cmd->where.has_condition ? " (with WHERE)" : "");

    return result;
}

ExecMessage execute_update(table *cmd)
{
    ExecMessage result;
    result.success = 1;

    snprintf(result.msg, max_len, "would update '%s' SET %s = %s%s",
             cmd->name, cmd->update_columns, cmd->update_values,
             cmd->where.has_condition ? " (with WHERE)" : "");

    return result;
}

ExecMessage execute_delete(table *cmd)
{
    ExecMessage result;
    result.success = 1;

    snprintf(result.msg, max_len, "would delete from %s %s",
             cmd->name,
             cmd->where.has_condition ? " (with WHERE)" : "");

    return result;
}

// dispatch is a array of pointers that points to functions to return ExecMessage
ExecuteFn dispatch[] = {
    [cmd_select] = execute_select,
    [cmd_insert] = execute_insert,
    [cmd_update] = execute_update,
    [cmd_delete] = execute_delete,
};