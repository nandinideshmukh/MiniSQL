#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

void to_lowercase(char *str)
{
    for (int i = 0; str[i]; i++)
    {   
        // 0 to str[i] only no negative 
        str[i] = tolower((unsigned char)str[i]);
    }
}

cmd_terminal parse_token(const char *token)
{

    // case insensitive!
    char buf[32];
    size_t i;
    for (i = 0; token[i] && i < sizeof(buf) - 1; i++)
    {
        buf[i] = token[i];
    }
    buf[i] = '\0';
    to_lowercase(buf);

    // tokenization
    if (strcmp(buf, "select") == 0)
        return cmd_select;
    if (strcmp(buf, "insert") == 0)
        return cmd_insert;
    if (strcmp(buf, "delete") == 0)
        return cmd_delete;
    if (strcmp(buf, "update") == 0)
        return cmd_update;
    if (strcmp(buf, "exit") == 0)
        return cmd_exit;
    return cmd_unknown;
}

int get_tokens(char* input ,char *tokens[], int max_tokens){
    // thread safe resumes if interrupted
    char *saveptr;
    char* space = strtok_r(input , " ",&saveptr);
    int cnt = 0;
    while(space && cnt < max_tokens){
        tokens[cnt++] = space;
        space = strtok_r(NULL , " ",&saveptr);
    }
    return cnt;
}


char *peek(Parser* p);
char *advance(Parser *p);
int expect(Parser *p,const char *keyword);
int at_end(Parser *p);

void parse_select(Parser *p, table *cmd);
void parse_insert(Parser *p, table *cmd);
void parse_update(Parser *p, table *cmd);
void parse_delete(Parser *p, table *cmd);
void parse_where(Parser *p, table *cmd);

