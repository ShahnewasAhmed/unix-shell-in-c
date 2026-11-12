#ifndef LEXER_H
#define LEXER_H

typedef enum
{
    TOKEN_WORD,

    TOKEN_PIPE,      // |
    TOKEN_SEMICOLON, // ;
    TOKEN_AND,       // &&
    TOKEN_OR,        // ||
    TOKEN_AMPERSAND, // &

    TOKEN_INPUT,        // <
    TOKEN_OUTPUT,       // >
    TOKEN_APPEND,       // >>
    TOKEN_ERROR,        // 2>
    TOKEN_ERROR_APPEND, // 2>>

    TOKEN_EOF,
    TOKEN_ERROR_TYPE
} TokenType;

typedef struct
{
    TokenType type;
    char *value;
} Token;

typedef struct
{
    Token *tokens;
    int count;
    int capacity;
} TokenList;

TokenList *lexer_tokenize(const char *input);
void token_list_free(TokenList *list);
int is_operator(char c);
int skip_whitespace(const char *input, int i);

#endif
