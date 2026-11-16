
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void test_lexer(
    const char *input,
    const TokenType expected_types[],
    const char *expected_values[],
    int expected_count)
{
    printf("\nTesting: \"%s\"\n", input);

    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
    {
        printf("FAIL: lexer_tokenize returned NULL\n");
        tests_failed++;
        return;
    }

    if (tokens->count != expected_count)
    {
        printf("FAIL: Expected %d tokens, got %d\n",
               expected_count, tokens->count);

        tests_failed++;
        token_list_free(tokens);
        return;
    }

    for (int i = 0; i < expected_count; i++)
    {
        if (tokens->tokens[i].type != expected_types[i] ||
            strcmp(tokens->tokens[i].value, expected_values[i]) != 0)
        {
            printf("FAIL: Token %d is incorrect\n", i);
            printf("Expected: type=%d, value=\"%s\"\n",
                   expected_types[i], expected_values[i]);

            printf("Got:      type=%d, value=\"%s\"\n",
                   tokens->tokens[i].type, tokens->tokens[i].value);

            tests_failed++;
            token_list_free(tokens);
            return;
        }
    }

    printf("PASS\n");
    tests_passed++;

    token_list_free(tokens);
}

static void test_invalid_input(const char *input)
{
    printf("\nTesting invalid input: \"%s\"\n", input);

    TokenList *tokens = lexer_tokenize(input);

    if (tokens == NULL)
    {
        printf("PASS: Lexer rejected invalid input\n");
        tests_passed++;
    }
    else
    {
        printf("FAIL: Lexer accepted invalid input\n");
        tests_failed++;
        token_list_free(tokens);
    }
}

int main(void)
{
    printf("========== LEXER UNIT TESTS ==========\n");

    // 1. Simple command
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "echo", "hello", ""
        };

        test_lexer("echo hello", types, values, 3);
    }

    // 2. Whitespace handling
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "echo", "hello", ""
        };

        test_lexer("   echo    hello   ", types, values, 3);
    }

    // 3. Quoted strings
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "echo", "hello world", ""
        };

        test_lexer("echo \"hello world\"", types, values, 3);
    }

    // 4. Pipeline
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_PIPE,
            TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "echo", "|", "cat", ""
        };

        test_lexer("echo | cat", types, values, 4);
    }

    // 5. AND, OR, sequence and background
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_AND,
            TOKEN_WORD, TOKEN_OR,
            TOKEN_WORD, TOKEN_SEMICOLON,
            TOKEN_WORD, TOKEN_AMPERSAND,
            TOKEN_EOF
        };

        const char *values[] = {
            "a", "&&", "b", "||",
            "c", ";", "d", "&", ""
        };

        test_lexer("a && b || c ; d &", types, values, 9);
    }

    // 6. Input and output redirection
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_INPUT,
            TOKEN_WORD, TOKEN_OUTPUT,
            TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "cat", "<", "input.txt", ">", "output.txt", ""
        };

        test_lexer("cat < input.txt > output.txt",
                   types, values, 6);
    }

    // 7. Append redirection
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_APPEND,
            TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "echo", ">>", "output.txt", ""
        };

        test_lexer("echo >> output.txt", types, values, 4);
    }

    // 8. Error redirection
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_ERROR,
            TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "command", "2>", "error.txt", ""
        };

        test_lexer("command 2> error.txt", types, values, 4);
    }

    // 9. Error append redirection
    {
        TokenType types[] = {
            TOKEN_WORD, TOKEN_ERROR_APPEND,
            TOKEN_WORD, TOKEN_EOF
        };

        const char *values[] = {
            "command", "2>>", "error.txt", ""
        };

        test_lexer("command 2>> error.txt", types, values, 4);
    }

    // 10. Empty input
    {
        TokenType types[] = {
            TOKEN_EOF
        };

        const char *values[] = {
            ""
        };

        test_lexer("", types, values, 1);
    }

    // 11. Invalid inputs
    test_invalid_input("echo \"hello");
    test_invalid_input("echo 'hello");

    // Summary
    printf("\n========== TEST SUMMARY ==========\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
