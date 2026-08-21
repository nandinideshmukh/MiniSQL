#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

#define max_len 64
#define max_val 16

// read commands ,prompt, exit and error messages -> phase 1
typedef enum
{
    cmd_select,
    cmd_insert,
    cmd_delete,
    cmd_update,
    cmd_exit,
    cmd_unknown
} cmd_terminal;

typedef struct
{
    char column[max_len];
    char value[max_len];
    int has_condition;
} whereClause;

typedef struct
{
    cmd_terminal type;
    char name[max_len];
    // doing select * only for now

    // insert
    char values[max_val][max_len];
    int value_count;

    // update
    char update_columns[max_len];
    char update_values[max_len];

    // where
    whereClause where;

    int iserror;
    char errorMsg[max_len];

} table;

// cursor state
typedef struct
{
    int pos;
    char error_msgs[max_len];
    int is_error;
    int cnt;
    char **tokens;
} Parser;

// parsing tokens
cmd_terminal parse_token(const char *token);
int get_tokens(const char *input, char *tokens[], int max_tokens);

// cursor level implementation -> recursive

char *peek(Parser* p);
char *advance(Parser *p);
int expect(Parser *p,const char *keyword);
int at_end(Parser *p);

void parse_select(Parser *p, table *cmd);
void parse_insert(Parser *p, table *cmd);
void parse_update(Parser *p, table *cmd);
void parse_delete(Parser *p, table *cmd);
void parse_where(Parser *p, table *cmd);

#endif