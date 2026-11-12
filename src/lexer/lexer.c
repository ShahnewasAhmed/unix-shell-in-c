#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../include/lexer.h"

void token_list_free(TokenList *list)
{
    if (list == NULL)
        return;

    for (int i = 0; i < list->count; i++)
    {
        free(list->tokens[i].value);
    }

    free(list->tokens);
    free(list);
}

TokenType recognize_operator(const char *input, int *length)
{
    switch (input[0])
    {
    case '|':
        if (input[1] == '|')
        {
            *length = 2;
            return TOKEN_OR;
        }
        *length = 1;
        return TOKEN_PIPE;
    case '&':
        if (input[1] == '&')
        {
            *length = 2;
            return TOKEN_AND;
        }

        *length = 1;
        return TOKEN_AMPERSAND;

    case ';':
        *length = 1;
        return TOKEN_SEMICOLON;

    case '<':
        *length = 1;
        return TOKEN_INPUT;

    case '>':
        if (input[1] == '>')
        {
            *length = 2;
            return TOKEN_APPEND;
        }

        *length = 1;
        return TOKEN_OUTPUT;
    case '2':
        if (input[1] == '>')
        {
            if (input[2] == '>')
            {
                *length = 3;
                return TOKEN_ERROR_APPEND;
            }

            *length = 2;
            return TOKEN_ERROR;
        }
        *length = 0;
        return TOKEN_ERROR_TYPE;

    default:
        *length = 0;
        return TOKEN_ERROR_TYPE;
    }
}

TokenType recognize_word(const char *input, int *length)
{
    if (*input == '"' || *input == '\'')
    {
        char quote = *input;
        int i = 1;

        while (input[i] != quote && input[i] != '\0')
        {
            i++;
        }

        if (input[i] == '\0')
            return TOKEN_ERROR_TYPE;

        *length = i + 1;

        return TOKEN_WORD;
    }

    int i = 0;

    while (input[i] != '\0' && !isspace((unsigned char)input[i]) && !is_operator(input[i]))
    {
        if (input[i] == '"' || input[i] == '\'')
            return TOKEN_ERROR_TYPE;

        i++;
    }

    *length = i;

    return TOKEN_WORD;
}

int token_list_add(TokenList *list, TokenType type, const char *value)
{
    if (list->count == list->capacity)
    {
        int new_capacity = list->capacity * 2;

        Token *new_tokens = realloc(
            list->tokens,
            new_capacity * sizeof(Token));

        if (new_tokens == NULL)
            return 0;

        list->tokens = new_tokens;
        list->capacity = new_capacity;
    }

    Token *token = &list->tokens[list->count];
    token->type = type;
    token->value = strdup(value);
    if (token->value == NULL)
    {
        return 0;
    }
    list->count++;
    return 1;
}

TokenList *lexer_tokenize(const char *input)
{
    if (input == NULL)
        return NULL;

    TokenList *list = malloc(sizeof(TokenList));
    if (list == NULL)
    {
        return NULL;
    }
    list->count = 0;
    list->capacity = 2;
    list->tokens = malloc(list->capacity * sizeof(Token));

    if (list->tokens == NULL)
    {
        free(list);
        return NULL;
    }

    int i = 0;
    while (input[i] != '\0')
    {
        i = skip_whitespace(input, i);

        if (input[i] == '\0')
            break;

        if (is_operator(input[i]) || (input[i] == '2' && input[i + 1] == '>'))
        {
            int length = 0;
            TokenType type = recognize_operator(&input[i], &length);
            char *value = strndup(&input[i], length);
            if (value == NULL)
            {
                token_list_free(list);
                return NULL;
            }
            if (!token_list_add(list, type, value))
            {
                free(value);
                token_list_free(list);
                return NULL;
            }

            free(value);
            i += length;
        }

        else
        {
            int length = 0;
            TokenType type = recognize_word(&input[i], &length);
            if (type == TOKEN_ERROR_TYPE || length <= 0)
            {
                token_list_free(list);
                return NULL;
            }
            char *value;
            if (input[i] == '"' || input[i] == '\'')
            {
                value = strndup(&input[i + 1], length - 2);
            }
            else
            {
                value = strndup(&input[i], length);
            }
            if (value == NULL)
            {
                token_list_free(list);
                return NULL;
            }
            if (!token_list_add(list, type, value))
            {
                free(value);
                token_list_free(list);
                return NULL;
            }

            free(value);
            i += length;
        }
    }
    if (!token_list_add(list, TOKEN_EOF, ""))
    {
        token_list_free(list);
        return NULL;
    }
    return list;
}