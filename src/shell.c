#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "ast.h"
#include "parse.h"
#include "signals.h"
#include "executor.h"

void shell_loop(void)
{
    char *line = NULL;
    size_t capacity = 0;

    while (1)
    {
        if (child_exited)
        {
            child_exited = 0;
            reap_background_processes();
        }
        printf("CSH> ");

        fflush(stdout);

        ssize_t length = getline(&line, &capacity, stdin);

        if (length == -1)
        {
            printf("\n");
            break;
        }
        if (length == 1 && line[0] == '\n')
        {
            continue;
        }
        TokenList *tokens = lexer_tokenize(line);

        if (tokens == NULL)
        {
            continue;
        }

        ASTNode *root = parser_parse(tokens);

        if (root != NULL)
        {
            execute_node(root);
            ast_free(root);
        }

        token_list_free(tokens);
    }
    free(line);
}