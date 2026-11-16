#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "builtin.h"

int is_builtin(const char *name)
{
    return strcmp(name, "cd") == 0 ||
           strcmp(name, "exit") == 0;
}

int execute_builtin(int argc, char **argv)
{
    if (strcmp(argv[0], "exit") == 0)
    {
        if (argc > 1)
        {
            fprintf(stderr, "exit: too many arguments\n");
            return 1;
        }

        exit(EXIT_SUCCESS);
    }
    if (strcmp(argv[0], "cd") == 0)
    {
        if (argc > 2)
        {
            fprintf(stderr, "cd: too many arguments\n");
            return 1;
        }
        const char *path = argc == 2 ? argv[1] : getenv("Home");
        if (path == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        if (chdir(path) == -1)
        {
            perror("cd");
            return 1;
        }
        return 0;
    }
    return 1;
}