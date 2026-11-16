
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parse.h"
#include "ast.h"

static int tests_passed = 0;
static int tests_failed = 0;

static ASTNode *parse_input(const char *input)
{
    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
        return NULL;

    ASTNode *root = parser_parse(tokens);

    token_list_free(tokens);
    return root;
}

static int command_matches(ASTNode *node, int argc,
                           const char *expected_argv[])
{
    if (node == NULL || node->type != NODE_COMMAND)
        return 0;

    if (node->data.command.argc != argc)
        return 0;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(node->data.command.argv[i], expected_argv[i]) != 0)
            return 0;
    }

    return 1;
}

static void test_root_type(const char *input, NodeType expected)
{
    printf("\nTesting: %s\n", input);

    ASTNode *root = parse_input(input);

    if (root == NULL)
    {
        printf("FAIL: Parser returned NULL\n");
        tests_failed++;
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
    }

    ast_free(root);
}

static void test_command_arguments(void)
{
    printf("\nTesting command arguments\n");

    const char *expected[] = {
        "echo", "hello", "world"};

    ASTNode *root = parse_input("echo hello world");

    if (command_matches(root, 3, expected))
    {
        printf("PASS: Command arguments are correct\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Incorrect command arguments\n");
        tests_failed++;
    }

    ast_free(root);
}

static void test_pipeline_structure(void)
{
    printf("\nTesting pipeline structure\n");

    ASTNode *root = parse_input("echo hello | cat | wc -l");

    int valid = root != NULL &&
                root->type == NODE_PIPELINE &&
                root->data.pipeline.count == 3;

    if (valid)
    {
        const char *first[] = {"echo", "hello"};
        const char *second[] = {"cat"};
        const char *third[] = {"wc", "-l"};

        valid = command_matches(root->data.pipeline.commands[0], 2, first) &&
                command_matches(root->data.pipeline.commands[1], 1, second) &&
                command_matches(root->data.pipeline.commands[2], 2, third);
    }

    if (valid)
    {
        printf("PASS: Pipeline contains 3 correct commands\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Incorrect pipeline structure\n");
        tests_failed++;
    }

    ast_free(root);
}

static void test_binary_structure(
    const char *input,
    NodeType expected,
    const char *left_command,
    const char *right_command)
{
    printf("\nTesting binary structure: %s\n", input);

    ASTNode *root = parse_input(input);

    int valid = root != NULL &&
                root->type == expected &&
                root->data.binary.left != NULL &&
                root->data.binary.right != NULL;

    if (valid)
    {
        const char *left[] = {left_command};
        const char *right[] = {right_command};

        valid = command_matches(root->data.binary.left, 1, left) &&
                command_matches(root->data.binary.right, 1, right);
    }

    if (valid)
    {
        printf("PASS: Binary AST structure is correct\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Incorrect binary AST structure\n");
        tests_failed++;
    }

    ast_free(root);
}

static void test_redirection(void)
{
    printf("\nTesting redirection structure\n");

    ASTNode *root = parse_input("echo hello > output.txt");

    int valid = root != NULL &&
                root->type == NODE_COMMAND &&
                root->data.command.argc == 2 &&
                root->data.command.redirection_count == 1;

    if (valid)
    {
        Redirection *redirect = &root->data.command.redirections[0];

        valid = redirect->type == TOKEN_OUTPUT &&
                strcmp(redirect->filename, "output.txt") == 0;
    }

    if (valid)
    {
        printf("PASS: Redirection information is correct\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Incorrect redirection structure\n");
        tests_failed++;
    }

    ast_free(root);
}

static void test_background_structure(void)
{
    printf("\nTesting background structure\n");

    ASTNode *root = parse_input("sleep 5 &");

    int valid = root != NULL &&
                root->type == NODE_BACKGROUND &&
                root->data.background.command != NULL;

    if (valid)
    {
        const char *expected[] = {"sleep", "5"};

        valid = command_matches(
            root->data.background.command, 2, expected);
    }

    if (valid)
    {
        printf("PASS: Background structure is correct\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Incorrect background structure\n");
        tests_failed++;
    }

    ast_free(root);
}

static void test_invalid_input(const char *input)
{
    printf("\nTesting invalid input: %s\n", input);

    ASTNode *root = parse_input(input);

    if (root == NULL)
    {
        printf("PASS: Parser rejected invalid input\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Parser accepted invalid input\n");
        tests_failed++;
        ast_free(root);
    }
}

static void test_null_tokens(void)
{
    printf("\nTesting NULL token list\n");

    if (parser_parse(NULL) == NULL)
    {
        printf("PASS: NULL input rejected\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: NULL input accepted\n");
        tests_failed++;
    }
}

int main(void)
{
    printf("========== PARSER UNIT TESTS ==========\n");

    // Basic command
    test_root_type("ls", NODE_COMMAND);
    test_root_type("ls -la", NODE_COMMAND);
    test_command_arguments();

    // Pipeline
    test_root_type("ls | grep txt", NODE_PIPELINE);
    test_pipeline_structure();

    // AND / OR
    test_root_type("echo hello && pwd", NODE_AND);
    test_root_type("false || echo yes", NODE_OR);

    test_binary_structure(
        "false && true", NODE_AND, "false", "true");

    test_binary_structure(
        "false || true", NODE_OR, "false", "true");

    // Sequence
    test_root_type("pwd ; ls", NODE_SEQUENCE);

    test_binary_structure(
        "pwd ; ls", NODE_SEQUENCE, "pwd", "ls");

    // Background
    test_root_type("sleep 5 &", NODE_BACKGROUND);
    test_background_structure();

    // Redirection
    test_redirection();

    // Invalid inputs
    test_invalid_input("ls |");
    test_invalid_input("ls &&");
    test_invalid_input("echo >");
    test_invalid_input("cat <");
    test_invalid_input("ls |||");
    test_invalid_input("");

    // NULL input
    test_null_tokens();

    // Summary
    printf("\n========== TEST SUMMARY ==========\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
