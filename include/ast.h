#ifndef AST_H
#define AST_H
#include "lexer.h"

typedef enum
{
    NODE_COMMAND,
    NODE_PIPELINE,
    NODE_AND,
    NODE_OR,
    NODE_SEQUENCE,
    NODE_BACKGROUND
} NodeType;

typedef struct
{
    TokenType type;
    char *filename;
} Redirection;

typedef struct ASTNode ASTNode;

struct ASTNode
{

    NodeType type;
    union
    {
        struct
        {
            char **argv;
            int argc;

            Redirection *redirections;
            int redirection_count;
        } command;

        struct
        {
            ASTNode **commands;
            int count;
        } pipeline;

        struct
        {
            ASTNode *left;
            ASTNode *right;
        } binary;

        struct
        {
            ASTNode *command;
        } background;

    } data;
};

void ast_free(ASTNode *node);
void ast_print(ASTNode *root);

#endif