
#include <stdlib.h>
#include "ast.h"

void ast_free(ASTNode *node)
{
    if (node == NULL)
    {
        return;
    }

    switch (node->type)
    {
    case NODE_COMMAND:
    {
        for (int i = 0; i < node->data.command.argc; i++)
        {
            free(node->data.command.argv[i]);
        }

        free(node->data.command.argv);

        for (int i = 0; i < node->data.command.redirection_count; i++)
        {
            free(node->data.command.redirections[i].filename);
        }

        free(node->data.command.redirections);

        break;
    }

    case NODE_PIPELINE:
    {
        for (int i = 0; i < node->data.pipeline.count; i++)
        {
            ast_free(node->data.pipeline.commands[i]);
        }

        free(node->data.pipeline.commands);

        break;
    }

    case NODE_AND:
    case NODE_OR:
    case NODE_SEQUENCE:
    {
        ast_free(node->data.binary.left);
        ast_free(node->data.binary.right);

        break;
    }

    case NODE_BACKGROUND:
    {
        ast_free(node->data.background.command);

        break;
    }
    }

    free(node);
}
