
#include <stdio.h>
#include "ast.h"
#include "lexer.h"

static void print_indent(int depth)
{
    for (int i = 0; i < depth; i++)
    {
        printf("    ");
    }
}

static void print_node(ASTNode *node, int depth)
{
    if (node == NULL)
        return;

    print_indent(depth);

    switch (node->type)
    {
    case NODE_COMMAND:
        printf("COMMAND\n");

        print_indent(depth + 1);
        printf("argc: %d\n", node->data.command.argc);

        print_indent(depth + 1);
        printf("argv:\n");

        for (int i = 0; i < node->data.command.argc; i++)
        {
            print_indent(depth + 2);
            printf("%s\n", node->data.command.argv[i]);
        }

        for (int i = 0; i < node->data.command.redirection_count; i++)
        {
            Redirection *redirection =
                &node->data.command.redirections[i];

            print_indent(depth + 1);

            switch (redirection->type)
            {
            case TOKEN_INPUT:
                printf("INPUT: %s\n", redirection->filename);
                break;

            case TOKEN_OUTPUT:
                printf("OUTPUT: %s\n", redirection->filename);
                break;

            case TOKEN_APPEND:
                printf("APPEND: %s\n", redirection->filename);
                break;

            case TOKEN_ERROR:
                printf("ERROR: %s\n", redirection->filename);
                break;

            case TOKEN_ERROR_APPEND:
                printf("ERROR_APPEND: %s\n", redirection->filename);
                break;

            default:
                break;
            }
        }
        break;

    case NODE_PIPELINE:
        printf("PIPELINE\n");

        for (int i = 0; i < node->data.pipeline.count; i++)
        {
            print_node(node->data.pipeline.commands[i], depth + 1);
        }
        break;

    case NODE_AND:
        printf("AND\n");

        print_node(node->data.binary.left, depth + 1);
        print_node(node->data.binary.right, depth + 1);
        break;

    case NODE_OR:
        printf("OR\n");

        print_node(node->data.binary.left, depth + 1);
        print_node(node->data.binary.right, depth + 1);
        break;

    case NODE_SEQUENCE:
        printf("SEQUENCE\n");

        print_node(node->data.binary.left, depth + 1);
        print_node(node->data.binary.right, depth + 1);
        break;

    case NODE_BACKGROUND:
        printf("BACKGROUND\n");

        print_node(node->data.background.command, depth + 1);
        break;
    }
}

void ast_print(ASTNode *root)
{
    if (root == NULL)
    {
        printf("Empty AST\n");
        return;
    }

    print_node(root, 0);
}
