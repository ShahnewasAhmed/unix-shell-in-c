#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include "builtin.h"

int is_builtin(const char *name)
{

    return strcmp(name, "cd") == 0 ||
           strcmp(name, "exit") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "export") == 0 ||
           strcmp(name, "unset") == 0;
}

static int is_valid_variable_name(const char *name)
{

    if (name == NULL || name[0] == '\0')
    {
        return 0;
    }
    if (!isalpha((unsigned char)name[0]) && name[0] != '_')
    {
        return 0;
    }
    for (int i = 1; name[i] != '\0'; i++)
    {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_')
        {
            return 0;
        }
    }
    return 1;
}

int execute_builtin(int argc, char **argv)
{
    if (strcmp(argv[0], "export") == 0)
    {
        if (argc < 2)
        {
            fprintf(stderr, "export: too less arguments\n");
            return 1;
        }
        for (int i = 1; i < argc; i++)
        {
            char *equals = strchr(argv[i], '=');
            if (equals == NULL)
            {
                fprintf(stderr, "export: invalid assignment: %s\n", argv[i]);
                return 1;
            }
            size_t name_length = equals - argv[i];
            char *name = malloc(name_length + 1);
            if (name == NULL)
            {
                perror("export");
                return 1;
            }
            memcpy(name, argv[i], name_length);
            name[name_length] = '\0';
            const char *value = equals + 1;
            if (!is_valid_variable_name(name))
            {
                fprintf(stderr, "export: invalid variable name: %s\n", name);
                free(name);
                return 1;
            }
            if (setenv(name, value, 1) == -1)
            {
                perror("export");
                free(name);
                return 1;
            }
            free(name);
        }
        return 0;
    }
    if (strcmp(argv[0], "unset") == 0)
    {
        if (argc < 2)
        {
            fprintf(stderr, "unset: too less arguments\n");
            return 1;
        }
        for (int i = 1; i < argc; i++)
        {
            if (!is_valid_variable_name(argv[i]))
            {
                fprintf(stderr, "export: invalid variable name: %s\n", argv[i]);
                return 1;
            }
            if (unsetenv(argv[i]) == -1)
            {
                perror("unset");
                return 1;
            }
        }
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0)
    {
        if (argc > 1)
        {
            fprintf(stderr, "pwd: too many arguments\n");
            return 1;
        }
        char *cwd = getcwd(NULL, 0);
        if (cwd == NULL)
        {
            perror("cwd");
            return 1;
        }
        printf("%s\n", cwd);
        free(cwd);
        return 0;
    }
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