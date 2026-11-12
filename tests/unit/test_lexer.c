
#include <stdio.h>
#include <string.h>
#include "../../include/lexer.h"

int main(void)
{
    const char *input = "echo hello";

    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
    {
        printf("FAIL: lexer_tokenize returned NULL\n");
        return 1;
    }

    if (tokens->count != 3)
    {
        printf("FAIL: Expected 3 tokens, got %d\n", tokens->count);
        token_list_free(tokens);
        return 1;
    }

    if (tokens->tokens[0].type != TOKEN_WORD ||
        strcmp(tokens->tokens[0].value, "echo") != 0)
    {
        printf("FAIL: First token is incorrect\n");
        token_list_free(tokens);
        return 1;
    }

    if (tokens->tokens[1].type != TOKEN_WORD ||
        strcmp(tokens->tokens[1].value, "hello") != 0)
    {
        printf("FAIL: Second token is incorrect\n");
        token_list_free(tokens);
        return 1;
    }

    if (tokens->tokens[2].type != TOKEN_EOF)
    {
        printf("FAIL: Expected EOF token\n");
        token_list_free(tokens);
        return 1;
    }

    printf("PASS: simple command\n");

    token_list_free(tokens);
    return 0;
}
