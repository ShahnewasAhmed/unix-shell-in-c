
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "executor.h"
#include "ast.h"

static int passed = 0;
static int failed = 0;

/* Create a command AST manually */
static ASTNode *create_command(int argc, const char *argv[])
{
    ASTNode *node = calloc(1, sizeof(ASTNode));

    if (node == NULL)
        return NULL;

    node->type = NODE_COMMAND;
    node->data.command.argc = argc;
    node->data.command.argv = calloc(argc + 1, sizeof(char *));

    if (node->data.command.argv == NULL)
    {
        free(node);
        return NULL;
    }

    for (int i = 0; i < argc; i++)
    {
        node->data.command.argv[i] = strdup(argv[i]);

        if (node->data.command.argv[i] == NULL)
        {
            ast_free(node);
            return NULL;
        }
    }

    return node;
}

static void test_status(
    const char *name,
    const char *argv[],
    int argc,
    int expected_status)
{
    printf("\nTesting: %s\n", name);

    ASTNode *node = create_command(argc, argv);

    if (node == NULL)
    {
        printf("FAIL: Could not create AST\n");
        failed++;
        return;
    }

    int status = execute_node(node);

    if (status == expected_status)
    {
        printf("PASS: Exit status = %d\n", status);
        passed++;
    }
    else
    {
        printf("FAIL: Expected %d, got %d\n",
               expected_status, status);
        failed++;
    }

    ast_free(node);
}

static void test_null_node(void)
{
    printf("\nTesting NULL AST node\n");

    int status = execute_node(NULL);

    if (status == 1)
    {
        printf("PASS\n");
        passed++;
    }
    else
    {
        printf("FAIL: Expected status 1, got %d\n", status);
        failed++;
    }
}

static void test_cd(void)
{
    printf("\nTesting built-in cd\n");

    char original_directory[4096];
    char current_directory[4096];

    if (getcwd(original_directory, sizeof(original_directory)) == NULL)
    {
        perror("getcwd");
        failed++;
        return;
    }

    const char *argv[] = {"cd", "/tmp"};
    ASTNode *node = create_command(2, argv);

    if (node == NULL)
    {
        failed++;
        return;
    }

    int status = execute_node(node);

    int cwd_ok =
        getcwd(current_directory, sizeof(current_directory)) != NULL &&
        strcmp(current_directory, "/tmp") == 0;

    if (chdir(original_directory) == -1)
        perror("restore directory");

    if (status == 0 && cwd_ok)
    {
        printf("PASS: Working directory changed to /tmp\n");
        passed++;
    }
    else
    {
        printf("FAIL: cd did not work correctly\n");
        failed++;
    }

    ast_free(node);
}

int main(void)
{
    printf("========== EXECUTOR UNIT TESTS ==========\n");

    const char *true_command[] = {"true"};
    test_status("External command: true", true_command, 1, 0);

    const char *false_command[] = {"false"};
    test_status("External command: false", false_command, 1, 1);

    const char *invalid_command[] = {"csh_command_that_does_not_exist"};
    test_status("Invalid external command", invalid_command, 1, 127);

    const char *cd_invalid[] = {"cd", "/tmp", "/home"};
    test_status("cd with too many arguments", cd_invalid, 3, 1);

    test_cd();
    test_null_node();

    printf("\n========== TEST SUMMARY ==========\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n", passed + failed);

    return failed == 0 ? 0 : 1;
}
