#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "ast.h"

void shell_loop(void)
{
    char *line = NULL;
    size_t capacity = 0;

    while (1)
    {
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
            free(line);
            continue;
        }

        ASTNode *root = parser_parse(tokens);

        if (root != NULL)
        {
            // Later: execute AST here

            ast_free(root);
        }

        token_list_free(tokens);
        free(line);
    }
    free(line);
}