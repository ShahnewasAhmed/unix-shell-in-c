#ifndef PARSE_H
#define PARSE_H

#include "ast.h"
#include "lexer.h"

ASTNode *parser_parse(TokenList *tokens);

ASTNode *parse_list(TokenList *tokens, int *position);
ASTNode *parse_and_or(TokenList *tokens, int *position);
ASTNode *parse_pipeline(TokenList *tokens, int *position);
ASTNode *parse_command(TokenList *tokens, int *position);

#endif