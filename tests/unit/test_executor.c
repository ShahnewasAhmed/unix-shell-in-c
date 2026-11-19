
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include "lexer.h"
#include "parse.h"
#include "executor.h"
#include "ast.h"

static int passed = 0;
static int failed = 0;

static void wait_for_background_jobs(void)
{
    for (int i = 0; i < 100; i++)
    {
        reap_background_processes();
        usleep(100000);
    }

    reap_background_processes();
}

static int file_contains(
    const char *filename,
    const char *expected)
{
    FILE *file = fopen(filename, "r");

    if (file == NULL)
        return 0;

    char buffer[512] = {0};
    size_t length = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);

    return length > 0 && strstr(buffer, expected) != NULL;
}

static void test_command_line(
    const char *name,
    const char *input,
    int expected_status)
{
    printf("\nTesting: %s\n", name);

    // 1. Lexical analysis
    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
    {
        printf("FAIL: Lexer failed\n");
        failed++;
        return;
    }

    // 2. Parsing
    ASTNode *root = parser_parse(tokens);
    token_list_free(tokens);

    if (root == NULL)
    {
        printf("FAIL: Parser failed\n");
        failed++;
        return;
    }

    // 3. Execution
    int status = execute_node(root);

    // 4. Cleanup
    ast_free(root);

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
}

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

static void test_output_redirection(void)
{
    printf("\nTesting output redirection\n");

    const char *input = "echo hello > /tmp/csh_test_output.txt";

    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
    {
        failed++;
        return;
    }

    ASTNode *root = parser_parse(tokens);
    token_list_free(tokens);

    if (root == NULL)
    {
        failed++;
        return;
    }

    int status = execute_node(root);
    ast_free(root);

    FILE *file = fopen("/tmp/csh_test_output.txt", "r");

    if (file == NULL)
    {
        printf("FAIL: Could not open redirected file\n");
        failed++;
        return;
    }

    char buffer[100] = {0};
    fgets(buffer, sizeof(buffer), file);
    fclose(file);

    remove("/tmp/csh_test_output.txt");

    if (status == 0 && strcmp(buffer, "hello\n") == 0)
    {
        printf("PASS: Output redirected correctly\n");
        passed++;
    }
    else
    {
        printf("FAIL: Redirection did not work correctly\n");
        printf("Received: %s\n", buffer);
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

static int execute_line(const char *input)
{
    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
        return -1;

    ASTNode *root = parser_parse(tokens);
    token_list_free(tokens);

    if (root == NULL)
        return -1;

    int status = execute_node(root);
    ast_free(root);

    return status;
}

static void test_append_redirection(void)
{
    printf("\nTesting append redirection\n");

    const char *input1 = "echo first > /tmp/csh_append_test.txt";
    const char *input2 = "echo second >> /tmp/csh_append_test.txt";

    // First command: create file
    TokenList *tokens1 = lexer_tokenize(input1);
    ASTNode *root1 = parser_parse(tokens1);
    token_list_free(tokens1);

    int status1 = execute_node(root1);
    ast_free(root1);

    // Second command: append to file
    TokenList *tokens2 = lexer_tokenize(input2);
    ASTNode *root2 = parser_parse(tokens2);
    token_list_free(tokens2);

    int status2 = execute_node(root2);
    ast_free(root2);

    FILE *file = fopen("/tmp/csh_append_test.txt", "r");

    if (file == NULL)
    {
        printf("FAIL: Could not open file\n");
        failed++;
        return;
    }

    char first[100] = {0};
    char second[100] = {0};

    fgets(first, sizeof(first), file);
    fgets(second, sizeof(second), file);

    fclose(file);
    remove("/tmp/csh_append_test.txt");

    if (status1 == 0 &&
        status2 == 0 &&
        strcmp(first, "first\n") == 0 &&
        strcmp(second, "second\n") == 0)
    {
        printf("PASS: Append redirection works\n");
        passed++;
    }
    else
    {
        printf("FAIL: Append redirection failed\n");
        failed++;
    }
}

static void test_input_redirection(void)
{
    printf("\nTesting input redirection\n");

    const char *filename = "/tmp/csh_input_test.txt";

    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        failed++;
        return;
    }

    fprintf(file, "hello from file\n");
    fclose(file);

    int status = execute_line("cat < /tmp/csh_input_test.txt");

    remove(filename);

    if (status == 0)
    {
        printf("PASS: Input redirection\n");
        passed++;
    }
    else
    {
        printf("FAIL: Input redirection, status=%d\n", status);
        failed++;
    }
}

static void test_stderr_redirection(void)
{
    printf("\nTesting stderr redirection\n");

    const char *filename = "/tmp/csh_stderr_test.txt";

    int status = execute_line(
        "ls /this_path_does_not_exist 2> /tmp/csh_stderr_test.txt");

    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        printf("FAIL: stderr file was not created\n");
        failed++;
        return;
    }

    char buffer[256] = {0};
    size_t length = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    remove(filename);

    if (status != 0 && length > 0)
    {
        printf("PASS: stderr redirected to file\n");
        passed++;
    }
    else
    {
        printf("FAIL: stderr redirection\n");
        failed++;
    }
}

static void test_stderr_append(void)
{
    printf("\nTesting stderr append redirection\n");

    const char *filename = "/tmp/csh_stderr_append.txt";

    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        failed++;
        return;
    }

    fprintf(file, "existing content\n");
    fclose(file);

    int status = execute_line(
        "ls /invalid_path_one 2>> /tmp/csh_stderr_append.txt");

    file = fopen(filename, "r");

    if (file == NULL)
    {
        failed++;
        return;
    }

    char buffer[512] = {0};
    size_t length = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    remove(filename);

    if (status != 0 &&
        strstr(buffer, "existing content") != NULL &&
        length > strlen("existing content\n"))
    {
        printf("PASS: stderr append preserved existing content\n");
        passed++;
    }
    else
    {
        printf("FAIL: stderr append\n");
        failed++;
    }
}

static void test_pipeline_redirection(void)
{
    printf("\nTesting pipeline with redirection\n");

    const char *filename = "/tmp/csh_pipeline_test.txt";

    int status = execute_line(
        "echo hello | cat > /tmp/csh_pipeline_test.txt");

    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        printf("FAIL: Pipeline output file not created\n");
        failed++;
        return;
    }

    char buffer[100] = {0};
    fgets(buffer, sizeof(buffer), file);
    fclose(file);
    remove(filename);

    if (status == 0 && strcmp(buffer, "hello\n") == 0)
    {
        printf("PASS: Pipeline and redirection work together\n");
        passed++;
    }
    else
    {
        printf("FAIL: Pipeline redirection\n");
        failed++;
    }
}

static void test_and_short_circuit(void)
{
    printf("\nTesting AND short-circuit\n");

    const char *filename = "/tmp/csh_and_should_not_exist.txt";

    remove(filename);

    int status = execute_line(
        "false && echo should_not_run > /tmp/csh_and_should_not_exist.txt");

    FILE *file = fopen(filename, "r");

    if (file == NULL && status == 1)
    {
        printf("PASS: AND skipped second command\n");
        passed++;
    }
    else
    {
        printf("FAIL: AND short-circuit\n");
        failed++;
        if (file != NULL)
            fclose(file);
    }

    remove(filename);
}

static void test_or_short_circuit(void)
{
    printf("\nTesting OR short-circuit\n");

    const char *filename = "/tmp/csh_or_should_not_exist.txt";

    remove(filename);

    int status = execute_line(
        "true || echo should_not_run > /tmp/csh_or_should_not_exist.txt");

    FILE *file = fopen(filename, "r");

    if (file == NULL && status == 0)
    {
        printf("PASS: OR skipped second command\n");
        passed++;
    }
    else
    {
        printf("FAIL: OR short-circuit\n");
        failed++;
        if (file != NULL)
            fclose(file);
    }

    remove(filename);
}

static void test_nested_operators(void)
{
    printf("\nTesting nested AND/OR operators\n");

    int status = execute_line("false || true && false");

    if (status == 1)
    {
        printf("PASS: Nested AND/OR execution\n");
        passed++;
    }
    else
    {
        printf("FAIL: Expected 1, got %d\n", status);
        failed++;
    }
}

static void test_three_command_pipeline(void)
{
    printf("\nTesting three-command pipeline\n");

    int status = execute_line("echo hello | cat | wc -c");

    if (status == 0)
    {
        printf("PASS: Three-command pipeline\n");
        passed++;
    }
    else
    {
        printf("FAIL: Three-command pipeline, status=%d\n", status);
        failed++;
    }
}

static void test_pipeline_data_transfer(void)
{
    printf("\nTesting pipeline data transfer\n");

    int status = execute_line("printf hello | grep -q hello");

    if (status == 0)
    {
        printf("PASS: Data transferred through pipeline\n");
        passed++;
    }
    else
    {
        printf("FAIL: Pipeline data transfer, status=%d\n", status);
        failed++;
    }
}

static void test_large_pipeline_output(void)
{
    printf("\nTesting large pipeline output\n");

    int status = execute_line("seq 1 100000 | wc -l");

    if (status == 0)
    {
        printf("PASS: Large pipeline completed\n");
        passed++;
    }
    else
    {
        printf("FAIL: Large pipeline, status=%d\n", status);
        failed++;
    }
}

static void test_empty_pipeline_data(void)
{
    printf("\nTesting empty pipeline data\n");

    int status = execute_line("true | cat | wc -c");

    if (status == 0)
    {
        printf("PASS: Empty pipeline completed\n");
        passed++;
    }
    else
    {
        printf("FAIL: Empty pipeline, status=%d\n", status);
        failed++;
    }
}

static void test_early_consumer_exit(void)
{
    printf("\nTesting early consumer exit\n");

    int status = execute_line("yes | head -n 1");

    if (status == 0)
    {
        printf("PASS: Early consumer exit\n");
        passed++;
    }
    else
    {
        printf("FAIL: Early consumer exit, status=%d\n", status);
        failed++;
    }
}

static void test_large_data_transfer(void)
{
    printf("\nTesting large data transfer\n");

    int status = execute_line(
        "head -c 10485760 /dev/zero | wc -c");

    if (status == 0)
    {
        printf("PASS: Large data transfer\n");
        passed++;
    }
    else
    {
        printf("FAIL: Large data transfer, status=%d\n", status);
        failed++;
    }
}

static void test_four_stage_pipeline(void)
{
    printf("\nTesting four-stage pipeline\n");

    int status = execute_line(
        "printf hello | cat | cat | grep -q hello");

    if (status == 0)
    {
        printf("PASS: Four-stage pipeline\n");
        passed++;
    }
    else
    {
        printf("FAIL: Four-stage pipeline, status=%d\n", status);
        failed++;
    }
}

static void test_pipeline_failure(void)
{
    printf("\nTesting pipeline failure status\n");

    int status = execute_line("true | false");

    if (status == 1)
    {
        printf("PASS: Pipeline returned final command status\n");
        passed++;
    }
    else
    {
        printf("FAIL: Expected 1, got %d\n", status);
        failed++;
    }
}

static void test_background_execution(void)
{
    printf("\nTesting basic background execution\n");

    int status = execute_line("sleep 1 &");

    if (status == 0)
    {
        printf("PASS: Background command launched\n");
        passed++;
    }
    else
    {
        printf("FAIL: Background execution, status=%d\n", status);
        failed++;
    }
}

static void test_multiple_background_jobs(void)
{
    printf("\nTesting multiple background jobs\n");

    int s1 = execute_line("sleep 2 &");
    int s2 = execute_line("sleep 2 &");
    int s3 = execute_line("sleep 2 &");

    if (s1 == 0 && s2 == 0 && s3 == 0)
    {
        printf("PASS: Three background jobs launched\n");
        passed++;
    }
    else
    {
        printf("FAIL: Multiple background jobs\n");
        failed++;
    }
}

static void test_background_job_list(void)
{
    printf("\nTesting background job list\n");

    int status = execute_line("sleep 3 &");

    if (status != 0)
    {
        printf("FAIL: Could not launch job\n");
        failed++;
        return;
    }

    printf("Current background jobs:\n");
    print_background_jobs();

    passed++;
}

static void test_background_reaping(void)
{
    printf("\nTesting background process reaping\n");

    int status = execute_line("true &");

    if (status != 0)
    {
        printf("FAIL: Could not launch background command\n");
        failed++;
        return;
    }

    wait_for_background_jobs();

    printf("PASS: Reaping function executed\n");
    passed++;
}

static void test_background_redirection(void)
{
    printf("\nTesting background redirection\n");

    const char *filename = "/tmp/csh_background_test.txt";
    remove(filename);

    int status = execute_line(
        "echo background_work > /tmp/csh_background_test.txt &");

    if (status != 0)
    {
        printf("FAIL: Background command launch\n");
        failed++;
        return;
    }

    wait_for_background_jobs();

    if (file_contains(filename, "background_work"))
    {
        printf("PASS: Background output redirection\n");
        passed++;
    }
    else
    {
        printf("FAIL: Background output missing\n");
        failed++;
    }

    remove(filename);
}

static void test_background_pipeline(void)
{
    printf("\nTesting background pipeline\n");

    const char *filename = "/tmp/csh_background_pipeline.txt";
    remove(filename);

    int status = execute_line(
        "printf hello | cat > /tmp/csh_background_pipeline.txt &");

    if (status != 0)
    {
        printf("FAIL: Background pipeline launch\n");
        failed++;
        return;
    }

    wait_for_background_jobs();

    if (file_contains(filename, "hello"))
    {
        printf("PASS: Background pipeline\n");
        passed++;
    }
    else
    {
        printf("FAIL: Background pipeline output\n");
        failed++;
    }

    remove(filename);
}

static void test_background_completion(void)
{
    printf("\nTesting background completion\n");

    int status = execute_line("sleep 1 &");

    if (status != 0)
    {
        printf("FAIL: Could not launch background process\n");
        failed++;
        return;
    }

    wait_for_background_jobs();

    printf("PASS: Background completion/reaping cycle executed\n");
    passed++;
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

    printf("\n========== INTEGRATION TESTS ==========\n");

    // Pipeline
    test_command_line(
        "Pipeline: true | false",
        "true | false",
        1);

    test_command_line(
        "Pipeline: false | true",
        "false | true",
        0);

    // AND
    test_command_line(
        "AND: true && true",
        "true && true",
        0);

    test_command_line(
        "AND: false && true",
        "false && true",
        1);

    // OR
    test_command_line(
        "OR: false || true",
        "false || true",
        0);

    test_command_line(
        "OR: true || false",
        "true || false",
        0);

    // Sequence
    test_command_line(
        "Sequence: true ; false",
        "true ; false",
        1);

    test_cd();
    test_output_redirection();
    test_append_redirection();
    test_null_node();
    test_input_redirection();
    test_stderr_redirection();
    test_stderr_append();
    test_pipeline_redirection();
    test_and_short_circuit();
    test_or_short_circuit();
    test_nested_operators();
    test_three_command_pipeline();
    test_pipeline_data_transfer();
    test_large_pipeline_output();
    test_empty_pipeline_data();
    test_early_consumer_exit();
    test_large_data_transfer();
    test_four_stage_pipeline();
    test_pipeline_failure();
    test_background_execution();
    test_multiple_background_jobs();
    test_background_job_list();
    test_background_reaping();
    test_background_redirection();
    test_background_pipeline();
    test_background_completion();
    wait_for_background_jobs();

    printf("\n========== TEST SUMMARY ==========\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Total:  %d\n", passed + failed);

    return failed == 0 ? 0 : 1;
}
