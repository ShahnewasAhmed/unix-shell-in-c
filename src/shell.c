#include <stdio.h>
#include <stdlib.h>

void shell_loop(void)
{
    char *line = NULL;
    size_t capacity = 0;

    while (1)
    {
        printf("CSH> ");

        fflush(stdout);

        ssize_t length = getline(&line, &capacity, stdin);

        if (length == -1)
        {
            printf("\n");
            break;
        }
        if (length == 1 && line[0] == '\n')
        {
            continue;
        }

        printf("You entered: %s", line);
    }
    free(line);
}