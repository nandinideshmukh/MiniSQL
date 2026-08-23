#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "cli.h"
#include "executor.h"

static void lowercase_tokens(char *tokens[], int count)
{
    for (int i = 0; i < count; i++)
    {
        for (int j = 0; tokens[i][j]; j++)
        {
            tokens[i][j] = (char)tolower((unsigned char)tokens[i][j]);
        }
    }
}

void run_cli(void)
{
    char input[256];
    char *tokens[32];

    printf("minidb ! type EXIT to quit\n");

    while (1)
    {
        printf("minidb> ");

        if (!fgets(input, sizeof(input), stdin))
        {
            printf("\n");
            break;
        }
        // removes new line character and marks it as end of line
        // string complementary span
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0)
        {
            continue;
        }

        int cnt = get_tokens(input, tokens, 32);
        if (cnt == 0)
        {
            continue;
        }

        lowercase_tokens(tokens, cnt);

        Parser p = {
            .pos = 0,
            .error_msgs = "",
            .is_error = 0,
            .cnt = cnt,
            .tokens = tokens};

        table cmd = {0};

        cmd_terminal type = parse_token(peek(&p));

        switch (type)
        {
        case cmd_select:
            parse_select(&p, &cmd);
            break;
        case cmd_insert:
            parse_insert(&p, &cmd);
            break;
        case cmd_update:
            parse_update(&p, &cmd);
            break;
        case cmd_delete:
            parse_delete(&p, &cmd);
            break;
        case cmd_exit:
            printf("Exiting...\n");
            return;
        default:
            printf("Error: unknown command\n");
            continue;
        }

        if (p.is_error)
        {
            printf("Error: %s\n", p.error_msgs);
            continue;
        }

        ExecMessage execution_message = dispatch[type](&cmd);
        printf("%s\n", execution_message.msg);

        printf("Parsed OK ! table: %s\n", cmd.name);
        if (cmd.where.has_condition)
        {
            printf("  WHERE %s = %s\n", cmd.where.column, cmd.where.value);
        }
    }
}