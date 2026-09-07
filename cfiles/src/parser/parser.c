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

int get_tokens(char *input, char *tokens[], int max_tokens)
{
    char *space = strtok(input, " ");
    int cnt = 0;
    while (space && cnt < max_tokens)
    {
        tokens[cnt++] = space;
        space = strtok(NULL, " ");
    }
    return cnt;
}

char *peek(Parser *p)
{
    if (p->is_error)
        return NULL;
    if (p->pos < p->cnt)
        return p->tokens[p->pos];
    return NULL;
}
char *advance(Parser *p)
{
    if (p->is_error)
        return NULL;
    if (p->pos < p->cnt)
    {
        char *ans = p->tokens[p->pos];
        p->pos += 1;
        return ans;
    }
    return NULL;
}

int expect(Parser *p, const char *keyword)
{
    if (p->is_error)
        return 0;
    char *token = peek(p);
    if (token && strcmp(token, keyword) == 0)
    {
        p->pos++;
        return 1;
    }
    p->is_error = 1;
    if (token)
    {
        snprintf(p->error_msgs, max_len, "expected '%s' but got '%s'\n", keyword, token);
    }
    else
    {
        snprintf(p->error_msgs, max_len, "expected '%s' but got end of input'\n", keyword);
    }
    return 0;
}

int at_end(Parser *p)
{
    return p->pos >= p->cnt;
}

void parse_where(Parser *p, table *cmd)
{
    if (!expect(p, "where"))
        return;
    // expect(p,"where");
    char *c = advance(p);
    if (!c)
        return;
    // cmd->update_columns[0] = p->tokens[p->pos - 1][0];
    if (!expect(p, "="))
        return;

    char *val = advance(p);
    if (!val)
        return;

    strncpy(cmd->where.column, c, max_len - 1);
    cmd->where.column[max_len - 1] = '\0';

    strncpy(cmd->where.value, val, max_len - 1);
    cmd->where.value[max_len - 1] = '\0';

    cmd->where.has_condition = 1;
}

void parse_select(Parser *p, table *cmd)
{
    cmd->type = cmd_select;
    expect(p, "select");
    expect(p, "*");
    expect(p, "from");
    char *tok = advance(p);
    if (tok)
    {
        strncpy(cmd->name, tok, max_len - 1);
        cmd->name[max_len - 1] = '\0';
    }
    if (!p->is_error && !at_end(p))
    {
        parse_where(p, cmd);
    }
}

void parse_insert(Parser *p, table *cmd)
{
    cmd->type = cmd_insert;
    if (!expect(p, "insert"))
        return;
    if (!expect(p, "into"))
        return;

    char *tab = advance(p);
    if (!tab)
        return;

    strncpy(cmd->name, tab, max_len - 1);
    cmd->name[max_len - 1] = '\0';

    // NEW: parse the column list before VALUES
    if (!expect(p, "("))
        return;

    cmd->col_count = 0;
    while (!at_end(p) && cmd->col_count < max_val)
    {
        char *col = peek(p);
        if (!col)
            return;

        if (strcmp(col, ")") == 0)
        {
            advance(p);
            break;
        }

        advance(p);
        strncpy(cmd->cols[cmd->col_count], col, max_len - 1);
        cmd->cols[cmd->col_count][max_len - 1] = '\0';
        cmd->col_count++;

        if (!at_end(p) && strcmp(peek(p), ",") == 0)
        {
            advance(p);
        }
    }

    if (!expect(p, "values"))
        return;

    if (!expect(p, "("))
        return;

    cmd->value_count = 0;
    while (!at_end(p) && cmd->value_count < max_val)
    {
        char *val = peek(p);
        if (!val)
            return;

        if (strcmp(val, ")") == 0)
        {
            advance(p);

            // validation: column count must match value count
            if (cmd->col_count != cmd->value_count)
            {
                p->is_error = 1;
                snprintf(p->error_msgs, max_len,
                         "column count (%d) does not match value count (%d)",
                         cmd->col_count, cmd->value_count);
            }
            return;
        }
        advance(p);
        strncpy(cmd->values[cmd->value_count], val, max_len - 1);
        cmd->values[cmd->value_count][max_len - 1] = '\0';
        cmd->value_count++;

        if (!at_end(p) && strcmp(peek(p), ",") == 0)
        {
            advance(p);
        }
    }
    p->is_error = 1;
    snprintf(p->error_msgs, max_len, "incorrect query, please check the syntax");
}

void parse_update(Parser *p, table *cmd)
{
    cmd->type = cmd_update;
    if (!expect(p, "update"))
        return;

    // update users set name = "san" where id  = 1;
    char *tab = advance(p);
    if (!tab)
        return;

    strncpy(cmd->name, tab, max_len - 1);
    cmd->name[max_len - 1] = '\0';
    if (!expect(p, "set"))
        return;

    char *col = advance(p);
    if (!col)
        return;
    strncpy(cmd->update_columns, col, max_len - 1);
    cmd->update_columns[max_len - 1] = '\0';

    if (!expect(p, "="))
        return;

    char *val = advance(p);
    if (!val)
        return;
    strncpy(cmd->update_values, val, max_len - 1);
    cmd->update_values[max_len - 1] = '\0';

    if (!p->is_error && !at_end(p))
    {
        parse_where(p, cmd);
    }
}

void parse_delete(Parser *p, table *cmd)
{
    cmd->type = cmd_delete;
    if (!expect(p, "delete"))
        return;
    if (!expect(p, "from"))
        return;

    char *tab = advance(p);
    if (!tab)
        return;

    strncpy(cmd->name, tab, max_len - 1);
    // termination every where
    cmd->name[max_len - 1] = '\0';

    if (!p->is_error && !at_end(p))
    {
        parse_where(p, cmd);
    }
}
