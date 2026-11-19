#include <stdlib.h>
#include <string.h>
#include "parse.h"

ASTNode *parse_list(TokenList *tokens, int *position)
{
    ASTNode *left = parse_and_or(tokens, position);

    if (left == NULL)
    {
        return NULL;
    }

    while (tokens->tokens[*position].type == TOKEN_SEMICOLON || tokens->tokens[*position].type == TOKEN_AMPERSAND)
    {
        TokenType operator = tokens->tokens[*position].type;
        if (operator == TOKEN_AMPERSAND)
        {
            ASTNode *background = malloc(sizeof(ASTNode));

            if (background == NULL)
            {
                ast_free(left);
                return NULL;
            }

            background->type = NODE_BACKGROUND;
            background->data.background.command = left;

            left = background;
        }

        (*position)++;
        if (tokens->tokens[*position].type == TOKEN_EOF)
        {
            break;
        }

        ASTNode *next = parse_and_or(tokens, position);
        if (next == NULL)
        {
            ast_free(left);
            return NULL;
        }

        ASTNode *node = malloc(sizeof(ASTNode));
        if (node == NULL)
        {
            ast_free(left);
            ast_free(next);
            return NULL;
        }

        node->type = NODE_SEQUENCE;
        node->data.binary.left = left;
        node->data.binary.right = next;
        left = node;
    }

    return left;
}

ASTNode *parse_and_or(TokenList *tokens, int *position)
{
    ASTNode *left = parse_pipeline(tokens, position);
    if (left == NULL)
    {
        return NULL;
    }

    while ((tokens->tokens[*position].type == TOKEN_AND) || (tokens->tokens[*position].type == TOKEN_OR))
    {
        TokenType operator = tokens->tokens[*position].type;

        (*position)++;
        ASTNode *next = parse_pipeline(tokens, position);
        if (next == NULL)
        {
            ast_free(left);
            return NULL;
        }

        ASTNode *node = malloc(sizeof(ASTNode));
        if (node == NULL)
        {
            ast_free(left);
            ast_free(next);
            return NULL;
        }

        node->type = (operator == TOKEN_AND) ? NODE_AND : NODE_OR;
        node->data.binary.left = left;
        node->data.binary.right = next;
        left = node;
    }

    return left;
}

ASTNode *parse_pipeline(TokenList *tokens, int *position)
{
    ASTNode *first = parse_command(tokens, position);
    if (first == NULL)
    {
        return NULL;
    }
    if (tokens->tokens[*position].type != TOKEN_PIPE)
    {
        return first;
    }

    ASTNode *node = malloc(sizeof(ASTNode));
    if (node == NULL)
    {
        ast_free(first);
        return NULL;
    }
    node->type = NODE_PIPELINE;
    node->data.pipeline.commands = malloc(sizeof(ASTNode *));
    if (node->data.pipeline.commands == NULL)
    {
        ast_free(first);
        free(node);
        return NULL;
    }
    node->data.pipeline.commands[0] = first;
    node->data.pipeline.count = 1;

    while (tokens->tokens[*position].type == TOKEN_PIPE)
    {
        (*position)++;
        ASTNode *next = parse_command(tokens, position);
        if (next == NULL)
        {
            ast_free(node);
            return NULL;
        }

        ASTNode **temp = realloc(node->data.pipeline.commands, sizeof(ASTNode *) * (node->data.pipeline.count + 1));
        if (temp == NULL)
        {
            ast_free(next);
            ast_free(node);
            return NULL;
        }
        node->data.pipeline.commands = temp;
        node->data.pipeline.commands[node->data.pipeline.count] = next;
        node->data.pipeline.count++;
    }

    return node;
}

ASTNode *parse_command(TokenList *tokens, int *position)
{
    ASTNode *node = malloc(sizeof(ASTNode));

    if (node == NULL)
    {
        return NULL;
    }
    node->type = NODE_COMMAND;
    node->data.command.argv = NULL;
    node->data.command.argc = 0;
    node->data.command.redirections = NULL;
    node->data.command.redirection_count = 0;

    while (1)
    {
        TokenType type = tokens->tokens[*position].type;

        if (type == TOKEN_PIPE ||
            type == TOKEN_AND ||
            type == TOKEN_OR ||
            type == TOKEN_SEMICOLON ||
            type == TOKEN_AMPERSAND ||
            type == TOKEN_EOF)
        {
            break;
        }

        if (type == TOKEN_WORD)
        {
            char **temp = realloc(node->data.command.argv, (sizeof(char *) * (node->data.command.argc + 2)));
            if (temp == NULL)
            {
                ast_free(node);
                return NULL;
            }
            node->data.command.argv = temp;
            node->data.command.argv[node->data.command.argc] = strdup(tokens->tokens[*position].value);
            if (node->data.command.argv[node->data.command.argc] == NULL)
            {
                ast_free(node);
                return NULL;
            }
            node->data.command.argc++;
            node->data.command.argv[node->data.command.argc] = NULL;
            (*position)++;
        }
        else if (type == TOKEN_INPUT ||
                 type == TOKEN_OUTPUT ||
                 type == TOKEN_APPEND ||
                 type == TOKEN_ERROR ||
                 type == TOKEN_ERROR_APPEND)
        {
            (*position)++;
            if (tokens->tokens[*position].type != TOKEN_WORD)
            {
                ast_free(node);
                return NULL;
            }
            Redirection *temp = realloc(node->data.command.redirections, sizeof(Redirection) * (node->data.command.redirection_count + 1));
            if (temp == NULL)
            {
                ast_free(node);
                return NULL;
            }
            node->data.command.redirections = temp;
            int index = node->data.command.redirection_count;
            node->data.command.redirections[index].type = type;
            node->data.command.redirections[index].filename = strdup(tokens->tokens[*position].value);
            if (node->data.command.redirections[index].filename == NULL)
            {
                ast_free(node);
                return NULL;
            }

            node->data.command.redirection_count++;

            (*position)++;
        }
        else
        {
            ast_free(node);
            return NULL;
        }
    }
    if (node->data.command.argc == 0)
    {
        ast_free(node);
        return NULL;
    }

    return node;
}

ASTNode *parser_parse(TokenList *tokens)
{
    if (tokens == NULL)
    {
        return NULL;
    }

    int position = 0;
    ASTNode *root = parse_list(tokens, &position);
    if (root == NULL)
    {
        return NULL;
    }

    if (tokens->tokens[position].type != TOKEN_EOF)
    {
        ast_free(root);
        return NULL;
    }

    return root;
}