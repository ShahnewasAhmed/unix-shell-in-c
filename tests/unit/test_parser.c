
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parse.h"
#include "ast.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void test_parser(const char *input, NodeType expected)
{
    printf("\nTesting: %s\n", input);

    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
    {
        printf("FAIL: Lexer failed\n");
        tests_failed++;
        return;
    }

    ASTNode *root = parser_parse(tokens);

    if (root == NULL)
    {
        printf("FAIL: Parser returned NULL\n");
        tests_failed++;
        token_list_free(tokens);
        return;
    }

    if (root->type != expected)
    {
        printf("FAIL: Expected node type %d, got %d\n",
               expected, root->type);

        tests_failed++;
    }
    else
    {
        printf("PASS\n");
        tests_passed++;

        ast_print(root);
    }

    ast_free(root);
    token_list_free(tokens);
}

static void test_invalid_parser(const char *input)
{
    printf("\nTesting invalid input: %s\n", input);

    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
    {
        printf("PASS: Lexer rejected input\n");
        tests_passed++;
        return;
    }

    ASTNode *root = parser_parse(tokens);

    if (root == NULL)
    {
        printf("PASS: Parser rejected input\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Parser accepted invalid input\n");
        tests_failed++;
        ast_free(root);
    }

    token_list_free(tokens);
}

int main(void)
{
    printf("========== PARSER TESTS ==========\n");

    // Valid inputs
    test_parser("ls", NODE_COMMAND);
    test_parser("ls -la", NODE_COMMAND);
    test_parser("ls | grep txt", NODE_PIPELINE);
    test_parser("echo hello && pwd", NODE_AND);
    test_parser("false || echo yes", NODE_OR);
    test_parser("pwd ; ls", NODE_SEQUENCE);
    test_parser("sleep 10 &", NODE_BACKGROUND);
    test_parser("echo hello > output.txt", NODE_COMMAND);
    test_parser("cat < input.txt", NODE_COMMAND);

    // Invalid inputs
    test_invalid_parser("ls |");
    test_invalid_parser("ls &&");
    test_invalid_parser("echo >");
    test_invalid_parser("cat <");
    test_invalid_parser("ls |||");

    printf("\n========== TEST SUMMARY ==========\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
