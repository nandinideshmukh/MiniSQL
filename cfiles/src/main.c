#include <stdio.h>
#include <string.h>
#include "parser.h"

int main()
{
    char input[256];
    char *tokens[32];

    printf("db > ");
    fgets(input, sizeof(input), stdin);

    // Remove newline
    input[strcspn(input, "\n")] = '\0';

    int cnt = get_tokens(input, tokens, 32);

    Parser p = {
        .pos = 0,
        .error_msgs = "",
        .is_error = 0,
        .cnt = cnt,
        .tokens = tokens
    };

    table cmd = {0};

    // Determine command
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
            cmd.type = cmd_update;
            parse_update(&p, &cmd);
            break;

        case cmd_delete:
            parse_delete(&p, &cmd);
            break;

        case cmd_exit:
            printf("Exiting...\n");
            return 0;

        default:
            printf("Unknown command\n");
            return 1;
    }

    if (p.is_error)
    {
        printf("Error: %s\n", p.error_msgs);
        return 1;
    }

    printf("\nParsed successfully!\n");
    printf("Command : %d\n", cmd.type);
    printf("Table   : %s\n", cmd.name);

    if (cmd.where.has_condition)
    {
        printf("WHERE   : %s = %s\n",
               cmd.where.column,
               cmd.where.value);
    }

    return 0;
}