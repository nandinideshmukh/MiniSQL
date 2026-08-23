#ifndef EXEC_H
#define EXEC_H
#include "parser.h"

typedef struct {
    int success;
    char msg[max_len];
} ExecMessage;



ExecMessage execute_select(table* cmd);
ExecMessage execute_update(table* cmd);
ExecMessage execute_insert(table* cmd );
ExecMessage execute_delete(table* cmd);

// function pointer syntax 
// return_type (*fname)(args)
// no need to allocate memory again
typedef ExecMessage (*ExecuteFn)(table *cmd);

// visibility across multiple files without allocating new memory!
extern ExecuteFn dispatch[];
// like this for eg->
// ExecMessage result = dispatch[cmd_select](cmd);
#endif
