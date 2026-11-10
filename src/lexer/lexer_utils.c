#include <ctype.h>

int is_char(char c)
{
    return isalpha((unsigned char)c);
}

int is_digit(char c)
{
    return isdigit((unsigned char)c);
}

int is_operator(char c)
{
    switch (c)
    {
    case '|':
    case ';':
    case '&':
    case '<':
    case '>':
        return 1;

    default:
        return 0;
    }
}

int skip_whitespace(const char *input, int i)
{
    while (isspace((unsigned char)input[i]))
    {
        i++;
    }

    return i;
}